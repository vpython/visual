#ifndef VPYTHON_WIN32_DISPLAY_HPP
#define VPYTHON_WIN32_DISPLAY_HPP

#include "display_kernel.hpp"
#include "util/thread.hpp"
#include <map>
#include <string>

namespace cvisual {

class display : public display_kernel
{
 public:
	display();
	virtual ~display();

	// Called by the gui_main class below (or render_manager as its agent)
	void create();
	void destroy();
	void paint();
	void swap() { gl_swap_buffers(); }

	// Tells the application where it can find its data.
	// Win32 doesn't use this information.
	static void set_dataroot( const std::wstring& ) {};

	// Implements key display_kernel virtual methods
	virtual void activate( bool active );
	virtual EXTENSION_FUNCTION getProcAddress( const char* name );
	
 private:
	friend class font;
	static LRESULT CALLBACK
		dispatch_messages( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static display* current;

 	HWND widget_handle;
 	HDC dev_context;
 	HGLRC gl_context;

 	bool window_visible;

	static void register_win32_class();
	static WNDCLASS win32_class;

	// Procedures used to process messages.
	LRESULT on_showwindow( WPARAM, LPARAM);
	LRESULT on_mouse( WPARAM, LPARAM);
	LRESULT on_paint( WPARAM, LPARAM );
	LRESULT on_close( WPARAM, LPARAM );
	LRESULT on_destroy( WPARAM, LPARAM );
	LRESULT on_getminmaxinfo( WPARAM, LPARAM);
	LRESULT on_keyUp( UINT, WPARAM, LPARAM);
	LRESULT on_keyDown( UINT, WPARAM, LPARAM);
	LRESULT on_keyChar( UINT, WPARAM, LPARAM);
	LRESULT on_size( WPARAM, LPARAM);
	LRESULT on_move( WPARAM, LPARAM);
	LRESULT on_activate( WPARAM, LPARAM );
	
	void update_size();
	
	// Functions to manipulate the OpenGL context
	void gl_begin();
	void gl_end();
	void gl_swap_buffers();

};

class gui_main
{
 private:	
	// Components of the startup sequence.
	static void init_thread(void);

	gui_main();	//< This is the only nonstatic member function that doesn't run in the gui thread!
	void run();
	void poll();

	static gui_main* self;
	
	DWORD gui_thread;
	mutex init_lock;
	condition initialized;
	
	static LRESULT CALLBACK threadMessage( int, WPARAM, LPARAM );

	static VOID CALLBACK timer_callback( PVOID, BOOLEAN );
	
 public:
	// Calls the given function in the GUI thread.
	static void call_in_gui_thread( const boost::function< void() >& f );
	
	// This signal is invoked when the user closes the program (closes a display
	// with display.exit = True).
	// wrap_display_kernel() connects a signal handler that forces Python to
	// exit.
	static boost::signal<void()> on_shutdown;
};

} // !namespace cvisual

#endif /*VPYTHON_WIN32_DISPLAY_HPP*/
