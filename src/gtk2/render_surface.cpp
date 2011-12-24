// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2003, 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "gtk2/render_surface.hpp"
#include "util/errors.hpp"
#include "util/gl_free.hpp"
#include "vpython-config.h"
#include "python/gil.hpp"

#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif

// The following are part of the gtkglextmm package:
#include <gtkmm/gl/init.h>
#include <gdkmm/gl/pixmap.h>
#include <gdkmm/gl/pixmapext.h>

#include <gdkmm/pixmap.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/stock.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/toolbar.h>

#include <gtkmm/toggletoolbutton.h>
#include <gtkmm/radiotoolbutton.h>

#include <algorithm>
#include <sstream>
#include <iostream>
#include <cassert>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#ifdef _WIN32
	#include <windows.h>
#endif

namespace cvisual {

namespace {
	Glib::RefPtr<Gdk::GL::Context> share_list;
}

render_surface::render_surface( display_kernel& _core, mouse_manager& _mouse, bool activestereo)
	:
	core( _core),
	mouse( _mouse )
{
	Glib::RefPtr<Gdk::GL::Config> config;

	if (activestereo) {
		config = Gdk::GL::Config::create(
			Gdk::GL::MODE_RGBA
			| Gdk::GL::MODE_DOUBLE
			| Gdk::GL::MODE_DEPTH
			//| Gdk::GL::MODE_MULTISAMPLE
			| Gdk::GL::MODE_STEREO );
		if (!config) {
			// Try again without the multisample extension.
			config = Gdk::GL::Config::create( Gdk::GL::MODE_RGB
				| Gdk::GL::MODE_DOUBLE
				| Gdk::GL::MODE_DEPTH
				| Gdk::GL::MODE_STEREO );
			if (!config) {
				VPYTHON_WARNING("'active' stereo requested, but not available."
					"  Falling back to: 'nostereo'.");
			}
		}
	}
	else if (!activestereo || !config) {
		config = Gdk::GL::Config::create(
			Gdk::GL::MODE_RGBA
			| Gdk::GL::MODE_DOUBLE
			| Gdk::GL::MODE_DEPTH
			//| Gdk::GL::MODE_MULTISAMPLE
			);
		if (!config) {
			// Try again without the multisample extension.
			config = Gdk::GL::Config::create( Gdk::GL::MODE_RGB
				| Gdk::GL::MODE_DOUBLE
				| Gdk::GL::MODE_DEPTH );
			if (!config) {
				VPYTHON_CRITICAL_ERROR("failed to initialize any OpenGL configuration, Aborting.");
				std::exit(1);
			}
		}
	}
	if (share_list) {
		this->set_gl_capability(config, share_list, true);
	}
	else {
		set_gl_capability( config);
	}

	add_events( Gdk::EXPOSURE_MASK
		| Gdk::BUTTON_PRESS_MASK
		| Gdk::BUTTON_RELEASE_MASK
		| Gdk::POINTER_MOTION_MASK
		| Gdk::ENTER_NOTIFY_MASK
		| Gdk::BUTTON2_MOTION_MASK
		| Gdk::BUTTON3_MOTION_MASK);
	set_size_request( 384, 256);

	set_flags( get_flags() | Gtk::CAN_FOCUS);
}

template <class E>
void
render_surface::mouse_event( E* event, int buttons_toggled ) {

	bool buttons[] = { event->state & GDK_BUTTON1_MASK, event->state & GDK_BUTTON3_MASK, event->state & GDK_BUTTON2_MASK };
	bool shiftState[] = { event->state & GDK_SHIFT_MASK, event->state & GDK_CONTROL_MASK, event->state & GDK_MOD1_MASK };

	buttons[0] = (buttons[0] != bool( buttons_toggled&1 ));
	buttons[1] = (buttons[1] != bool( buttons_toggled&4 ));
	buttons[2] = (buttons[2] != bool( buttons_toggled&2 ));

	mouse.report_mouse_state( 3, buttons, event->x, event->y, 3, shiftState, false );

	// TODO: Is mouse locking possible with GTK2?
}

bool
render_surface::on_motion_notify_event( GdkEventMotion* event)
{
	python::gil_lock L;
	mouse_event( event );
	return true;
}

bool
render_surface::on_enter_notify_event( GdkEventCrossing* event)
{
	python::gil_lock L;

	bool buttons[] = { 0,0,0 };
	bool shiftState[] = { 0,0,0 };

	mouse.report_mouse_state( 3, buttons, event->x, event->y, 3, shiftState, false );

	mouse_event( event );
	return true;
}

bool
render_surface::on_configure_event( GdkEventConfigure* event)
{
	// Driven by resize events, not window move events, because
	//   it's attached to the drawing area, not the window,
	//   so that a window move doesn't affect the drawing area
	python::gil_lock L;
	// Report width and height of drawing area:
	core.report_view_resize( event->width, event->height );
	return true;
}

void
render_surface::on_realize()
{
	python::gil_lock L;
	Gtk::GL::DrawingArea::on_realize();

	if (!share_list) {
		share_list = get_gl_context();
	}
	assert( share_list);
}

/* Not yet working; see paint function below
GdkCursor* arrow_cursor=0;
GdkCursor* blank_cursor=0;
*/

void
render_surface::paint(Gtk::Window* window, bool change, bool vis) // if change, install appropriate cursor
{
	gl_begin();
	{
		python::gil_lock L;
		if (change and !vis) {
			std::cerr << "cursor.visible = 0 is not yet supported on Linux." << std::endl;
		}
		/* TODO: The following doesn't work because window is a high-level GtkA::Window,
		 * and we need a low-level GdkWindow in gdk_window_set_cursor. Not clear how to proceed.
		if (change) {
			if (!arrow_cursor) {
				arrow_cursor = gdk_cursor_new(GDK_ARROW);
				blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR); // blank cursor not yet available in Ubuntu 20090401
			}
			if (vis) {
				gdk_window_set_cursor(window, arrow_cursor);
			} else {
				gdk_window_set_cursor(window, blank_cursor);
			}
		}
		*/
		core.render_scene(); // render the scene
	}
	gl_end();
}

bool
render_surface::on_expose_event( GdkEventExpose*)
{
	// TODO: should we render here, instead of waiting for the normal frame rate?
	// paint(); swap();

	return true;
}

bool
render_surface::on_button_press_event( GdkEventButton* event)
{
	python::gil_lock L;
	mouse_event( event, 1 << (event->button-1) );
	return true;
}

bool
render_surface::on_button_release_event( GdkEventButton* event)
{
	python::gil_lock L;
	mouse_event( event, 1 << (event->button-1) );
	return true;
}

void
render_surface::gl_begin()
{
	bool ok = get_gl_window()->gl_begin(get_gl_context());
	assert(ok);
}

void
render_surface::gl_end()
{
	get_gl_window()->gl_end();
}

void
render_surface::gl_swap_buffers()
{
	gl_begin();
	get_gl_window()->swap_buffers();
	glFinish(); 	// Ensure rendering completes here (without the GIL) rather than at the next paint (with the GIL)
	gl_end();
}

} // !namespace cvisual
