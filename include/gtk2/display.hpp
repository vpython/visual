#ifndef VPYTHON_GTK2_DISPLAY_HPP
#define VPYTHON_GTK2_DISPLAY_HPP

// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "display_kernel.hpp"
#include "gtk2/render_surface.hpp"
#include "util/atomic_queue.hpp"
#include "mouseobject.hpp"

#include <boost/scoped_ptr.hpp>


#include <gtkmm/gl/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/box.h>
#include <libglademm/xml.h>

namespace cvisual {
using boost::scoped_ptr;

class display : public display_kernel,
	public sigc::trackable
{
public:
	display();
	virtual ~display();

	// Called by the gui_main class below (or render_manager as its agent)
	void create();
	void destroy();
	void paint();
	void swap() { area->swap(); }

	// Tells the application where it can find its data.
	static void set_dataroot( const std::wstring& dataroot);

	// Implements key display_kernel virtual methods
	virtual void activate(bool);
	EXTENSION_FUNCTION getProcAddress( const char* name );

 private:
	static int get_titlebar_height();
	static int get_toolbar_height();

	// Signal handlers for the various widgets.
	void on_fullscreen_clicked();
	void on_pan_clicked();
	void on_rotate_clicked();
	void on_quit_clicked();
	void on_zoom_clicked();
	bool on_window_delete( GdkEventAny*);
	void on_window_configure( GdkEventConfigure*);
	bool on_key_pressed( GdkEventKey*);

 private:
	scoped_ptr<render_surface> area;
	Glib::RefPtr<Gnome::Glade::Xml> glade_file;
	Gtk::Window* window;
};

// A singleton.  This class provides all of the abstraction from the Gtk::Main
// object, in addition to providing asynchronous communication channels between
// threads.
class gui_main : public sigc::trackable
{
 private:
	Gtk::Main kit;

	Glib::Dispatcher signal_add_display;
	void add_display_impl();

	Glib::Dispatcher signal_remove_display;
	void remove_display_impl();

	Glib::Dispatcher signal_shutdown;
	void shutdown_impl();

	bool poll(); //< Runs periodically to update displays

	// Storage used for communication, initialized by the caller, filled by the
	// callee.  Some of them are marked volitile to inhibit optimizations that
	// could prevent a read operation from observing the change in state.
	mutex call_lock;
	condition call_complete;
	display* caller;
	volatile bool returned;
	volatile bool waiting_allclosed;
	volatile bool thread_exited;
	volatile bool shutting_down;

	std::vector<display*> displays;

	// Componants of the startup sequence.
	gui_main();
	void run();
	static gui_main* self; //< Always initialized by the thread after it starts up.

	// init_{signal,lock} are always initialized by the Python thread.
	static mutex* init_lock;
	static condition* init_signal;
	static void thread_proc(void);
	static void init_thread(void);

 public:
	// Force all displays to close and exit the Gtk event loop.
	static void shutdown();

	// Called by a display to make it visible, or invisible.
	static void add_display( display*);
	static void remove_display( display*);

	// Called by a display from within the Gtk loop when closed by the user.
	static void report_window_delete( display*);
	static void quit();

	// This signal is invoked when the gui thread exits on shutdown.
	// wrap_display_kernel() connects a signal handler that forces Python to
	// exit upon shutdown of the render loop.
	static boost::signal<void()> on_shutdown;
};

} // !namespace cvisual

#endif // !defined VPYTHON_GTK2_DISPLAY_HPP
