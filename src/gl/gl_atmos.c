/*
 * gl_atmos.c - atmosphere haze.
 *
 * Near a planet the game builds its atmosphere (sky haze on the ground,
 * the glowing rim seen from space) as 2D shapes, bands between two bezier
 * curves, and puts them at the back of its software depth tree. No GL hook
 * draws them; the GL depth node made for each one remembers the address of
 * the game's node (PRIM_SOFT_NODE), and the band is filled from there when
 * that node is drawn, so it lands in exactly the game's draw order.
 *
 * Band node layout (words, after the 12 byte tree header):
 *   42                            start shape
 *   90  x0 y0 x1 y1 x2 y2 x3 y3   bezier edge
 *   56  x0 y0 x1 y1               straight edge
 *   104 colour_id                 fill (even-odd) and end
 * Coordinates are 3D view pixels: x 0..320, y 0..168 downwards.
 */
#include "gl_fill2d.h"
#include "gl_scene.h"
#include "m68000.h"
#include "host.h"

#define ADDR_DYN_COLS 0x56 /* dynamic colour table */
#define TREE_HEADER   12   /* z, two child pointers */

#define OP_SHAPE  42
#define OP_LINE   56
#define OP_BEZIER 90
#define OP_FILL   104

static void draw_band(uint32_t p)
{
	fill2d_begin();
	for (int guard = 0; guard < 64; guard++)
	{
		int op = STMemory_ReadWord(p);
		p += 2;
		switch (op)
		{
		case OP_BEZIER:
		{
			int16_t w[8];
			for (int i = 0; i < 8; i++)
				w[i] = STMemory_ReadWord(p + i * 2);
			p += 16;
			fill2d_bezier(w, 0);
			break;
		}
		case OP_LINE:
			fill2d_line(STMemory_ReadWord(p), STMemory_ReadWord(p + 2), STMemory_ReadWord(p + 4),
						STMemory_ReadWord(p + 6), 0);
			p += 8;
			break;
		case OP_FILL:
		{
			int rgb = STMemory_ReadWord(ADDR_DYN_COLS + STMemory_ReadWord(p) + 2) & 0xfff;
			fill2d_draw(FILL2D_EVEN_ODD, fill2d_solid, &rgb);
			return;
		}
		default:
			return; /* not a band */
		}
	}
}

void draw_soft_node(const void *payload)
{
	uint32_t node = *(const uint32_t *)payload;
	if (node == 0 || node + TREE_HEADER + 2 >= MEM_SIZE)
		return;
	if (STMemory_ReadWord(node + TREE_HEADER) == OP_SHAPE)
		draw_band(node + TREE_HEADER + 2);
}
