// Copyright (c) 2000, 2001, 2002, 2003 by David Scherer and others.
// Copyright (c) 2003, 2004 by Jonathan Brandmeyer and others.
// See the file license.txt for complete license terms.
// See the file authors.txt for a complete list of contributors.

#include "sphere.hpp"
#include "util/quadric.hpp"
#include "util/errors.hpp"
#include "util/icososphere.hpp"
#include "util/gl_enable.hpp"

#include <vector>

namespace cvisual {

displaylist sphere::lod_cache[6];

sphere::sphere()
{
}

sphere::sphere( const sphere& other)
	: axial(other)
{
}

sphere::~sphere()
{
}

void
sphere::gl_pick_render( const view& geometry)
{
	if (degenerate())
		return;
	init_model();

	clear_gl_error();

	gl_matrix_stackguard guard;
	model_world_transform( geometry.gcf, get_scale() ).gl_mult();

	lod_cache[0].gl_render();
	check_gl_error();
}

void
sphere::gl_render( const view& geometry)
{
	if (degenerate())
		return;

	init_model();

	clear_gl_error();
	
	// coverage is the radius of this sphere in pixels:
	double coverage = geometry.pixel_coverage( pos, radius);
	int lod = 0;
	
	if (coverage < 0) // Behind the camera, but still visible.
		lod = 4;
	else if (coverage < 30)
		lod = 0;
	else if (coverage < 100)
		lod = 1;
	else if (coverage < 500)
		lod = 2;
	else if (coverage < 5000)
		lod = 3;
	else
		lod = 4;

	lod += geometry.lod_adjust; // allow user to reduce level of detail
	if (lod > 5)
		lod = 5;
	else if (lod < 0)
		lod = 0;

	gl_matrix_stackguard guard;
	model_world_transform( geometry.gcf, get_scale() ).gl_mult();

	color.gl_set(opacity);

	if (translucent()) {
		// Spheres are convex, so we don't need to sort
		gl_enable cull_face( GL_CULL_FACE);

		// Render the back half (inside)
		glCullFace( GL_FRONT );
		lod_cache[lod].gl_render();

		// Render the front half (outside)
		glCullFace( GL_BACK );
		lod_cache[lod].gl_render();
	}
	else {
		// Render a simple sphere.
		lod_cache[lod].gl_render();
	}
	check_gl_error();
}

void
sphere::grow_extent( extent& e)
{
	e.add_sphere( pos, radius);
	e.add_body();
}

void
sphere::init_model()
{
	if (lod_cache[0]) return;

	clear_gl_error();

	quadric sph;
	
	lod_cache[0].gl_compile_begin();
	sph.render_sphere( 1.0, 13, 7);
	lod_cache[0].gl_compile_end();

	lod_cache[1].gl_compile_begin();
	sph.render_sphere( 1.0, 19, 11);
	lod_cache[1].gl_compile_end();

	lod_cache[2].gl_compile_begin();
	sph.render_sphere( 1.0, 35, 19);
	lod_cache[2].gl_compile_end();

	lod_cache[3].gl_compile_begin();
	sph.render_sphere( 1.0, 55, 29);
	lod_cache[3].gl_compile_end();

	lod_cache[4].gl_compile_begin();
	sph.render_sphere( 1.0, 70, 34);
	lod_cache[4].gl_compile_end();

	// Only for the very largest bodies.
	lod_cache[5].gl_compile_begin();
	sph.render_sphere( 1.0, 140, 69);
	lod_cache[5].gl_compile_end();
	
	check_gl_error();
}

vector
sphere::get_scale()
{
	return vector( radius, radius, radius);
}

bool
sphere::degenerate()
{
	return !visible || radius == 0.0;
}

void
sphere::get_material_matrix(const view&, tmatrix& out) { 
	out.translate( vector(.5,.5,.5) ); 
	vector scale = get_scale();
	out.scale( scale * (.5 / std::max(scale.x, std::max(scale.y, scale.z))) ); 
}

PRIMITIVE_TYPEINFO_IMPL(sphere)

} // !namespace cvisual
