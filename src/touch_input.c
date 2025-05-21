#include "touch_input.h"

#include "main.h"
#include "keymap.h"
#include "renderer.h"
#include "input.h"

#include "SDL_keyboard.h"

int toggle_arrow_keys_touch = FALSE;

VirtualJoystick vjoy = {550, 300, 60, 550, 300, 0, 0.0f, 0.0f};

touch_button fn_buttons[] = {}; // initialized in init_fn_buttons
touch_button arrow_buttons[] = {};
touch_button thrust_buttons[] = {};

touch_button pause_button = {};

// used to determine if init_touch_buttons has been called once only
static int init_touch_buttons_called = 0;
void init_touch_buttons()
{
	if (init_touch_buttons_called == 0)
	{
		init_touch_buttons_called = 1;

		pause_button = (touch_button){0, 0, screen_h - 53, 20, 20, {0, 0, 0, 0}, {0, 255, 0, 255}, 0, {.scancode = SDL_SCANCODE_ESCAPE, .sym = SDLK_ESCAPE}};

		const int left_size = 33;
		const int right_size = 32;

		SDL_Color colors[] = {
			{0, 255, 0, 255},
			{255, 0, 0, 255},
			{0, 0, 255, 255},
			{0, 255, 255, 255},
			{255, 255, 0, 255},
		};

		SDL_Keysym keys[] = {
			// left side
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_F1},
			{.scancode = SDL_SCANCODE_F2, .sym = SDLK_F2},
			{.scancode = SDL_SCANCODE_F3, .sym = SDLK_F3},
			{.scancode = SDL_SCANCODE_F4, .sym = SDLK_F4},
			// right side
			{.scancode = SDL_SCANCODE_F10, .sym = SDLK_F10},
			{.scancode = SDL_SCANCODE_F9, .sym = SDLK_F9},
			{.scancode = SDL_SCANCODE_F8, .sym = SDLK_F8},
			{.scancode = SDL_SCANCODE_F7, .sym = SDLK_F7},
			{.scancode = SDL_SCANCODE_F6, .sym = SDLK_F6},
		};

		for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
		{
			if (i < 4)
			{
				// left side
				fn_buttons[i].index = i;
				fn_buttons[i].x = left_size * i;
				fn_buttons[i].y = screen_h - left_size;
				fn_buttons[i].width = left_size;
				fn_buttons[i].height = left_size;
				fn_buttons[i].debug_color = colors[i];
				fn_buttons[i].sdlkey = keys[i];
			}
			else
			{
				// right side
				fn_buttons[i].index = i;
				fn_buttons[i].x = screen_w - right_size * (i - 3);
				fn_buttons[i].y = screen_h - right_size;
				fn_buttons[i].width = right_size;
				fn_buttons[i].height = right_size;
				fn_buttons[i].debug_color = colors[i - 4];
				fn_buttons[i].sdlkey = keys[i];
			}
		}

		SDL_Keysym arrow_keys[] = {
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_DOWN},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_LEFT},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_UP},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_RIGHT},
		};

		SDL_Color color = {255, 255, 255, 255};
		int originX = 50;
		int originY = 300;
		int size = 35;
		int arrowSpacing = 40;

		for (int i = 0; i < sizeof(arrow_buttons) / sizeof(arrow_buttons[0]); i++)
		{
			arrow_buttons[i].index = i;
			switch (i)
			{
			case 0: // down arrow
				arrow_buttons[i].x = originX;
				arrow_buttons[i].y = originY;
				break;
			case 1: // left arrow
				arrow_buttons[i].x = originX - arrowSpacing;
				arrow_buttons[i].y = originY;
				break;
			case 2: // up arrow
				arrow_buttons[i].x = originX;
				arrow_buttons[i].y = originY - arrowSpacing;
				break;
			case 3: // right arrow
				arrow_buttons[i].x = originX + arrowSpacing;
				arrow_buttons[i].y = originY;
				break;
			}
			arrow_buttons[i].width = size;
			arrow_buttons[i].height = arrowSpacing;
			arrow_buttons[i].color = color;
			arrow_buttons[i].sdlkey = arrow_keys[i];
		};

		SDL_Color color_thrust = {255, 255, 255, 255};
		originX = screen_w - 100;
		originY = 300;
		size = 35;
		int Spacing = 40;

		for (int i = 0; i < sizeof(thrust_buttons) / sizeof(thrust_buttons[0]); i++)
		{
			thrust_buttons[i].index = i;
			switch (i)
			{
			case 0: // down arrow
				thrust_buttons[i].x = originX;
				thrust_buttons[i].y = originY;
				thrust_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_LSHIFT, .sym = SDLK_RSHIFT};
				break;
			case 1: // left arrow
				thrust_buttons[i].x = originX - Spacing;
				thrust_buttons[i].y = originY - (30);
				thrust_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_COMMA, .sym = SDLK_COMMA};
				break;
			case 2: // up arrow
				thrust_buttons[i].x = originX;
				thrust_buttons[i].y = originY - Spacing;
				thrust_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_RETURN, .sym = SDLK_RETURN};
				break;
			case 3: // right arrow
				thrust_buttons[i].x = originX + Spacing;
				thrust_buttons[i].y = originY - (30);
				thrust_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_PERIOD, .sym = SDLK_PERIOD};
				break;
			}
			thrust_buttons[i].width = size;
			thrust_buttons[i].height = Spacing;
			thrust_buttons[i].color = color_thrust;
			// thrust_buttons[i].sdlkey = arrow_keys[i];
		}
	}
}

int fn_button_pressed(SDL_Event *event)
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
		{
			if (i == 6 || i == 7) // f7 and f6 need to be free as they are zoom buttons
			{
				continue;
			}

			int x = event->button.x;
			int y = event->button.y;

			if (x >= fn_buttons[i].x && x <= fn_buttons[i].x + fn_buttons[i].width && y >= screen_h - fn_buttons[i].height && y <= screen_h)
			{
				SDL_Keysym sdlkey = fn_buttons[i].sdlkey;

				Keymap_KeyDown(&sdlkey);
				Keymap_KeyUp(&sdlkey);

				// printf("Button %d pressed sdlkey: %s times pressed ctr %d max ctr %d\n", i, SDL_GetKeyName(fn_buttons[i].sdlkey.sym), fn_buttons[i].times_pressed, fn_buttons[i].times_pressed_max);

				toggle_arrow_keys_touch = 0;

				if (i == 0)
				{
					fn_buttons[i].active = 1;
				}

				if (i != 0)
				{
					fn_buttons[0].active = 0;
				}

				return 1;
			}
		}
	}
	return 0;
}

static int current_arrow_button = -1;
static int arrow_key_down = 0;

int handle_arrow_buttons_pressed(SDL_Event *event)
{
	int x = event->button.x;
	int y = event->button.y;

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		// First click on arrow_buttons[0] acts as toggle if not enabled yet
		if (!toggle_arrow_keys_touch &&
			x >= arrow_buttons[0].x && x <= arrow_buttons[0].x + arrow_buttons[0].width &&
			y >= arrow_buttons[0].y && y <= arrow_buttons[0].y + arrow_buttons[0].height)
		{
			toggle_arrow_keys_touch = 1;
			return 1;
		}

		// Only allow arrow buttons if toggle is on
		if (toggle_arrow_keys_touch)
		{
			for (int i = 0; i < sizeof(arrow_buttons) / sizeof(arrow_buttons[0]); i++)
			{
				if (x >= arrow_buttons[i].x && x <= arrow_buttons[i].x + arrow_buttons[i].width &&
					y >= arrow_buttons[i].y && y <= arrow_buttons[i].y + arrow_buttons[i].height)
				{
					current_arrow_button = i;
					arrow_key_down = 1;

					SDL_Keysym sdlkey = arrow_buttons[i].sdlkey;
					if (fn_buttons[0].active && !fn_buttons[1].active)
					{
						printf("active\n");
						for (int j = 0; j < 15; j++)
							Keymap_KeyDown(&sdlkey);
					}
					else
					{
						Keymap_KeyDown(&sdlkey);
					}
					return 1;
				}
			}
		}
	}

	// Release key on mouse up
	if (event->type == SDL_MOUSEBUTTONUP && current_arrow_button != -1 && arrow_key_down)
	// if (event->type == SDL_MOUSEBUTTONUP && current_arrow_button != -1)
	{
		SDL_Keysym sdlkey = arrow_buttons[current_arrow_button].sdlkey;
		if (fn_buttons[0].active && !fn_buttons[1].active)
		{
			for (int j = 0; j < 15; j++)
				Keymap_KeyUp(&sdlkey);
		}
		else
		{
			Keymap_KeyUp(&sdlkey);
		}

		current_arrow_button = -1;
		arrow_key_down = 0;
		return 1;
	}

	// Release key if mouse dragged off
	if (event->type == SDL_MOUSEMOTION && current_arrow_button != -1 && arrow_key_down)
	// if (event->type == SDL_MOUSEMOTION && current_arrow_button != -1)
	{
		int i = current_arrow_button;
		if (!(x >= arrow_buttons[i].x && x <= arrow_buttons[i].x + arrow_buttons[i].width &&
			  y >= arrow_buttons[i].y && y <= arrow_buttons[i].y + arrow_buttons[i].height))
		{
			SDL_Keysym sdlkey = arrow_buttons[i].sdlkey;
			if (fn_buttons[0].active && !fn_buttons[1].active)
			{
				for (int j = 0; j < 15; j++)
					Keymap_KeyDown(&sdlkey);
			}
			else
			{
				Keymap_KeyUp(&sdlkey);
			}

			current_arrow_button = -1;
			arrow_key_down = 0;
			return 1;
		}
	}

	return 0;
}

int current_thrust_button = -1;

int handle_thrust_buttons_pressed(SDL_Event *event)
{
	int x = event->button.x;
	int y = event->button.y;

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		for (int i = 0; i < sizeof(thrust_buttons) / sizeof(thrust_buttons[0]); i++)
		{
			if (x >= thrust_buttons[i].x && x <= thrust_buttons[i].x + thrust_buttons[i].width &&
				y >= thrust_buttons[i].y && y <= thrust_buttons[i].y + thrust_buttons[i].height)
			{
				current_thrust_button = i;

				SDL_Keysym sdlkey = thrust_buttons[i].sdlkey;
				Keymap_KeyDown(&sdlkey);
				return 1;
			}
		}
	}

	// Release key on mouse up
	if (event->type == SDL_MOUSEBUTTONUP && current_thrust_button != -1)
	{
		SDL_Keysym sdlkey = thrust_buttons[current_thrust_button].sdlkey;
		Keymap_KeyUp(&sdlkey);

		current_thrust_button = -1;
		return 1;
	}

	// Release key if mouse dragged off
	if (event->type == SDL_MOUSEMOTION && current_thrust_button != -1)
	{
		int i = current_thrust_button;
		if (!(x >= thrust_buttons[i].x && x <= thrust_buttons[i].x + thrust_buttons[i].width &&
			  y >= thrust_buttons[i].y && y <= thrust_buttons[i].y + thrust_buttons[i].height))
		{
			SDL_Keysym sdlkey = thrust_buttons[i].sdlkey;
			Keymap_KeyUp(&sdlkey);

			current_thrust_button = -1;
			return 1;
		}
	}

	return 0;
}

int pause_button_pressed(SDL_Event *event)
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		int x = event->button.x;
		int y = event->button.y;

		if (x >= pause_button.x && x <= pause_button.x + pause_button.width && y >= pause_button.y && y <= pause_button.y + pause_button.height)
		{
			SDL_Keysym sdlkey = {.scancode = SDL_SCANCODE_ESCAPE, .sym = SDLK_ESCAPE};

			Keymap_KeyDown(&sdlkey);
			Keymap_KeyUp(&sdlkey);

			return 1;
		}
	}
	return 0;
}

void update_virtual_joystick(int x, int y)
{
	vjoy.active = 1;
	vjoy.knob_x = x;
	vjoy.knob_y = y;
	vjoy.dx = (x - vjoy.base_x) / (float)vjoy.radius;
	vjoy.dy = (y - vjoy.base_y) / (float)vjoy.radius;

	SDL_SetRelativeMouseMode(SDL_TRUE);
	input.cur_mousebut_state |= 0x1;
	input.cur_mousebut_state &= ~0x2;

	return;
}

void handle_virtual_joystick(SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
		if (vjoy_mouse_down)
		{
			toggle_arrow_keys_touch = 0;
			update_virtual_joystick(event->motion.x, event->motion.y);
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			vjoy_mouse_down = 1;
		}
		break;

	case SDL_MOUSEBUTTONUP:
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			vjoy_mouse_down = 0;
			vjoy.active = 0; // reset joystick when mouse is released

			vjoy.knob_x = vjoy.base_x;
			vjoy.knob_y = vjoy.base_y;
			vjoy.dx = 0.0f;
			vjoy.dy = 0.0f;

			SDL_SetRelativeMouseMode(SDL_FALSE);
			input.cur_mousebut_state &= ~0x1;
			input.cur_mousebut_state |= 0x2;
		}
		break;
	}
}

void handle_touch_inputs(SDL_Event *event)
{
	init_touch_buttons();			// only needs to be called once
	handle_virtual_joystick(event); // handle virtual joystick called every frame
}