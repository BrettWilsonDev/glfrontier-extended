/*
 * touch_overlay.c - draws the touch controls, see touch_overlay.h.
 *
 * Everything is filled, alpha blended triangles: shapes get a one pixel
 * feathered edge (alpha fading to 0) so they look smooth without MSAA.
 * Buttons are dark glass with a light rim, orange when held, in the game's
 * own colours (panel greys, HUD orange, scanner red).
 */
#include <math.h>

#include "freecam.h"
#include "main.h"
#include "renderer.h"
#include "touch_input.h"
#include "touch_overlay.h"

#define PI         3.14159265358979323846f
#define COUNT(a)   ((int)(sizeof(a) / sizeof((a)[0])))
#define MAX_POINTS 160

typedef OverlayColour Col;

static const OverlayCanvas *cv;
static float unit = 1.0f; /* window pixels per pixel of a 480 line screen */
static const float AA = 1.0f; /* feather width, pixels */

/* ---- colours ---- */
static const Col BG = {10, 14, 26, 150};         /* button glass */
static const Col BG_HELD = {255, 136, 0, 120};   /* HUD orange */
static const Col RIM = {200, 208, 224, 110};
static const Col RIM_HELD = {255, 160, 50, 235};
static const Col ICON = {238, 242, 250, 240};
static const Col ICON_DIM = {238, 242, 250, 110};
static const Col ORANGE = {255, 136, 0, 240};
static const Col FIRE_BG = {110, 0, 0, 140};
static const Col FIRE_BG_HELD = {238, 20, 0, 190};
static const Col FIRE_RIM = {255, 70, 50, 220};
static const Col PANEL = {8, 12, 22, 120}; /* behind the open dropdown */

static Col alpha(Col c, float k)
{
	c.a = (unsigned char)(c.a * k);
	return c;
}

/* =========================================================================
 * Primitives
 * ========================================================================= */
static void tri(float x1, float y1, Col c1, float x2, float y2, Col c2, float x3, float y3, Col c3)
{
	const float xy[6] = {x1, y1, x2, y2, x3, y3};
	const Col col[3] = {c1, c2, c3};
	cv->tri(cv->ctx, xy, col);
}

/* quad a b c d (in order round the edge) */
static void quad(const float *a, Col ca, const float *b, Col cb, const float *c, Col cc, const float *d, Col cd)
{
	tri(a[0], a[1], ca, b[0], b[1], cb, c[0], c[1], cc);
	tri(a[0], a[1], ca, c[0], c[1], cc, d[0], d[1], cd);
}

/* Miter normal at each point of a path, unit length along the edge normals
 * and longer at corners. closed: the path wraps around. */
static void path_normals(const float *p, int n, bool closed, float *nrm)
{
	for (int i = 0; i < n; i++)
	{
		int ip = i - 1, in = i + 1;
		if (closed)
		{
			ip = (i + n - 1) % n;
			in = (i + 1) % n;
		}
		float n1x = 0, n1y = 0, n2x = 0, n2y = 0;
		bool has1 = ip >= 0, has2 = in < n;
		if (has1)
		{
			float dx = p[i * 2] - p[ip * 2], dy = p[i * 2 + 1] - p[ip * 2 + 1];
			float l = sqrtf(dx * dx + dy * dy);
			if (l > 0)
			{
				n1x = -dy / l;
				n1y = dx / l;
			}
		}
		if (has2)
		{
			float dx = p[in * 2] - p[i * 2], dy = p[in * 2 + 1] - p[i * 2 + 1];
			float l = sqrtf(dx * dx + dy * dy);
			if (l > 0)
			{
				n2x = -dy / l;
				n2y = dx / l;
			}
		}
		if (!has1)
		{
			n1x = n2x;
			n1y = n2y;
		}
		if (!has2)
		{
			n2x = n1x;
			n2y = n1y;
		}
		float mx = n1x + n2x, my = n1y + n2y, ml = sqrtf(mx * mx + my * my);
		if (ml < 1e-4f)
		{
			nrm[i * 2] = n1x;
			nrm[i * 2 + 1] = n1y;
			continue;
		}
		mx /= ml;
		my /= ml;
		float d = mx * n1x + my * n1y; /* cos of half the turn */
		float k = d > 0.34f ? 1.0f / d : 3.0f; /* miter limit */
		nrm[i * 2] = mx * k;
		nrm[i * 2 + 1] = my * k;
	}
}

/* Filled convex (or star shaped round its centre) polygon, with a feathered
 * rim; centre and edge colours for soft gradients */
static void fill_poly2(const float *p, int n, Col centre, Col edge)
{
	float cx = 0, cy = 0, nrm[MAX_POINTS * 2];
	for (int i = 0; i < n; i++)
	{
		cx += p[i * 2];
		cy += p[i * 2 + 1];
	}
	cx /= n;
	cy /= n;
	path_normals(p, n, true, nrm);
	/* the normals point to one side of the path: make them point outwards */
	float s = (p[0] - cx) * nrm[0] + (p[1] - cy) * nrm[1] < 0 ? -1.0f : 1.0f;
	Col clear = alpha(edge, 0);
	for (int i = 0; i < n; i++)
	{
		int j = (i + 1) % n;
		const float *a = &p[i * 2], *b = &p[j * 2];
		tri(cx, cy, centre, a[0], a[1], edge, b[0], b[1], edge);
		float ao[2] = {a[0] + nrm[i * 2] * s * AA, a[1] + nrm[i * 2 + 1] * s * AA};
		float bo[2] = {b[0] + nrm[j * 2] * s * AA, b[1] + nrm[j * 2 + 1] * s * AA};
		quad(a, edge, b, edge, bo, clear, ao, clear);
	}
}

static void fill_poly(const float *p, int n, Col c)
{
	fill_poly2(p, n, c, c);
}

/* A line of `width` along a path, feathered on both sides */
static void stroke(const float *p, int n, bool closed, float width, Col c)
{
	if (n < 2)
		return;
	float nrm[MAX_POINTS * 2];
	path_normals(p, n, closed, nrm);
	float h = width * 0.5f;
	Col clear = alpha(c, 0);
	int segs = closed ? n : n - 1;
	for (int i = 0; i < segs; i++)
	{
		int j = (i + 1) % n;
		const float *a = &p[i * 2], *b = &p[j * 2];
		const float *na = &nrm[i * 2], *nb = &nrm[j * 2];
		float al[2] = {a[0] + na[0] * h, a[1] + na[1] * h}, ar[2] = {a[0] - na[0] * h, a[1] - na[1] * h};
		float bl[2] = {b[0] + nb[0] * h, b[1] + nb[1] * h}, br[2] = {b[0] - nb[0] * h, b[1] - nb[1] * h};
		float alo[2] = {a[0] + na[0] * (h + AA), a[1] + na[1] * (h + AA)};
		float aro[2] = {a[0] - na[0] * (h + AA), a[1] - na[1] * (h + AA)};
		float blo[2] = {b[0] + nb[0] * (h + AA), b[1] + nb[1] * (h + AA)};
		float bro[2] = {b[0] - nb[0] * (h + AA), b[1] - nb[1] * (h + AA)};
		quad(al, c, bl, c, br, c, ar, c);
		quad(alo, clear, blo, clear, bl, c, al, c);
		quad(ar, c, br, c, bro, clear, aro, clear);
	}
}

static int circle_points(float cx, float cy, float r, float a0, float a1, int n, float *out)
{
	for (int i = 0; i < n; i++)
	{
		float t = a0 + (a1 - a0) * i / (float)(n - 1 > 0 ? n - 1 : 1);
		out[i * 2] = cx + cosf(t) * r;
		out[i * 2 + 1] = cy + sinf(t) * r;
	}
	return n;
}

static int segments(float r)
{
	int n = (int)(r * 0.8f);
	return n < 20 ? 20 : n > 72 ? 72 : n;
}

static void fill_circle2(float cx, float cy, float r, Col centre, Col edge)
{
	float p[MAX_POINTS * 2];
	int n = segments(r);
	for (int i = 0; i < n; i++)
	{
		float t = 2 * PI * i / n;
		p[i * 2] = cx + cosf(t) * r;
		p[i * 2 + 1] = cy + sinf(t) * r;
	}
	fill_poly2(p, n, centre, edge);
}

static void fill_circle(float cx, float cy, float r, Col c)
{
	fill_circle2(cx, cy, r, c, c);
}

static void ring(float cx, float cy, float r, float width, Col c)
{
	float p[MAX_POINTS * 2];
	int n = segments(r);
	for (int i = 0; i < n; i++)
	{
		float t = 2 * PI * i / n;
		p[i * 2] = cx + cosf(t) * r;
		p[i * 2 + 1] = cy + sinf(t) * r;
	}
	stroke(p, n, true, width, c);
}

/* An open path with round ends */
static void line_round(const float *p, int n, float width, Col c)
{
	stroke(p, n, false, width, c);
	fill_circle(p[0], p[1], width * 0.5f, c);
	fill_circle(p[(n - 1) * 2], p[(n - 1) * 2 + 1], width * 0.5f, c);
}

static void seg(float x1, float y1, float x2, float y2, float width, Col c)
{
	const float p[4] = {x1, y1, x2, y2};
	line_round(p, 2, width, c);
}

static int round_rect_points(float x, float y, float w, float h, float r, float *out)
{
	const float cx[4] = {x + w - r, x + r, x + r, x + w - r};
	const float cy[4] = {y + r, y + r, y + h - r, y + h - r};
	const int per = 7;
	int n = 0;
	for (int k = 0; k < 4; k++)
	{
		float a0 = -PI / 2 * k, a1 = a0 - PI / 2; /* corners anticlockwise from top right */
		n += circle_points(cx[k], cy[k], r, a0, a1, per, out + n * 2);
	}
	return n;
}

static void fill_round_rect(float x, float y, float w, float h, float r, Col c)
{
	float p[MAX_POINTS * 2];
	fill_poly(p, round_rect_points(x, y, w, h, r, p), c);
}

static void stroke_round_rect(float x, float y, float w, float h, float r, float width, Col c)
{
	float p[MAX_POINTS * 2];
	stroke(p, round_rect_points(x, y, w, h, r, p), true, width, c);
}

/* =========================================================================
 * Buttons and icons
 * ========================================================================= */
static float rim_width(void)
{
	return fmaxf(1.2f, 1.4f * unit);
}
static float icon_width(float size)
{
	return fmaxf(1.6f, size * 0.09f);
}

static void round_button(float cx, float cy, float r, bool held)
{
	fill_circle2(cx, cy, r, alpha(held ? BG_HELD : BG, 0.8f), held ? BG_HELD : BG);
	ring(cx, cy, r, rim_width(), held ? RIM_HELD : RIM);
}

static void rect_button(const touch_button *b, bool held)
{
	float r = fminf(b->width, b->height) * 0.24f;
	fill_round_rect(b->x, b->y, b->width, b->height, r, held ? BG_HELD : BG);
	stroke_round_rect(b->x, b->y, b->width, b->height, r, rim_width(), held ? RIM_HELD : RIM);
}

/* Filled triangle pointing along angle `a` (0 = right, y down) */
static void arrow_head(float cx, float cy, float size, float a, Col c)
{
	float p[6];
	for (int i = 0; i < 3; i++)
	{
		float t = a + i * 2 * PI / 3;
		float r = size * (i == 0 ? 1.0f : 0.8f);
		p[i * 2] = cx + cosf(t) * r;
		p[i * 2 + 1] = cy + sinf(t) * r;
	}
	fill_poly(p, 3, c);
}

/* Chevron ">" pointing along angle a */
static void chevron(float cx, float cy, float size, float a, float width, Col c)
{
	float ca = cosf(a), sa = sinf(a);
	float p[6] = {-0.5f, -0.9f, 0.4f, 0.0f, -0.5f, 0.9f};
	for (int i = 0; i < 3; i++)
	{
		float x = p[i * 2] * size * 0.55f, y = p[i * 2 + 1] * size * 0.55f;
		p[i * 2] = cx + x * ca - y * sa;
		p[i * 2 + 1] = cy + x * sa + y * ca;
	}
	line_round(p, 3, width, c);
}

/* Curved arrow round (cx, cy), open at the bottom, head at the end it
 * turns towards (y down: increasing angles go clockwise) */
static void roll_arrow(float cx, float cy, float r, bool clockwise, float width, Col c)
{
	const int n = 36;
	float p[36 * 2];
	float lo = PI * 0.5f + 0.75f, hi = PI * 2.5f - 0.75f; /* the arc's ends */
	float head = width * 1.9f, trim = head * 0.7f / r;  /* arc left for the head */
	float from = clockwise ? lo : hi, to = clockwise ? hi - trim : lo + trim;
	circle_points(cx, cy, r, from, to, n, p);
	line_round(p, n, width, c);
	float end = clockwise ? hi - trim * 0.4f : lo + trim * 0.4f;
	float tangent = clockwise ? end + PI / 2 : end - PI / 2;
	arrow_head(cx + cosf(end) * r, cy + sinf(end) * r, head, tangent, c);
}

static void cog(float cx, float cy, float r, Col c)
{
	const int teeth = 8;
	float body = r * 0.72f, hole = r * 0.28f;
	/* the body as a thick ring (leaves the hole see-through) */
	ring(cx, cy, (body + hole) / 2, body - hole, c);
	for (int i = 0; i < teeth; i++)
	{
		float a = i * 2 * PI / teeth, w0 = PI / teeth * 0.62f, w1 = PI / teeth * 0.42f;
		float p[8] = {cx + cosf(a - w0) * (body - 1), cy + sinf(a - w0) * (body - 1),
					  cx + cosf(a - w1) * r,          cy + sinf(a - w1) * r,
					  cx + cosf(a + w1) * r,          cy + sinf(a + w1) * r,
					  cx + cosf(a + w0) * (body - 1), cy + sinf(a + w0) * (body - 1)};
		fill_poly(p, 4, c);
	}
}

static void magnifier(float cx, float cy, float s, bool plus, float width, Col c)
{
	float r = s * 0.3f, gx = cx - s * 0.08f, gy = cy - s * 0.08f;
	ring(gx, gy, r, width, c);
	float d = r + width * 0.4f, e = r + s * 0.3f;
	seg(gx + d * 0.707f, gy + d * 0.707f, gx + e * 0.707f, gy + e * 0.707f, width * 1.4f, c);
	seg(gx - r * 0.5f, gy, gx + r * 0.5f, gy, width, c);
	if (plus)
		seg(gx, gy - r * 0.5f, gx, gy + r * 0.5f, width, c);
}

/* =========================================================================
 * Flight / map pads
 * ========================================================================= */
static void joystick(void)
{
	if (!vjoy.active)
		return;
	float r = vjoy.radius * 0.5f;
	fill_circle2((float)vjoy.knob_x, (float)vjoy.knob_y, r * 1.6f, alpha(ICON, 0.10f), alpha(ICON, 0.0f));
	fill_circle2((float)vjoy.knob_x, (float)vjoy.knob_y, r, alpha(ICON, 0.35f), alpha(ICON, 0.18f));
	ring((float)vjoy.knob_x, (float)vjoy.knob_y, r, rim_width() * 1.3f, alpha(ICON, 0.7f));
}

static void pad_button(const touch_button *b, bool held, float *cx, float *cy, float *r)
{
	*cx = b->x + b->width / 2.0f;
	*cy = b->y + b->height / 2.0f;
	*r = fminf(b->width, b->height) * 0.5f;
	round_button(*cx, *cy, *r, held);
}

static void arrow_pad(void)
{
	/* down, left, up, right */
	static const float angle[4] = {PI / 2, PI, -PI / 2, 0};
	int held = touch_held_arrow();
	for (int i = 0; i < COUNT(arrow_buttons); i++)
	{
		float cx, cy, r;
		pad_button(&arrow_buttons[i], held == i, &cx, &cy, &r);
		arrow_head(cx, cy, r * 0.42f, angle[i], ICON);
	}
}

static void thrust_pad(void)
{
	int held = touch_held_thrust();
	for (int i = 0; i < COUNT(thrust_buttons); i++)
	{
		float cx, cy, r;
		pad_button(&thrust_buttons[i], held == i, &cx, &cy, &r);
		float w = icon_width(r * 2);
		switch (i)
		{
		case 0: /* decelerate */
		case 2: /* accelerate */
		{
			float a = i == 2 ? -PI / 2 : PI / 2, d = r * 0.2f * (i == 2 ? 1 : -1);
			chevron(cx, cy + d, r * 0.75f, a, w, ICON);
			chevron(cx, cy - d, r * 0.75f, a, w, ICON);
			break;
		}
		case 1: /* roll left */
		case 3: /* roll right */
			roll_arrow(cx, cy, r * 0.42f, i == 3, w, ICON);
			break;
		}
	}

	/* fire */
	const touch_button *b = &shoot_button;
	float cx = b->x + b->width / 2.0f, cy = b->y + b->height / 2.0f, r = fminf(b->width, b->height) * 0.62f;
	Uint32 since = SDL_GetTicks() - touch_fire_time();
	bool flash = touch_fire_time() && since < 140;
	fill_circle2(cx, cy, r, alpha(flash ? FIRE_BG_HELD : FIRE_BG, 0.75f), flash ? FIRE_BG_HELD : FIRE_BG);
	ring(cx, cy, r, rim_width() * 1.3f, FIRE_RIM);
	float w = icon_width(r * 2);
	ring(cx, cy, r * 0.42f, w, ICON);
	for (int k = 0; k < 4; k++)
	{
		float a = k * PI / 2, c = cosf(a), s = sinf(a);
		seg(cx + c * r * 0.2f, cy + s * r * 0.2f, cx + c * r * 0.64f, cy + s * r * 0.64f, w, ICON);
	}
}

/* =========================================================================
 * Dropdown column
 * ========================================================================= */
static void letter_p(float cx, float cy, float s, float w, Col c)
{
	float x = cx - s * 0.17f, top = cy - s * 0.3f, bot = cy + s * 0.3f, br = s * 0.16f;
	float p[24 * 2];
	int n = 0;
	p[n * 2] = x;
	p[n * 2 + 1] = bot;
	n++;
	p[n * 2] = x;
	p[n * 2 + 1] = top;
	n++;
	n += circle_points(x + s * 0.14f, top + br, br, -PI / 2, PI / 2, 20, p + n * 2);
	p[n * 2] = x;
	p[n * 2 + 1] = top + br * 2;
	n++;
	line_round(p, n, w, c);
}

static void letter_c(float cx, float cy, float s, float w, Col c)
{
	float p[32 * 2];
	int n = circle_points(cx + s * 0.03f, cy, s * 0.27f, PI * 0.28f, PI * 1.72f, 32, p);
	line_round(p, n, w, c);
}

static void dropdown(void)
{
	const touch_button *first = &dropdown_buttons[0];
	float gap = (float)(dropdown_buttons[1].y - first->y - first->height);
	bool open = toggle_dropdown_keys_touch;
	int held = touch_held_dropdown();

	if (open)
	{
		const touch_button *last = &dropdown_buttons[DD_COUNT - 1];
		float px = first->x - gap, py = first->y - gap;
		float pw = first->width + gap * 2, ph = last->y + last->height + gap - py;
		fill_round_rect(px, py, pw, ph, pw * 0.3f, PANEL);
		stroke_round_rect(px, py, pw, ph, pw * 0.3f, rim_width(), alpha(RIM, 0.6f));
	}

	for (int i = 0; i < DD_COUNT; i++)
	{
		if (!open && i != DD_MENU)
			break;
		const touch_button *b = &dropdown_buttons[i];
		float cx = b->x + b->width / 2.0f, cy = b->y + b->height / 2.0f, s = fminf(b->width, b->height);
		float w = icon_width(s);
		bool on = held == i || (i == DD_MENU && open) || (i == DD_SETTINGS && toggle_m68k_menu);

		if (i == DD_MENU || on)
		{
			float r = s * 0.24f;
			fill_round_rect(b->x, b->y, b->width, b->height, r, on ? BG_HELD : BG);
			stroke_round_rect(b->x, b->y, b->width, b->height, r, rim_width(), on ? RIM_HELD : RIM);
		}

		switch (i)
		{
		case DD_MENU:
			if (open) /* a cross to close it */
			{
				float d = s * 0.2f;
				seg(cx - d, cy - d, cx + d, cy + d, w, ICON);
				seg(cx - d, cy + d, cx + d, cy - d, w, ICON);
			}
			else
			{
				for (int k = -1; k <= 1; k++)
					seg(cx - s * 0.22f, cy + k * s * 0.17f, cx + s * 0.22f, cy + k * s * 0.17f, w, ICON);
			}
			break;
		case DD_PADS: /* a d-pad: dim while the pads are hidden */
		{
			Col c = touch_pads_hidden() ? ICON_DIM : ICON;
			float a = s * 0.3f, t = s * 0.1f;
			fill_round_rect(cx - t, cy - a, t * 2, a * 2, t * 0.5f, c);
			fill_round_rect(cx - a, cy - t, a - t, t * 2, t * 0.5f, c);
			fill_round_rect(cx + t, cy - t, a - t, t * 2, t * 0.5f, c);
			if (touch_pads_hidden())
				seg(cx - a, cy + a, cx + a, cy - a, w, ORANGE);
			break;
		}
		case DD_ZOOM_IN:
		case DD_ZOOM_OUT:
			magnifier(cx, cy, s, i == DD_ZOOM_IN, w, ICON);
			break;
		case DD_P:
			letter_p(cx, cy, s, w, ICON);
			break;
		case DD_C:
			letter_c(cx, cy, s, w, ICON);
			break;
		case DD_SETTINGS:
			cog(cx, cy, s * 0.3f, ICON);
			break;
		}
	}
}

static void touch_controls(void)
{
	touch_update_pads();
	joystick();
	if (toggle_arrow_keys_touch)
		arrow_pad();
	if (toggle_thrust_keys_touch)
		thrust_pad();
	dropdown();
}

/* =========================================================================
 * Free camera
 * ========================================================================= */
static void freecam_controls(const FreecamTouchView *v)
{
	for (int i = 0; i < FC_BUTTONS; i++)
	{
		const FreecamButton *b = &v->button[i];
		float cx = b->x + b->w / 2, cy = b->y + b->h / 2, s = fminf(b->w, b->h), r = s * 0.5f;
		float w = icon_width(s);
		round_button(cx, cy, r, b->held);
		switch (i)
		{
		case FC_SPEED_DOWN:
			seg(cx - s * 0.2f, cy, cx + s * 0.2f, cy, w, ICON);
			break;
		case FC_SPEED_UP:
			seg(cx - s * 0.2f, cy, cx + s * 0.2f, cy, w, ICON);
			seg(cx, cy - s * 0.2f, cx, cy + s * 0.2f, w, ICON);
			break;
		case FC_HELP: /* "i" */
			seg(cx, cy - s * 0.02f, cx, cy + s * 0.2f, w, ICON);
			fill_circle(cx, cy - s * 0.17f, w * 0.75f, ICON);
			break;
		case FC_EXIT:
			seg(cx - s * 0.17f, cy - s * 0.17f, cx + s * 0.17f, cy + s * 0.17f, w, ICON);
			seg(cx - s * 0.17f, cy + s * 0.17f, cx + s * 0.17f, cy - s * 0.17f, w, ICON);
			break;
		case FC_UP:
		case FC_DOWN:
			arrow_head(cx, cy, s * 0.2f, i == FC_UP ? -PI / 2 : PI / 2, ICON);
			break;
		case FC_ROLL_LEFT:
		case FC_ROLL_RIGHT:
			roll_arrow(cx, cy, s * 0.21f, i == FC_ROLL_RIGHT, w, ICON);
			break;
		}
	}

	/* speed: a bar between - and + */
	const FreecamButton *lo = &v->button[FC_SPEED_DOWN], *hi = &v->button[FC_SPEED_UP];
	float bx = lo->x + lo->w * 1.25f, bw = hi->x - lo->w * 0.25f - bx;
	float bh = lo->h * 0.26f, by = lo->y + (lo->h - bh) / 2;
	fill_round_rect(bx, by, bw, bh, bh / 2, BG);
	stroke_round_rect(bx, by, bw, bh, bh / 2, rim_width(), RIM);
	float fw = bh + (bw - bh) * v->speed_fraction;
	fill_round_rect(bx, by, fw, bh, bh / 2, ORANGE);

	/* thumbstick */
	if (v->stick_active)
	{
		float r = v->stick_radius;
		fill_circle2(v->stick_x, v->stick_y, r, alpha(BG, 0.4f), BG);
		ring(v->stick_x, v->stick_y, r, rim_width(), RIM);
		fill_circle2(v->knob_x, v->knob_y, r * 0.38f, alpha(ICON, 0.55f), alpha(ICON, 0.3f));
		ring(v->knob_x, v->knob_y, r * 0.38f, rim_width() * 1.3f, ICON);
	}
}

/* =========================================================================
 * Debug hit regions
 * ========================================================================= */
static void debug_rect(const touch_button *b, SDL_Color c)
{
	float p[8] = {(float)b->x, (float)b->y, (float)(b->x + b->width), (float)b->y, (float)(b->x + b->width),
				  (float)(b->y + b->height), (float)b->x, (float)(b->y + b->height)};
	stroke(p, 4, true, 1.0f, (Col){c.r, c.g, c.b, 255});
}

static void debug_regions(void)
{
	for (int i = 0; i < COUNT(fn_buttons); i++)
		debug_rect(&fn_buttons[i], fn_buttons[i].debug_color);
	debug_rect(&pause_button, pause_button.debug_color);
}

void touch_overlay_draw(const OverlayCanvas *canvas)
{
	cv = canvas;
	unit = Screen_GetGameHeight() / 480.0f;
	FreecamTouchView freecam;
	if (toggle_touch_controls && freecam_touch_view(&freecam))
	{
		freecam_controls(&freecam);
	}
	else if (toggle_touch_controls)
	{
		touch_controls();
	}
	else
	{
		/* small settings cog in the top left corner of the game area, where
		 * main.c's settings_button() looks for clicks */
		float sx = Screen_GetGameWidth() / 320.0f, sy = Screen_GetGameHeight() / 200.0f;
		cog(Screen_GetGameOffsetX() + 5 * sx, Screen_GetGameOffsetY() + 6 * sy, 3.4f * sy, alpha(ICON, 0.75f));
	}
	if (toggle_debug_draw)
		debug_regions();
	cv = NULL;
}
