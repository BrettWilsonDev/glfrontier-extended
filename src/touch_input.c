/*
 * touch_input.c - on-screen touch controls: F-key strip, arrow and thrust
 * pads, the dropdown menu and a virtual joystick that drives the mouse.
 *
 * Which pads are shown follows the game's own screen and view, read from
 * emulated RAM (game_state.h).
 *
 * All positions are window pixels, laid out inside the game area and
 * scaled from a 640x480 design.
 */
#include "touch_input.h"

#include "game_state.h"
#include "input.h"
#include "keymap.h"
#include "main.h"
#include "renderer.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

int toggle_arrow_keys_touch;
int toggle_thrust_keys_touch;
int toggle_dropdown_keys_touch;

VirtualJoystick vjoy = {550, 300, 60, 550, 300, 0, 0.0f, 0.0f};

/* fn_buttons: 0..3 F1-F4 (left), 4..8 F10-F6 (right), 9..14 the
 * shift+key time acceleration strip */
touch_button fn_buttons[15];
touch_button arrow_buttons[4];
touch_button thrust_buttons[4];
touch_button dropdown_buttons[DD_COUNT];
touch_button pause_button;
touch_button shoot_button;

/* F-key strip indices (fn_buttons) */
enum
{
	BTN_SHIFT_STRIP = 9, /* first of the shift + key buttons */
};

static bool pads_hidden; /* hidden with the dropdown's d-pad button */
static bool layout_done;
static int vjoy_mouse_down; /* the finger / mouse that may become the virtual joystick is down */
static int vjoy_press_x, vjoy_press_y; /* where it went down */

/* =========================================================================
 * Helpers
 * ========================================================================= */
static SDL_Keysym key(SDL_Scancode sc, SDL_Keycode sym)
{
	SDL_Keysym k = {0};
	k.scancode = sc;
	k.sym = sym;
	return k;
}

static void tap_key(SDL_Keysym k)
{
	Keymap_KeyDown(&k);
	Keymap_KeyUp(&k);
}

static bool inside(const touch_button *b, int x, int y)
{
	return x >= b->x && x <= b->x + b->width && y >= b->y && y <= b->y + b->height;
}

/* `design` pixels at 480 (or 640) scaled to `size`, at least `min` */
static int scaled(int size, int design, int base, int min)
{
	int v = size * design / base;
	return v < min ? min : v;
}

/* In the external view each arrow press only turns the camera a little,
 * so the pad sends it 15 times */
static int arrow_repeat(void)
{
	return game_screen() == SCREEN_FLIGHT ? 15 : 1;
}

/* =========================================================================
 * Layout
 * ========================================================================= */
static void layout_buttons(void)
{
	if (layout_done)
		return;
	layout_done = true;

	int gx = Screen_GetGameOffsetX(), gy = Screen_GetGameOffsetY();
	int gw = Screen_GetGameWidth(), gh = Screen_GetGameHeight();

	int btn_size = scaled(gh, 33, 480, 18);
	int btn_w = scaled(gw, 33, 640, 18);
	int btn_h = scaled(gh, 33, 480, 18);
	int margin_x = scaled(gw, 10, 640, 4);
	const SDL_Color none = {0, 0, 0, 0}, white = {255, 255, 255, 255};

	pause_button = (touch_button){0,        gx,   gy + gh - btn_size, btn_size,
								  btn_size, none, {0, 255, 0, 255},   key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE)};
	shoot_button =
		(touch_button){0,        gx + margin_x, gy + gh - btn_size * 7, btn_size,
					   btn_size, white,         {0, 255, 0, 255},       key(SDL_SCANCODE_SPACE, SDLK_SPACE)};

	/* ---- F-key strip: F1-F4 on the left, F10-F6 on the right ---- */
	static const SDL_Color debug_colours[] = {
		{0, 255, 0, 255}, {255, 0, 0, 255}, {0, 0, 255, 255}, {0, 255, 255, 255}, {255, 255, 0, 255}};
	const SDL_Keysym fkeys[9] = {
		key(SDL_SCANCODE_F1, SDLK_F1), key(SDL_SCANCODE_F2, SDLK_F2),   key(SDL_SCANCODE_F3, SDLK_F3),
		key(SDL_SCANCODE_F4, SDLK_F4), key(SDL_SCANCODE_F10, SDLK_F10), key(SDL_SCANCODE_F9, SDLK_F9),
		key(SDL_SCANCODE_F8, SDLK_F8), key(SDL_SCANCODE_F7, SDLK_F7),   key(SDL_SCANCODE_F6, SDLK_F6)};
	for (int i = 0; i < 9; i++)
	{
		touch_button *b = &fn_buttons[i];
		b->index = i;
		b->x = (i < 4) ? gx + btn_w * i : gx + gw - btn_w * (i - 3);
		b->y = gy + gh - btn_h;
		b->width = btn_w;
		b->height = btn_h;
		b->debug_color = debug_colours[i < 4 ? i : i - 4];
		b->sdlkey = fkeys[i];
	}

	/* ---- shift + key time acceleration strip above the left side ---- */
	SDL_Keysym shift_keys[6] = {key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE), key(SDL_SCANCODE_F1, SDLK_F1),
								key(SDL_SCANCODE_F2, SDLK_F2),         key(SDL_SCANCODE_F3, SDLK_F3),
								key(SDL_SCANCODE_F4, SDLK_F4),         key(SDL_SCANCODE_F5, SDLK_F5)};
	static const float shift_widths[6] = {0.09f, 0.1f, 0.09f, 0.09f, 0.09f, 0.09f};
	float x = (float)gx;
	for (int i = 0; i < 6; i++)
	{
		touch_button *b = &fn_buttons[9 + i];
		int w = (int)(gw * shift_widths[i] * 0.35f);
		b->index = 9 + i;
		b->x = (int)x;
		b->y = gy + gh - btn_h * 2;
		b->width = w;
		b->height = btn_h;
		b->sdlkey = shift_keys[i];
		if (i > 0)
			b->sdlkey.mod = KMOD_LSHIFT;
		x += (float)w;
	}

	/* ---- arrow pad (left) and thrust pad (right) ---- */
	int pad = scaled(gh, 35, 480, 20);
	int spacing = scaled(gh, 40, 480, 24);
	int ax = gx + spacing + pad / 2, ay = gy + gh * 2 / 3;
	const int arrow_pos[4][2] = {{0, 0}, {-1, 0}, {0, -1}, {1, 0}}; /* down, left, up, right */
	const SDL_Keysym arrow_keys[4] = {key(SDL_SCANCODE_DOWN, SDLK_DOWN), key(SDL_SCANCODE_LEFT, SDLK_LEFT),
									  key(SDL_SCANCODE_UP, SDLK_UP), key(SDL_SCANCODE_RIGHT, SDLK_RIGHT)};
	for (int i = 0; i < 4; i++)
	{
		touch_button *b = &arrow_buttons[i];
		b->index = i;
		b->x = ax + arrow_pos[i][0] * spacing;
		b->y = ay + arrow_pos[i][1] * spacing;
		b->width = pad;
		b->height = spacing;
		b->color = white;
		b->sdlkey = arrow_keys[i];
	}

	int tx = gx + gw - spacing - pad / 2 - gw / 20, ty = gy + gh * 2 / 3;
	int side_drop = spacing * 3 / 4;
	/* decelerate (bottom), roll left, accelerate (top), roll right */
	const int thrust_pos[4][2] = {{0, 0}, {-spacing, -side_drop}, {0, -spacing}, {spacing, -side_drop}};
	const SDL_Keysym thrust_keys[4] = {
		key(SDL_SCANCODE_LSHIFT, SDLK_RSHIFT), key(SDL_SCANCODE_COMMA, SDLK_COMMA),
		key(SDL_SCANCODE_RETURN, SDLK_RETURN), key(SDL_SCANCODE_PERIOD, SDLK_PERIOD)};
	for (int i = 0; i < 4; i++)
	{
		touch_button *b = &thrust_buttons[i];
		b->index = i;
		b->x = tx + thrust_pos[i][0];
		b->y = ty + thrust_pos[i][1];
		b->width = pad;
		b->height = spacing;
		b->color = white;
		b->sdlkey = thrust_keys[i];
	}

	/* ---- dropdown column, top right: menu, show / hide pads, zoom +/-, P, C,
	 * settings ---- */
	const SDL_Keysym dd_keys[DD_COUNT] = {{0},
										  {0},
										  key(SDL_SCANCODE_EQUALS, SDLK_EQUALS),
										  key(SDL_SCANCODE_MINUS, SDLK_MINUS),
										  key(SDL_SCANCODE_P, SDLK_p),
										  key(SDL_SCANCODE_C, SDLK_c),
										  {0}};
	int dd_gap = scaled(gh, 4, 480, 2);
	for (int i = 0; i < DD_COUNT; i++)
	{
		touch_button *b = &dropdown_buttons[i];
		b->index = i;
		b->x = gx + gw - pad - dd_gap;
		b->y = gy + dd_gap + (pad + dd_gap) * i;
		b->width = pad;
		b->height = pad;
		b->color = white;
		b->sdlkey = dd_keys[i];
	}
}

void reinit_touch_buttons(void)
{
	layout_done = false;

	/* recentre the joystick in the left of the new game area */
	vjoy.base_x = Screen_GetGameOffsetX() + Screen_GetGameWidth() / 4;
	vjoy.base_y = Screen_GetGameOffsetY() + Screen_GetGameHeight() * 2 / 3;
	vjoy.knob_x = vjoy.base_x;
	vjoy.knob_y = vjoy.base_y;

	layout_buttons();
}

/* =========================================================================
 * Button presses
 * ========================================================================= */
/* A button held like a key on a keyboard: down on touch, up when the
 * finger lifts. The game polls held keys itself (the ST keyboard has no
 * auto repeat); `repeat` also sends the key again while held, after a
 * pause, for what only acts on each press. */
static struct
{
	bool held;
	bool repeat;
	SDL_Keysym key;
	Uint32 next_repeat;
} held_key;

#define KEY_REPEAT_DELAY 500
#define KEY_REPEAT_EVERY 60

static void hold_key(SDL_Keysym k, bool repeat)
{
	Keymap_KeyDown(&k);
	held_key.held = true;
	held_key.repeat = repeat;
	held_key.key = k;
	held_key.next_repeat = SDL_GetTicks() + KEY_REPEAT_DELAY;
}

static bool release_key(void)
{
	if (!held_key.held)
		return false;
	Keymap_KeyUp(&held_key.key);
	held_key.held = false;
	return true;
}

static bool fn_button_pressed(const SDL_Event *event)
{
	int x = event->button.x, y = event->button.y;
	for (int i = 0; i < COUNT(fn_buttons); i++)
	{
		if (!inside(&fn_buttons[i], x, y))
			continue;
		if (i >= BTN_SHIFT_STRIP) /* time acceleration strip: shift + key */
		{
			SDL_Keysym shift = key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT);
			Keymap_KeyDown(&shift);
			tap_key(fn_buttons[i].sdlkey);
			Keymap_KeyUp(&shift);
		}
		else
		{
			hold_key(fn_buttons[i].sdlkey, false); /* held as long as the finger is */
		}
		return true;
	}
	return false;
}

/* A pad button being held: its key goes down `repeat` times on touch and
 * up as many times on release */
typedef struct
{
	int button; /* -1 = none */
	int repeat;
} HeldPad;

static HeldPad held_arrow = {-1, 0}, held_thrust = {-1, 0};
static int held_zoom = -1; /* dropdown zoom button held down (drawn lit) */
static Uint32 fire_time;

int touch_held_arrow(void)
{
	return held_arrow.button;
}
int touch_held_thrust(void)
{
	return held_thrust.button;
}
int touch_held_dropdown(void)
{
	return held_zoom;
}
bool touch_pads_hidden(void)
{
	return pads_hidden;
}
void touch_set_pads_hidden(bool hidden)
{
	pads_hidden = hidden;
}
Uint32 touch_fire_time(void)
{
	return fire_time;
}

static bool handle_pad(const SDL_Event *event, touch_button *buttons, int count, HeldPad *held, bool enabled,
					   int repeat)
{
	int x = event->button.x, y = event->button.y;

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT && enabled)
	{
		for (int i = 0; i < count; i++)
		{
			if (!inside(&buttons[i], x, y))
				continue;
			held->button = i;
			held->repeat = repeat;
			for (int j = 0; j < repeat; j++)
				Keymap_KeyDown(&buttons[i].sdlkey);
			return true;
		}
	}
	if (held->button < 0)
		return false;

	/* release on lift, or when the finger slides off the button */
	if (event->type == SDL_MOUSEBUTTONUP ||
		(event->type == SDL_MOUSEMOTION && !inside(&buttons[held->button], event->motion.x, event->motion.y)))
	{
		SDL_Keysym k = buttons[held->button].sdlkey;
		for (int j = 0; j < held->repeat; j++)
			Keymap_KeyUp(&k);
		held->button = -1;
		return true;
	}
	return false;
}

static bool pause_button_pressed(const SDL_Event *event)
{
	if (!inside(&pause_button, event->button.x, event->button.y))
		return false;
	tap_key(pause_button.sdlkey);
	return true;
}

/* The fire button is drawn with the thrust pad, so only works with it */
static bool shoot_button_pressed(const SDL_Event *event)
{
	if (!toggle_thrust_keys_touch || !inside(&shoot_button, event->button.x, event->button.y))
		return false;
	tap_key(shoot_button.sdlkey);
	fire_time = SDL_GetTicks();
	return true;
}

/* A second finger can fire while the first one steers the joystick */
static void shoot_button_finger(const SDL_Event *event)
{
	int x = (int)(event->tfinger.x * screen_w), y = (int)(event->tfinger.y * screen_h);
	if (toggle_thrust_keys_touch && inside(&shoot_button, x, y))
	{
		tap_key(shoot_button.sdlkey);
		fire_time = SDL_GetTicks();
	}
}

static bool dropdown_button_pressed(const SDL_Event *event)
{
	for (int i = 0; i < DD_COUNT; i++)
	{
		if (!toggle_dropdown_keys_touch && i != DD_MENU)
			continue; /* collapsed: only the menu button */
		if (!inside(&dropdown_buttons[i], event->button.x, event->button.y))
			continue;

		switch (i)
		{
		case DD_MENU:
			toggle_dropdown_keys_touch = !toggle_dropdown_keys_touch;
			break;
		case DD_PADS:
			pads_hidden = !pads_hidden;
			break;
		case DD_ZOOM_IN:
		case DD_ZOOM_OUT: /* zoom: held (the game repeats it) until the finger lifts */
			hold_key(dropdown_buttons[i].sdlkey, true); /* zooms a step per press */
			held_zoom = i;
			return true;
		case DD_SETTINGS:
			toggle_m68k_menu = !toggle_m68k_menu;
			break;
		}
		tap_key(dropdown_buttons[i].sdlkey);
		return true;
	}
	return false;
}

/* =========================================================================
 * Pads for the current screen
 * ========================================================================= */
void touch_update_pads(void)
{
	GameScreen screen = game_screen();
	bool flight = screen == SCREEN_FLIGHT;

	/* arrows turn the external camera and scroll the galaxy map */
	toggle_arrow_keys_touch =
		!pads_hidden && ((flight && game_flight_view() == VIEW_EXTERNAL) || screen == SCREEN_GALAXY_MAP);
	toggle_thrust_keys_touch = !pads_hidden && flight;
}

/* =========================================================================
 * Virtual joystick: dragging steers like holding the left mouse button
 * ========================================================================= */
static void joystick_move(int x, int y)
{
	vjoy.active = 1;
	vjoy.knob_x = x;
	vjoy.knob_y = y;
	vjoy.dx = (x - vjoy.base_x) / (float)vjoy.radius;
	vjoy.dy = (y - vjoy.base_y) / (float)vjoy.radius;

	SDL_SetRelativeMouseMode(SDL_TRUE);
	input.cur_mousebut_state |= 0x1;
	input.cur_mousebut_state &= ~0x2;
}

static void handle_joystick(const SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
		if (vjoy_mouse_down && !vjoy.active)
		{
			/* a finger held on something (the game's zoom icons, say) wobbles a
			   little: it stays a held left click until it really drags */
			int dx = event->motion.x - vjoy_press_x, dy = event->motion.y - vjoy_press_y;
			int dead = scaled(Screen_GetGameHeight(), 14, 480, 8);
			if (dx * dx + dy * dy < dead * dead)
				break;
		}
		if (vjoy_mouse_down)
			joystick_move(event->motion.x, event->motion.y);
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			vjoy_mouse_down = 1;
			vjoy_press_x = event->button.x;
			vjoy_press_y = event->button.y;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			bool was_steering = vjoy.active;
			vjoy_mouse_down = 0;
			vjoy.active = 0;
			vjoy.knob_x = vjoy.base_x;
			vjoy.knob_y = vjoy.base_y;
			vjoy.dx = vjoy.dy = 0.0f;
			if (was_steering)
			{
				SDL_SetRelativeMouseMode(SDL_FALSE);
				input.cur_mousebut_state &= ~0x1;
			}
		}
		break;
	case SDL_FINGERDOWN:
		shoot_button_finger(event);
		break;
	default:
		break;
	}
}

/* =========================================================================
 * Entry points
 * ========================================================================= */
void touch_input_tick(void)
{
	if (held_key.held && held_key.repeat && (Sint32)(SDL_GetTicks() - held_key.next_repeat) >= 0)
	{
		Keymap_KeyDown(&held_key.key);
		held_key.next_repeat = SDL_GetTicks() + KEY_REPEAT_EVERY;
	}
}

int handle_touch_inputs(SDL_Event *event)
{
	layout_buttons();
	touch_update_pads();
	handle_joystick(event);

	bool consumed = false;
	if (event->type == SDL_MOUSEBUTTONUP && release_key())
	{
		held_zoom = -1;
		consumed = true;
	}
	consumed |= handle_pad(event, arrow_buttons, COUNT(arrow_buttons), &held_arrow, toggle_arrow_keys_touch,
						   arrow_repeat());
	consumed |=
		handle_pad(event, thrust_buttons, COUNT(thrust_buttons), &held_thrust, toggle_thrust_keys_touch, 1);
	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		consumed = consumed || fn_button_pressed(event) || pause_button_pressed(event) ||
				   shoot_button_pressed(event) || dropdown_button_pressed(event);
	}
	return consumed;
}
