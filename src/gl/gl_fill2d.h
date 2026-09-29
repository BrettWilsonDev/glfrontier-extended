/*
 * gl_fill2d.h - scanline fill of 2D shapes made of lines and bezier curves,
 * in the game's 3D view pixel coordinates (x 0..320, y 0..168 downwards).
 * Used to draw shapes the game builds for its own software renderer
 * (atmosphere haze, planets).
 *
 * Two fill rules:
 *   FILL2D_EVEN_ODD  plain even-odd fill (codes ignored).
 *   FILL2D_PLANET    the game's planet rule: code 0 edges are the outline,
 *                    drawing runs from the first code 0 crossing on a row to
 *                    the next one. The region code is the row's base code
 *                    XOR the codes of all crossings so far, and picks the
 *                    colour; that is how surface features and shading work.
 *
 *     fill2d_begin();
 *     fill2d_bezier(words, code);   // any number of edges
 *     fill2d_line(x0, y0, x1, y1, code);
 *     fill2d_row_code(row, code);   // optional per-row base codes
 *     fill2d_draw(FILL2D_PLANET, colour_fn, ctx);
 */
#ifndef GL_FILL2D_H
#define GL_FILL2D_H

#include <stdint.h>

#define FILL2D_VIEW_W 320.0f
#define FILL2D_VIEW_H 168.0f
#define FILL2D_ROWS   168

void fill2d_begin(void);
void fill2d_line(float x0, float y0, float x1, float y1, int code);
/* Cubic bezier: start, control 1, control 2, end (x, y word pairs). */
void fill2d_bezier(const int16_t w[8], int code);
/* XOR `code` into the base region code of `row` and every row above it
 * (the game accumulates these from the bottom of the view upwards). */
void fill2d_row_code(int row, int code);

/* Returns the rgb444 colour for a region code, or -1 to leave it empty */
typedef int (*fill2d_colour_fn)(int code, const void *ctx);

enum Fill2dRule
{
	FILL2D_EVEN_ODD,
	FILL2D_PLANET
};

/* Queues the shape into the 3D view. */
void fill2d_draw(enum Fill2dRule rule, fill2d_colour_fn colour, const void *ctx);

/* Colour function for plain single colour fills: ctx points at an int
 * rgb444 value. */
int fill2d_solid(int code, const void *ctx);

#endif /* GL_FILL2D_H */
