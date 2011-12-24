// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2003, 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#define _WIN32_WINNT 0x0500		// Requires Windows 2000, for CreateTimerQueueTimer()

#include "win32/display.hpp"
#include "util/render_manager.hpp"
#include "util/errors.hpp"
#include "python/gil.hpp"

#include <windowsx.h>	// For GET_X_LPARAM, GET_Y_LPARAM

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
using boost::thread;
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

/*
How to shut down on Windows (Dave Scherer, 2008/6/22)

There's more than one possible sequence of events leading to shutdown,
and can be more than one window. Remember that the program can close windows,
and that the user closing a window does NOT always mean the program terminates.

Here's a high level description of how it works on Windows:

1. In display::destroy(), ask the OS to close the window. Don't do any cleanup.

2. When an event is received indicating that the USER wants to close the window,
also ask the OS to close the window. Additionally, if this->exit (equivalent to scene.exit = 1),
ask the event loop to shut down.

3. In the event handler for window destruction (which gets called after either of the above cases),
call report_closed() and then clean up the resources used by the window like OpenGL contexts.

4. When the event loop terminates (i.e. returns), call gui_main::on_shutdown(),
which will terminate the program (the actual shutdown logic is in wrap_display_kernel.cpp).

5. If the program closes all its windows programmatically and then terminates itself,
the event loop never terminates; it is killed by the OS when the main thread calls std::exit().

I don't know if this exact design will work on the Mac; it depends somewhat on OS behavior.
But its behavior needs to be followed closely.
The most obvious possible implementation differences required:

 - There might be only a single event for 2 and 3.
   In that case, you need to clean up resources, and request event loop shutdown
   only if this->exit AND this->visible. I believe that check will identify only
   the case where the user is closing the window, as opposed to setting scene.visible = 0.

 - 5 might not work, so the program might hang on shutdown in that situation.
 In fact, a quick test revealed that this was broken on Windows due to a bug in display_kernel,
 which I'll check in a fix to. If there's a problem with this on the Mac,
 talk to me about it since it's not worth my solving if it isn't a problem.
*/

namespace cvisual {

/**************************** Utilities ************************************/
// Extracts and decodes a Win32 error message.
void
win32_write_critical(
	std::string file, int line, std::string func, std::string msg)
{
	DWORD code = GetLastError();
	char* message = 0;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		0, // Ignored parameter
		code, // Error code to be decoded.
		0, // Use the default language
		(char*)&message, // The output buffer to fill the message
		512, // Allocate at least one byte in order to free something below.
		0); // No additional arguments.

	std::ostringstream os;
	os << "VPython ***Win32 Critical Error*** " << file << ":" << line << ": "
		<< func << ": " << msg << ": [" << code << "] " << message;
	write_stderr( os.str() );

	LocalFree(message);
	std::exit(1);
}

// The first OpenGL Rendering Context, used to share displaylists.
static HGLRC root_glrc = 0;
WNDCLASS display::win32_class;

// A lookup-table for the default widget procedure to use to find the actual
// widget that should handle a particular callback message.
static std::map<HWND, display*> widgets;

display* display::current = 0;

display::display()
  : widget_handle(0), dev_context(0), gl_context(0), window_visible(false)
{
}

// This function dispatches incoming messages to the particular message-handler.
LRESULT CALLBACK
display::dispatch_messages( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	python::gil_lock gil;

	display* This = widgets[hwnd];
	if (This == 0)
		return DefWindowProc( hwnd, uMsg, wParam, lParam);

	switch (uMsg) {
		// Handle the cases for the kinds of messages that we are listening for.
		case WM_CLOSE:
			return This->on_close( wParam, lParam);
		case WM_DESTROY:
			return This->on_destroy( wParam, lParam );
		case WM_SIZE:
			return This->on_size( wParam, lParam);
		case WM_MOVE:
			return This->on_move( wParam, lParam);
		case WM_PAINT:
			return This->on_paint( wParam, lParam);
		case WM_SHOWWINDOW:
			return This->on_showwindow( wParam, lParam);
		case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
		case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			return This->on_mouse( wParam, lParam);
		case WM_KEYUP:
			return This->on_keyUp(uMsg, wParam, lParam);
		case WM_KEYDOWN:
			return This->on_keyDown(uMsg, wParam, lParam);
		case WM_CHAR:
			return This->on_keyChar(uMsg, wParam, lParam);
		case WM_GETMINMAXINFO:
			return This->on_getminmaxinfo( wParam, lParam);
		case WM_ACTIVATE:
			return This->on_activate( wParam, lParam );
	}
	// Default window processing for anything else:
	return DefWindowProc( hwnd, uMsg, wParam, lParam);
}

void
display::register_win32_class()
{
	static bool done = false;
	if (done)
		return;
	else {
		std::memset( &win32_class, 0, sizeof(win32_class));

		win32_class.lpszClassName = "vpython_win32_render_surface";
		win32_class.lpfnWndProc = &dispatch_messages;
		win32_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
		win32_class.hInstance = GetModuleHandle(0);
		win32_class.hIcon = LoadIcon( NULL, IDI_APPLICATION );
		win32_class.hCursor = LoadCursor( NULL, IDC_ARROW );
		if (!RegisterClass( &win32_class))
			WIN32_CRITICAL_ERROR("RegisterClass()");
		done = true;
	}
}

LRESULT
display::on_showwindow( WPARAM wParam, LPARAM lParam)
{
	switch (lParam) {
		case 0:	// Opening for the first time.
		case SW_PARENTOPENING: // restart rendering when the window is restored.
			window_visible = true;
			break;
		case SW_PARENTCLOSING:
			// Stop rendering when the window is minimized.
			window_visible = false;
			break;
		default:
			return DefWindowProc( widget_handle, WM_SHOWWINDOW, wParam, lParam);
	}
	return 0;
}

LRESULT
display::on_activate( WPARAM wParam, LPARAM lParam ) {
	if (fullscreen && wParam == WA_INACTIVE )
		ShowWindow( widget_handle, SW_MINIMIZE );
	return DefWindowProc( widget_handle, WM_ACTIVATE, wParam, lParam );
}

LRESULT
display::on_size( WPARAM, LPARAM)
{
	update_size();
	return 0;
}

LRESULT
display::on_move( WPARAM, LPARAM lParam)
{
	update_size();
	return 0;
}

void
display::update_size()
{
	if (!IsIconic( widget_handle )) {
		RECT windowRect, clientSize;
		GetWindowRect( widget_handle, &windowRect );
		GetClientRect( widget_handle, &clientSize );
		POINT clientPos; clientPos.x = clientPos.y = 0;
		ClientToScreen( widget_handle, &clientPos);
		report_window_resize( windowRect.left, windowRect.top, windowRect.right-windowRect.left, windowRect.bottom-windowRect.top );
		report_view_resize(	clientSize.right, clientSize.bottom );
	}
}

LRESULT
display::on_getminmaxinfo( WPARAM, LPARAM lParam)
{
	MINMAXINFO* info = (MINMAXINFO*)lParam;
	// Prevents making the window too small.
	info->ptMinTrackSize.x = 70;
	info->ptMinTrackSize.y = 70;
	return 0;
}

LRESULT
display::on_paint( WPARAM, LPARAM)
{
	// paint(); gl_swap_buffers();  //< Doesn't seem qualitatively better, even at very low frame rates
	ValidateRect( widget_handle, NULL );
	return 0;
}

void display::paint() { // called by render_manager "paint_displays"
	if (!window_visible) return;

	gl_begin();

	{
		python::gil_lock gil;
		if (cursor.visible != cursor.last_visible) {
			cursor.last_visible = cursor.visible;
			// The following code locks the invisible cursor to its window.
			if (!cursor.visible) {
				RECT rcClip;
				GetWindowRect(widget_handle, &rcClip);
				ClipCursor(&rcClip);
			} else {
				ClipCursor(0);
			}
			ShowCursor(cursor.visible);
		}
		render_scene();
	}

	gl_end();
}

LRESULT
display::on_close( WPARAM, LPARAM)
{
	// Happens only when the user closes the window
	VPYTHON_NOTE( "Closing a window from the GUI");
	DestroyWindow(widget_handle);
	if (exit)
		PostQuitMessage(0);
	return 0;
}

LRESULT
display::on_destroy(WPARAM wParam,LPARAM lParam)
{
	// Happens after on_close, and also when a window is destroyed programmatically
	// (e.g. scene.visible = 0)
	report_closed();
	// We can only free the OpenGL context if it isn't the one we are using for display list sharing
	// TODO: Eliminate display list sharing!
	if ( gl_context != root_glrc )
		wglDeleteContext( gl_context );
	ReleaseDC( widget_handle, dev_context);
	widgets.erase( widget_handle );
	return DefWindowProc( widget_handle, WM_DESTROY, wParam, lParam );
}

void
display::gl_begin()
{
	if (!wglMakeCurrent( dev_context, gl_context))
		WIN32_CRITICAL_ERROR( "wglMakeCurrent failed");
	current = this;
}

void
display::gl_end()
{
	wglMakeCurrent( NULL, NULL );
	current = 0;
}


void
display::gl_swap_buffers()
{
	if (!window_visible) return;

	gl_begin();
	SwapBuffers( dev_context);
	glFinish();	// Ensure rendering completes here (without the GIL) rather than at the next paint (with the GIL)
	gl_end();
}

void
display::create()
{
	python::gil_lock gil;  // protect statics like widgets, the shared display list context

	register_win32_class();

	RECT screen;
	SystemParametersInfo( SPI_GETWORKAREA, 0, &screen, 0);
	int style = -1;

	int real_x = static_cast<int>(window_x);
	int real_y = static_cast<int>(window_y);
	int real_width = static_cast<int>(window_width);
	int real_height = static_cast<int>(window_height);

	if (stereo_mode == PASSIVE_STEREO || stereo_mode == CROSSEYED_STEREO)
		real_width *= 2;

	if (fullscreen) {
		real_x = screen.left;
		real_y = screen.top;
		real_width = screen.right - real_x;
		real_height = screen.bottom - real_y;
		style = WS_OVERLAPPED | WS_POPUP | WS_MAXIMIZE;
	}
	else if (real_x < 0 && real_y < 0 || real_x > screen.right || real_y > screen.bottom) {
		real_x = CW_USEDEFAULT;
		real_y = CW_USEDEFAULT;
	}
	else if (real_x < screen.left) {
		real_x = screen.left;
	}
	else if (real_y < screen.top) {
		real_y = screen.top;
	}

	if (real_x + real_width > screen.right)
		real_width = screen.right - real_x;
	if (real_y + real_height > screen.bottom)
		real_height = screen.bottom - real_y;

	if (!fullscreen)
		style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	widget_handle = CreateWindow(
		win32_class.lpszClassName,
		title.c_str(),
		style,
		real_x, real_y,
		real_width, real_height,
		0,
		0, // A unique index to identify this widget by the parent
		GetModuleHandle(0),
		0 // No data passed to the WM_CREATE function.
	);
	widgets[widget_handle] = this;

	dev_context = GetDC(widget_handle);
	if (!dev_context)
		WIN32_CRITICAL_ERROR( "GetDC()");

	DWORD pfdFormat = PFD_DOUBLEBUFFER;
	if (stereo_mode == ACTIVE_STEREO)
		pfdFormat |= PFD_STEREO;

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd
		1,                               // version number
		PFD_DRAW_TO_WINDOW |             // output to screen (not an image)
		PFD_SUPPORT_OPENGL |             // support OpenGL
		pfdFormat,                		 // double buffered, and possibly stereo
		PFD_TYPE_RGBA,                   // RGBA type
		24,                              // 24-bit color depth
		0, 0, 0, 0, 0, 0,                // color bits ignored
		0,                               // no opacity buffer
		0,                               // shift bit ignored
		0,                               // no accumulation buffer
		0, 0, 0, 0,                      // accum bits ignored
		32,                              // 32-bit z-buffer
		0,                               // no stencil buffer
		0,                               // no auxiliary buffer
		PFD_MAIN_PLANE,                  // main layer
		0,                               // reserved
		0, 0, 0                          // layer masks ignored
	};

	int pixelformat = ChoosePixelFormat( dev_context, &pfd);

	DescribePixelFormat( dev_context, pixelformat, sizeof(pfd), &pfd);
	SetPixelFormat( dev_context, pixelformat, &pfd);

	gl_context = wglCreateContext( dev_context);
	if (!gl_context)
		WIN32_CRITICAL_ERROR("wglCreateContext()");

	if (!root_glrc)
		root_glrc = gl_context;
	else
		wglShareLists( root_glrc, gl_context);

	ShowWindow( widget_handle, SW_SHOW);

	update_size();
}

void
display::destroy()
{
	DestroyWindow( widget_handle);
}

display::~display()
{
}

void
display::activate(bool active) {
	if (active) {
		VPYTHON_NOTE( "Opening a window from Python.");
		//SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
		gui_main::call_in_gui_thread( boost::bind( &display::create, this ) );
	} else {
		VPYTHON_NOTE( "Closing a window from Python.");
		gui_main::call_in_gui_thread( boost::bind( &display::destroy, this ) );
	}
}

display::EXTENSION_FUNCTION
display::getProcAddress(const char* name) {
	return (EXTENSION_FUNCTION)::wglGetProcAddress( name );
}

LRESULT
display::on_mouse( WPARAM wParam, LPARAM lParam)
{
	bool buttons[] = { wParam & MK_LBUTTON, wParam & MK_RBUTTON };
	bool shiftState[] = { wParam & MK_SHIFT, wParam & MK_CONTROL, GetKeyState( VK_MENU) < 0 };

	int mouse_x = GET_X_LPARAM(lParam);
	int mouse_y = GET_Y_LPARAM(lParam);
	bool was_locked = mouse.is_mouse_locked();
	int old_x = mouse.get_x(), old_y = mouse.get_y();

	mouse.report_mouse_state( 2, buttons, mouse_x, mouse_y, 3, shiftState, true );

	// This mouse locking code is more complicated but much smoother and the cursor can't escape.
	if ( mouse.is_mouse_locked() != was_locked ) {
		if (mouse.is_mouse_locked()) SetCapture( widget_handle );
		else ReleaseCapture();
		ShowCursor( !mouse.is_mouse_locked() );
	}
	/*
	Correction below by CL Cheung to address a problem encountered when
	trying to use Visual with wxpython (docking and undocking). He says,
	"Both SetCursorPos call will move the mouse to an incorrect coordinate.
	It is because WM_MOVE message is not sending to vPython any more after
	it is reparented. With the change, Now the mouse cursor is
	correctly positioned after dragging, no matter if it has been reparented or not.
	Seems no more crashes now."
	*/
	if (mouse.is_mouse_locked() && ( mouse_x < 20 || mouse_x > view_width - 20 || mouse_y < 20 || mouse_y > view_height - 20 ) ) {
		mouse.report_setcursor( view_width/2, view_height/2 );
		POINT pt;
		pt.x = pt.y = 0;
		ClientToScreen(widget_handle, &pt);
		SetCursorPos( pt.x + view_width/2, pt.y + view_height/2 );
	}
	if (was_locked && !mouse.is_mouse_locked()) {
		mouse.report_setcursor( old_x, old_y );
		POINT pt;
		pt.x = pt.y = 0;
		ClientToScreen(widget_handle, &pt);
		SetCursorPos( pt.x + old_x,  pt.y + old_y );
	}
	return 0;
}

LRESULT
display::on_keyUp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	return 0;
}

LRESULT
display::on_keyDown(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Note that this algorithm will probably fail if the user is using anything
	// other than a US keyboard.
	char *kNameP;
	char kStr[60],fStr[4];

	bool Kshift = (GetKeyState(VK_SHIFT) < 0) ||
		(GetKeyState(VK_CAPITAL) & 1);
	bool Kalt = GetKeyState(VK_MENU) < 0;
	bool Kctrl = GetKeyState(VK_CONTROL) < 0;
	kStr[0] = 0;
	kNameP = NULL;

	switch (wParam) {

	case VK_F1:
	case VK_F2:
	case VK_F3:
	case VK_F4:
	case VK_F5:
	case VK_F6:
	case VK_F7:
	case VK_F8:
	case VK_F9:
	case VK_F10:
	case VK_F11:
	case VK_F12:
		sprintf(fStr,"f%d",wParam-VK_F1+1);
		kNameP = fStr;
		break;

	case VK_PRIOR:
		kNameP = "page up";
		break;

	case VK_NEXT:
		kNameP = "page down";
		break;

	case VK_END:
		kNameP = "end";
		break;

	case VK_HOME:
		kNameP = "home";
		break;

	case VK_LEFT:
		kNameP = "left";
		break;

	case VK_UP:
		kNameP = "up";
		break;

	case VK_RIGHT:
		kNameP = "right";
		break;

	case VK_DOWN:
		kNameP = "down";
		break;

	case VK_SNAPSHOT:
		kNameP = "print screen";
		break;

	case VK_INSERT:
		kNameP = "insert";
		break;

	case VK_DELETE:
		kNameP = "delete";
		break;

	case VK_NUMLOCK:
		kNameP = "numlock";
		break;

	case VK_SCROLL:
		kNameP = "scrlock";
		break;

	} // wParam

	if (kNameP) {
		if (Kctrl) strcat(kStr,"ctrl+");
		if (Kalt) strcat(kStr,"alt+");
		if (Kshift) strcat(kStr,"shift+");
		strcat(kStr,kNameP);
		keys.push( std::string(kStr));
	}

	return 0;
}

LRESULT
display::on_keyChar(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Note that this algorithm will probably fail if the user is using anything
	// other than a US keyboard.
	int fShift,fAlt,fCtrl;
	char *kNameP;
	char kStr[60],wStr[2];

	if ((wParam >= 32) && (wParam <= 126))
	{
		char kk[2];
	    kk[0] = wParam;
	    kk[1] = 0;
	    keys.push( std::string(kk));
	    return 0;
	}

	fShift = (GetKeyState(VK_SHIFT) < 0) ||
		(GetKeyState(VK_CAPITAL) & 1);
	fAlt = GetKeyState(VK_MENU) < 0;
	fCtrl = GetKeyState(VK_CONTROL) < 0;
	kStr[0] = 0;
	kNameP = NULL;

	if (!fCtrl && wParam == VK_RETURN)
		kNameP = "\n";
	else if (!fCtrl && wParam == VK_ESCAPE)
		kNameP = "escape";
	else if (!fCtrl && wParam == VK_BACK)
		kNameP = "backspace";
	else if (!fCtrl && wParam == VK_TAB)
		kNameP = "\t";
	else if ((wParam > 0) && (wParam <= 26)) {
		wStr[0] = wParam-1+'a';
		wStr[1] = 0;
		kNameP = wStr;
	} else if (wParam == 27)
		kNameP = "[";
	else if (wParam == 28)
		kNameP = "\\";
	else if (wParam == 29)
		kNameP = "]";
	else if (wParam == 30)
		kNameP = "^";
	else if (wParam == 31)
		kNameP = "_";

	if (kNameP) {
		if (fCtrl) strcat(kStr,"ctrl+");
		if (fAlt) strcat(kStr,"alt+");
		if (fShift) strcat(kStr,"shift+");
		strcat(kStr,kNameP);
		if (strcmp(kStr,"escape") == 0)
		{
			// Allow the user to delete a fullscreen window this way
			destroy();
			if (exit)
				PostQuitMessage(0);
			return 0;
		}
		keys.push( std::string(kStr));
	}

	return 0;
}

/******************************* gui_main implementatin **********************/

gui_main* gui_main::self = 0;  // Protected by python GIL

boost::signal<void()> gui_main::on_shutdown;

const int CALL_MESSAGE = WM_USER;

gui_main::gui_main()
 : gui_thread(-1)
{
}

LRESULT
gui_main::threadMessage( int code, WPARAM wParam, LPARAM lParam ) {
	if (wParam == PM_REMOVE) {
		MSG& message = *(MSG*)lParam;

		if (message.hwnd ==0 && message.message == CALL_MESSAGE ) {
			boost::function<void()>* f = (boost::function<void()>*)message.wParam;
			(*f)();
			delete f;
		}
	}
	return CallNextHookEx( NULL, code, wParam, lParam );
}

#define TIMER_RESOLUTION 5
void __cdecl end_period() { timeEndPeriod(TIMER_RESOLUTION); }

void
gui_main::run()
{
 	MSG message;

	// Create a message queue and hook thread messages (we can't just check for them in the loop
	// below, because Windows runs modal message loops e.g. when resizing a window)
	SetWindowsHookEx(WH_GETMESSAGE, &gui_main::threadMessage, NULL, GetCurrentThreadId());
	PeekMessage(&message, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	// Tell the initializing thread
	{
		lock L(init_lock);
		gui_thread = GetCurrentThreadId();
		initialized.notify_all();
	}

	timeBeginPeriod(TIMER_RESOLUTION);
	atexit( end_period );

	poll();

	// Enter the message loop; runs about 60 times per second
	while (GetMessage(&message, 0, 0, 0) > 0) {
		TranslateMessage( &message);
		DispatchMessage( &message);
	}

	// We normally exit the message queue because PostQuitMessage() has been called
	VPYTHON_NOTE( "WM_QUIT (or message queue error) received");
	on_shutdown(); // Tries to kill Python
}

void
gui_main::init_thread(void)
{
	if (!self) {
		// We are holding the Python GIL through this process, including the wait!
		// We can't let go because a different Python thread could come along and mess us up (e.g.
		//   think that we are initialized and go on to call PostThreadMessage without a valid idThread)
		self = new gui_main;
		thread gui( boost::bind( &gui_main::run, self ) );
		lock L( self->init_lock );
		while (self->gui_thread == -1)
			self->initialized.wait( L );
	}
}

void
gui_main::call_in_gui_thread( const boost::function< void() >& f )
{
	init_thread();
	PostThreadMessage( self->gui_thread, CALL_MESSAGE, (WPARAM)(new boost::function<void()>(f)), 0);
}

HANDLE timer_handle; // For some reason there are link errors if this is in display.hpp

VOID CALLBACK gui_main::timer_callback( PVOID, BOOLEAN ) {
	// Called in high-priority timer thread when it's time to render
	DeleteTimerQueueTimer(NULL, timer_handle, NULL); // There is a memory leak without this
	self->call_in_gui_thread( boost::bind( &gui_main::poll, self ) );
}

void gui_main::poll() {
	// Called in gui thread when it's time to render
	// We don't need the lock here, because displays can't be created or destroyed from Python
	// without a message being processed by the GUI thread.  paint_displays() will pick
	// the lock up as necessary to synchronize access to the actual display contents.

	std::vector<display*> displays;

	for(std::map<HWND, display*>::iterator i = widgets.begin(); i != widgets.end(); ++i )
		if (i->second)
			displays.push_back( i->second );

	// Setting the 2nd, optional argument for paint_displays to true made multiwindow programs work for one user.
	// Scherer notes, "That isn't acceptable to ship (it is TERRIBLE for performance on many drivers)."
	int interval = int( 1000. * render_manager::paint_displays( displays ) );
	CreateTimerQueueTimer( &timer_handle, NULL, &timer_callback,
			NULL, interval, 0, WT_EXECUTEINTIMERTHREAD );
}

} // !namespace cvisual;
