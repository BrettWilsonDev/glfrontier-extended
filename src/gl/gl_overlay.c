/*
 * gl_overlay.c - draws the touch controls / settings cog (touch_overlay.c)
 * into the gl_draw batch, in window pixel coordinates.
 */
#include "gl_draw.h"
#include "gl_overlay.h"
#include "renderer.h"
#include "touch_overlay.h"

static void canvas_tri(void *ctx, const float xy[6], const OverlayColour col[3])
{
	(void)ctx;
	float a[3] = {xy[0], xy[1], 0}, b[3] = {xy[2], xy[3], 0}, c[3] = {xy[4], xy[5], 0};
	gd_tri_rgba(a, &col[0].r, b, &col[1].r, c, &col[2].r);
}

void gl_overlay_draw(void)
{
	mat4 saved = *gd_projection();
	mat4 window = mat4_ortho(0, (float)screen_w, (float)screen_h, 0, -1, 1); /* y down */
	gd_set_viewport(GD_VP_WINDOW);
	gd_set_projection(&window);
	gd_push();
	gd_identity();

	gd_set_blend(true);
	OverlayCanvas canvas = {canvas_tri, NULL};
	touch_overlay_draw(&canvas);
	gd_set_blend(false);

	gd_pop();
	gd_set_projection(&saved);
}
