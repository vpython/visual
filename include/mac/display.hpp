#ifndef VPYTHON_MAC_DISPLAY_HPP
#define VPYTHON_MAC_DISPLAY_HPP

#include "display_kernel.hpp"
#include "util/atomic_queue.hpp"
#include "mouseobject.hpp"

#include <map>
#include <string>

#include <boost/scoped_ptr.hpp>

// Apparently check is defined in Carbon.h, so we have to include after the boost include
#include <Carbon/Carbon.h>
#include <AGL/agl.h>

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
	// Mac doesn't use this information.
	static void set_dataroot( const std::wstring& ) {};

	// Implements key display_kernel virtual methods
	virtual void activate( bool active );
	EXTENSION_FUNCTION getProcAddress( const char* name );

 private:
	bool initWindow( std::string title, int x, int y, int width, int height );
	void update_size();
	void on_destroy();

	// Functions to manipulate the OpenGL context
	void gl_begin();
	void gl_end();
	void gl_swap_buffers();

	OSStatus vpWindowHandler (EventHandlerCallRef target, EventRef event);
	OSStatus vpMouseHandler (EventHandlerCallRef target, EventRef event);
	OSStatus vpKeyboardHandler (EventHandlerCallRef target, EventRef event);
	static OSStatus vpEventHandler (EventHandlerCallRef target, EventRef event, void * data);

	int getShiftKey();
	int getAltKey();
	int getCtrlKey();

 private:  // data
	static display* current;

	WindowRef	window;
	AGLContext	gl_context;

	bool user_close; // true if user closed the window
	int yadjust; // set to height of title bar when creating a window
 	bool window_visible;

	UInt32 keyModState;
};

/***************** gui_main implementation ********************/

class gui_main
{
 private:
	// Components of the startup sequence.
	static void init_thread(void);

	gui_main();	//< This is the only nonstatic member function that doesn't run in the gui thread!
	void event_loop();
	void poll();

	static gui_main* self;

	int gui_thread;
	mutex init_lock;
	condition initialized;

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

#endif /*VPYTHON_MAC_DISPLAY_HPP*/
