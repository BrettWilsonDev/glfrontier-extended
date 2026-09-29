/*
 * touch_overlay.h - draws the touch controls, the settings cog and the
 * debug hit regions. Renderer independent: each renderer supplies a small
 * canvas that can draw alpha blended triangles in window pixels.
 */
#ifndef TOUCH_OVERLAY_H
#define TOUCH_OVERLAY_H

typedef struct
{
	unsigned char r, g, b, a;
} OverlayColour;

typedef struct
{
	/* one triangle: xy = x1, y1, x2, y2, x3, y3; a colour per corner */
	void (*tri)(void *ctx, const float xy[6], const OverlayColour col[3]);
	void *ctx;
} OverlayCanvas;

/* Everything for this frame: touch pads when enabled (otherwise the small
 * settings cog) and the debug rectangles when toggle_debug_draw is on. */
void touch_overlay_draw(const OverlayCanvas *canvas);

#endif /* TOUCH_OVERLAY_H */
