#ifndef VPYTHON_GTK2_RENDER_SURFACE_HPP
#define VPYTHON_GTK2_RENDER_SURFACE_HPP

// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "display_kernel.hpp"
#include "mouseobject.hpp"

// Part of the gtkglextmm package:
#include <gtkmm/gl/drawingarea.h>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/box.h>

namespace cvisual {

class render_surface : public Gtk::GL::DrawingArea
{
 private:
	mouse_manager& mouse;

 public:
	render_surface( display_kernel& _core, mouse_manager& mouse, bool activestereo = false);
	display_kernel& core;

	void paint(Gtk::Window* window, bool change, bool vis); // if change, install appropriate cursor
	void swap() { gl_swap_buffers(); }

 protected:
	// Low-level signal handlers
	// Called when the user moves the mouse across the screen.
	virtual bool on_motion_notify_event( GdkEventMotion* event);
	// Called when the window is resized.
	virtual bool on_configure_event( GdkEventConfigure* event);
	// Called when the mouse enters the scene.
	virtual bool on_enter_notify_event( GdkEventCrossing* event);
	// Called when the window is established for the first time.
 	virtual void on_realize();
	virtual bool on_expose_event( GdkEventExpose*);
	virtual bool on_button_press_event( GdkEventButton*);
	virtual bool on_button_release_event( GdkEventButton*);

 private:
	// Timer function for rendering
	bool forward_render_scene();

	template <class E>
	void mouse_event( E* event, int buttons_toggled = 0 );

	// Manage the current OpenGL context
	void gl_begin();
	void gl_end();
	void gl_swap_buffers();
};

} // !namespace cvisual

#endif // !defined VPYTHON_GTK2_RENDER_SURFACE_HPP
