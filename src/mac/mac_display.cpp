// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2003, 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "vpython-config.h"
#include "mac/display.hpp"
#include "util/render_manager.hpp"
#include "util/errors.hpp"
#include "python/gil.hpp"

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
using boost::thread;
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

namespace cvisual {

static bool modBit (int mask, int bit)
{
	return (mask & (1 << bit));
}

static std::set<display*> widgets;

// The first OpenGL Rendering Context, used to share displaylists.
static AGLContext root_glrc = NULL;

void
init_platform() // called just once
{
	ProcessSerialNumber psn;
	CFDictionaryRef		app;
	const void *		backKey;
	const void *		background;
	int					err;

	// If we were invoked from python rather than pythonw, change into a GUI app
	GetCurrentProcess(&psn);
	app = ProcessInformationCopyDictionary(&psn, kProcessDictionaryIncludeAllInformationMask);
	backKey = CFStringCreateWithCString(NULL, "LSBackgroundOnly", kCFStringEncodingASCII);
	background = CFDictionaryGetValue(app, backKey);
	if (background == kCFBooleanTrue) {
		TransformProcessType(&psn, kProcessTransformToForegroundApplication);
	}
	SetFrontProcess(&psn);
}

/*
Deprecated Carbon elements (which means doubly deprecated, since Carbon itself has little support):
In display::initWindow, the routines GetMainDevice and aglSetDrawable
*/

/**************************** display methods ************************************/
display* display::current = 0;

display::display()
 : window_visible(false),
   window(0),
   gl_context(0),
   keyModState(0)
{
}

display::~display()
{
}

void display::paint() {
	if (!window_visible) return;

	gl_begin();

	{
		python::gil_lock gil;
		if (cursor.visible != cursor.last_visible) {
			cursor.last_visible = cursor.visible;
			if (cursor.visible) {
				ShowCursor();
			} else {
				HideCursor();
			}
		}
		render_scene();
	}

	gl_end();
}

void
display::gl_begin()
{
	aglSetCurrentContext(gl_context);
	current = this;
}

void
display::gl_end()
{
	aglSetCurrentContext(NULL);
	current = 0;
}

void
display::gl_swap_buffers()
{
	if (!window_visible) return;

	gl_begin();
	aglSwapBuffers(gl_context);
	glFinish();	// Ensure rendering completes here (without the GIL) rather than at the next paint (with the GIL)
	gl_end();
}

void
display::create()
{
	python::gil_lock gil;  // protect statics like widgets, the shared display list context
	widgets.insert(this);
	if (!initWindow(title, window_x, window_y, window_width, window_height)) {
		std::ostringstream msg;
		msg << "Could not create the window!\n";
		VPYTHON_CRITICAL_ERROR( msg.str());
		std::exit(1);
	}
}

/*
// Original comment:
 	// Do not delete contexts directly, as there can be
	// nasty interactions with the event loop thread.
	// Use the destroy_context function instead.
void destroy_context (glContext * cx)
{
	(static_cast<aglContext *>(cx))->cleanup();
}
*/

void
display::destroy()
{
	DisposeWindow(window);
	window = 0;
	user_close = false;
}

void
display::activate(bool active) { // This seems not to be called on the Mac
	if (active) {
		gui_main::call_in_gui_thread( boost::bind( &display::create, this ) );
		//SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
	} else {
		gui_main::call_in_gui_thread( boost::bind( &display::destroy, this ) );
	}
}

int
display::getShiftKey()
{
	return modBit(keyModState, shiftKeyBit);
}

int
display::getAltKey()
{
	return modBit(keyModState, optionKeyBit);
}

int
display::getCtrlKey()
{
	// For Mac, treat command key as control also
	return modBit(keyModState, cmdKeyBit) || modBit(keyModState, controlKeyBit);
}

void
display::on_destroy()
{
	if ((gl_context == root_glrc) || !gl_context) {
		return;
	}
	aglDestroyContext(gl_context);
	gl_context = NULL;
	widgets.erase(this);
	report_closed();
}

OSStatus
display::vpWindowHandler (EventHandlerCallRef target, EventRef event)
{
	UInt32	kind;

	kind = GetEventKind(event);
	if (kind == kEventWindowBoundsChanged) { // changed size or position of window
		python::gil_lock gil;
		update_size();
		// Tell OpenGL about it
		aglUpdateContext(gl_context);
	} else if (kind == kEventWindowClose) { // user hit close box
		python::gil_lock gil;
		user_close = true;
	} else if (kind == kEventWindowClosed) {
		python::gil_lock gil;
		// window has been closed by user or by setting display.visible = False
		// Hugh Fisher said, "Safest way to destroy a window is by generating ESC key event,
		// which will be passed on and interpreted as close request." But we're not doing this.
		//keys.push("escape");
		on_destroy(); // window has been killed; clean up
		if (exit && user_close)
			QuitApplicationEventLoop();
		return noErr;
	}
	return eventNotHandledErr;
}

OSStatus
display::vpKeyboardHandler (EventHandlerCallRef target, EventRef event)
{
	UInt32	kind;
	char	key[2];
	UInt32	code;
	std::string keyStr;

	kind = GetEventKind(event);
	if (kind == kEventRawKeyDown || kind == kEventRawKeyRepeat) {
		GetEventParameter(event, kEventParamKeyMacCharCodes,
						  typeChar, NULL,
						  sizeof(char), NULL,
						  &(key[0]));
		key[1] = 0;
		if (isprint(key[0])) {
			// Easy
			keys.push(std::string(key));
			return noErr;
		} else {
			keyStr = std::string("");
			GetEventParameter(event, kEventParamKeyModifiers,
							  typeUInt32, NULL,
							  sizeof(keyModState), NULL,
							  &keyModState);
			GetEventParameter(event, kEventParamKeyCode,
							  typeUInt32, NULL,
							  sizeof(code), NULL,
							  &code);
			if (this->getShiftKey())
				keyStr += "shift+";
			if (this->getCtrlKey())
				keyStr += "ctrl+";
			if (this->getAltKey())
				keyStr += "alt+";
			// Carbon doco is still in 1984 with no special keys...sigh
			switch (code) {
				// Apple really do number the F keys like this
				case 122:
					keyStr += "f1";
					break;
				case 120:
					keyStr += "f2";
					break;
				case 99:
					keyStr += "f3";
					break;
				case 118:
					keyStr += "f4";
					break;
				case 96:
					keyStr += "f5";
					break;
				case 97:
					keyStr += "f6";
					break;
				case 98:
					keyStr += "f7";
					break;
				case 100:
					keyStr += "f8";
					break;
				// F9-F12 are reserved by Apple for various purposes
				case 116:
					keyStr += "page up";
					break;
				case 121:
					keyStr += "page down";
					break;
				case 119:
					keyStr += "end";
					break;
				case 115:
					keyStr += "home";
					break;
				case 123:
					keyStr += "left";
					break;
				case 126:
					keyStr += "up";
					break;
				case 124:
					keyStr += "right";
					break;
				case 125:
					keyStr += "down";
					break;
				case 117:
					keyStr += "delete";
					break;
				case 51:
					keyStr += "backspace";
					break;
				case 48:
					keyStr += "\t";
					break;
				case 36:
					keyStr += "\n";
					break;
				case 53:
					if (exit) {
						python::gil_lock gil;
						on_destroy();
						QuitApplicationEventLoop();
					}
					keyStr += "escape";
					break;
				default:
					keyStr += "unknown";
					break;
			}
			keys.push(std::string(keyStr));
			return noErr;
		}
	} else if (kind == kEventRawKeyModifiersChanged) {
		GetEventParameter(event, kEventParamKeyModifiers,
						  typeUInt32, NULL,
						  sizeof(keyModState), NULL,
						  &keyModState);
		return noErr;
	} else
		return eventNotHandledErr;
}

OSStatus
display::vpMouseHandler (EventHandlerCallRef target, EventRef event)
{
	WindowPartCode	part;
	WindowRef		win;
	Point			pt;
	bool buttons[3]; // left, right, middle buttons
	bool shiftState[4]; // shift, ctrl, option, command key down

	python::gil_lock gil;

	GetEventParameter(event, kEventParamMouseLocation,
					  typeQDPoint, NULL,
					  sizeof(pt), NULL,
					  &pt);
	if (!fullscreen) { // If not full screen, check if mouse event occurred within our content area
		part = FindWindow(pt, &win);
		if (win != window || part != inContent) {
			// Title bar, close box, ... pass to OS
			return eventNotHandledErr;
		}
		// And now get mouse coordinates in the content area
		GetEventParameter(event, kEventParamWindowMouseLocation,
						  typeQDPoint, NULL,
						  sizeof(pt), NULL,
						  &pt);
	}

	// Get any modifier keys
	GetEventParameter(event, kEventParamKeyModifiers,
					  typeUInt32, NULL,
					  sizeof(keyModState), NULL,
					  &keyModState);

	shiftState[0] = modBit(keyModState, shiftKeyBit);
	shiftState[1] = modBit(keyModState, controlKeyBit);
	shiftState[2] = modBit(keyModState, optionKeyBit);
	shiftState[3] = modBit(keyModState, cmdKeyBit);

	unsigned btnMask = GetCurrentEventButtonState();
	for(int b=0; b<3; b++) buttons[b] = btnMask & (1<<b);

	// On a one-button Mac mouse (buttons[0]), command and option indicate right and middle buttons: 
	if (shiftState[3]) { 
		buttons[1] = buttons[0];
		buttons[0] = false;
		buttons[3] = false;
	} else if (shiftState[2]) { 
		buttons[2] = buttons[0];
		buttons[0] = false; 
		buttons[1] = false; 
	}

	mouse.report_mouse_state( 3, buttons, pt.h, pt.v-yadjust, 4, shiftState, false );

	// clicking in content area should bring this window forward.
	UInt32 kind = GetEventKind(event);
	if (kind == kEventMouseDown && !fullscreen && !IsWindowActive(window)) {
		return 1; // let standard handler bring window forward
	}
	return noErr;
}

OSStatus
display::vpEventHandler (EventHandlerCallRef target, EventRef event, void * data)
{
	UInt32	evtClass;
	display * thiswindow;

	thiswindow = (display *)data;

	evtClass = GetEventClass(event);

	switch (evtClass) {
		case kEventClassWindow:
			return thiswindow->vpWindowHandler(target, event);
			break;
		case kEventClassKeyboard:
			return thiswindow->vpKeyboardHandler(target, event);
			break;
		case kEventClassMouse:
			if (!thiswindow->vpMouseHandler(target, event)) return noErr; // mouse has been handled
			break;
		default:
			break;
	}
	// Default is let OS do it
	return eventNotHandledErr;
}

void
display::update_size()
{
	Rect	bounds, drawing; // outer bounds of window, actual drawing region

	GetWindowBounds(window, kWindowStructureRgn, &bounds);
	GetWindowBounds(window, kWindowContentRgn, &drawing);
	report_window_resize( bounds.left, bounds.top-yadjust, bounds.right-bounds.left, bounds.bottom-bounds.top );
	report_view_resize(	drawing.right-drawing.left, drawing.bottom-drawing.top );
}

bool
display::initWindow(std::string title, int x, int y, int width, int height)
// x, y, width, height refer to the outer bounds of the window, including the title bar
// but the Mac CreateNewWindow has parameters in terms of the content area only.
{
	GDHandle    dev;
	OSStatus	err;
	Rect		drawing;
	int			idx;
	AGLPixelFormat	fmt;
	GLint		attrList[] = {
				AGL_RGBA, AGL_DOUBLEBUFFER,
				AGL_DEPTH_SIZE, 32,
				AGL_ALL_RENDERERS, AGL_ACCELERATED,
				AGL_NO_RECOVERY,
				AGL_NONE,		// Expansion
				AGL_NONE,		// Expansion
				AGL_NONE
				};
	EventHandlerUPP	upp;
	EventHandlerRef	discard;
	EventTypeSpec	handled[] = {
		{ kEventClassWindow, kEventWindowClose },
		{ kEventClassWindow, kEventWindowClosed },
		{ kEventClassWindow, kEventWindowBoundsChanged },
		{ kEventClassKeyboard, kEventRawKeyDown },
		{ kEventClassKeyboard, kEventRawKeyRepeat },
		{ kEventClassKeyboard, kEventRawKeyModifiersChanged },
		{ kEventClassMouse, kEventMouseDown },
		{ kEventClassMouse, kEventMouseUp },
		{ kEventClassMouse, kEventMouseMoved },
		{ kEventClassMouse, kEventMouseDragged },
		{ 0, 0 }
	};

	if (window) { // window already exists; make active and bring to the front
		SelectWindow(window);
		return true;
	}

	// Window
	// drawing refers only to the content region of the window, so must adjust
	// for the fact that the VPython API for window size and placement is in
	// terms of the outer bounds of the window

	dev = GetMainDevice();
	if (fullscreen) {
		width = (**dev).gdRect.right-(**dev).gdRect.left;
		height = (**dev).gdRect.bottom-(**dev).gdRect.top;
		SetRect(&drawing, 0, 0, width, height);
	} else {
		yadjust = GetMBarHeight();
		if (stereo_mode == PASSIVE_STEREO || stereo_mode == CROSSEYED_STEREO)
			width *= 2;
		SetRect(&drawing, x, y+2*yadjust, x + width, y+yadjust + height);
	}
	err = CreateNewWindow(kDocumentWindowClass, kWindowStandardDocumentAttributes, &drawing, &window);
	if (err != noErr)
		return false;
	window_x = x;
	window_y = y;
	window_width = width;
	window_height = height;

	SetWindowTitleWithCFString(window, CFStringCreateWithCString(NULL, title.c_str(), kCFStringEncodingASCII));

	// GL context
	idx = 0;
	while (attrList[idx] != AGL_NONE) idx ++;
	// Special
	if (fullscreen) {
		attrList[idx] =	AGL_FULLSCREEN;
		idx ++;
	}

	fmt = aglChoosePixelFormat(&dev, 1, (const GLint *)attrList);

	if (fmt == NULL || aglGetError() != AGL_NO_ERROR)
		return false;

	if (!root_glrc) {
		root_glrc = aglCreateContext(fmt, NULL);
		if (root_glrc == NULL)
			return false;
	}
	gl_context = aglCreateContext(fmt, root_glrc);
	if (gl_context == NULL)
		return false;

	// Fullscreen on Mac
	if (fullscreen) {
		aglEnable(gl_context, AGL_FS_CAPTURE_SINGLE);
		aglSetFullScreen(gl_context, 0, 0, 0, 0);
	} else {
		aglSetDrawable(gl_context, GetWindowPort(window));
	}
	aglDestroyPixelFormat(fmt);

	// Set up event handling
	InstallStandardEventHandler(GetWindowEventTarget(window));
	upp = NewEventHandlerUPP(vpEventHandler);
	idx = 0;
	while (handled[idx].eventClass != 0 || handled[idx].eventKind != 0) idx ++;
	// Events within window
	InstallEventHandler(GetWindowEventTarget(window),
						upp,
						idx, handled,
						this, &discard);

	// This handles menu bar quit and all events in fullscreen mode
	if (fullscreen)
		InstallEventHandler(GetApplicationEventTarget(),
						upp,
						idx, handled,
						this, &discard);

	DisposeEventHandlerUPP(upp);
	// Make visible
	if (!fullscreen) ShowWindow(window);
	window_visible = true;
	update_size();

	return true;
}

#include <dlfcn.h>
display::EXTENSION_FUNCTION
display::getProcAddress(const char* name) {
	void *lib = dlopen( (const char *)0L, RTLD_LAZY | RTLD_GLOBAL );
	void *sym = dlsym( lib, name );
	dlclose( lib );
	//printf("%s: %p\n", name, sym);
	return (EXTENSION_FUNCTION)sym;
	//return (EXTENSION_FUNCTION)::wglGetProcAddress( name ); // Windows
	//return (EXTENSION_FUNCTION)Gdk::GL::get_proc_address( name ); // GTK2
}

/******************** gui_main implementation **********************/

gui_main* gui_main::self = 0;  // Protected by python GIL

boost::signal<void()> gui_main::on_shutdown;

gui_main::gui_main()
 : gui_thread(-1)
{
}

void
gui_main::event_loop ()
{
    init_platform();
	// Tell the initializing thread
	{
		lock L(init_lock);
		gui_thread = 0; //just an arbitrary value != -1
		initialized.notify_all();
	}
	poll();
	RunApplicationEventLoop();
	on_shutdown();
}

void
gui_main::init_thread()
{
	if (!self) {
        // if we perform this from the current thread (the python one),
        // the gui thread will _not_ be the Carbon "main" one
		//init_platform();
		
		// We are holding the Python GIL through this process, including the wait!
		// We can't let go because a different Python thread could come along and mess us up (e.g.
		//   think that we are initialized and go on to call PostThreadMessage without a valid idThread)
		self = new gui_main;
		thread gui( boost::bind( &gui_main::event_loop, self ) );
		lock L( self->init_lock );
		while (self->gui_thread == -1)
			self->initialized.wait( L );
	}
}

static void call_boost_function( EventLoopTimerRef, void* f_as_pvoid ) {
	boost::function<void()>* f = reinterpret_cast<boost::function<void()>*>(f_as_pvoid);
	(*f)();
	delete f;
}

EventLoopTimerRef discard;

static void call_in_gui_thread_delayed( double delay, const boost::function< void() >& f) {
	EventLoopTimerUPP upp = NewEventLoopTimerUPP(call_boost_function);
	InstallEventLoopTimer(GetMainEventLoop(), delay, 0, upp, new boost::function<void()>(f), &discard);
}

void
gui_main::call_in_gui_thread( const boost::function< void() >& f )
{
	init_thread();
	// TODO: The delay should be 0.0, but there appears to be some kind of OS bug that
	// occasionally delivers timer events twice if the delay is zero.  This is a workaround.
    call_in_gui_thread_delayed( 0.001, f );
}

void gui_main::poll() {
	// Called in gui thread when it's time to render
	// We don't need the lock here, because displays can't be created or destroyed from Python
	// without a message being processed by the GUI thread.  paint_displays() will pick
	// the lock up as necessary to synchronize access to the actual display contents.
	RemoveEventLoopTimer(discard);

	std::vector<display*> displays( widgets.begin(), widgets.end() );

	double interval = render_manager::paint_displays( displays );

	//std::cout << "poll " << widgets.size() << " " << interval << std::endl;
	call_in_gui_thread_delayed( interval, boost::bind(&gui_main::poll, this) );
}

} // !namespace cvisual
