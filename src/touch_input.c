#include "touch_input.h"

#include "main.h"
#include "keymap.h"
#include "renderer.h"
#include "input.h"

#include "SDL_keyboard.h"

int toggle_arrow_keys_touch = FALSE;
int toggle_thrust_keys_touch = FALSE;
int toggle_dropdown_keys_touch = FALSE;

VirtualJoystick vjoy = {550, 300, 60, 550, 300, 0, 0.0f, 0.0f};

touch_button fn_buttons[] = {}; // initialized in init_fn_buttons
touch_button arrow_buttons[] = {};
touch_button thrust_buttons[] = {};
touch_button dropdown_buttons[] = {};

touch_button pause_button = {};
static int times_paused_pressed = 0;
static enum Views view_before_pause = FIRST_PERSON;
static enum Views person_view = FIRST_PERSON;
touch_button play_button = {};

// static enum Views current_view = FIRST_PERSON;
static enum Views current_view = FIRST_PERSON;
enum Views public_view = FIRST_PERSON;
static int current_button_index = -1;

int pixel_view_eval = -1;

// used to determine if init_touch_buttons has been called once only
static int init_touch_buttons_called = 0;
void init_touch_buttons()
{
	if (init_touch_buttons_called == 0)
	{
		init_touch_buttons_called = 1;

		pause_button = (touch_button){0, 0, screen_h - 53, 20, 20, {0, 0, 0, 0}, {0, 255, 0, 255}, 0, {.scancode = SDL_SCANCODE_ESCAPE, .sym = SDLK_ESCAPE}};
		play_button = (touch_button){0, 20, screen_h - 53, 20 + 82, 20, {0, 0, 0, 0}, {255, 100, 200, 255}, 0, {.scancode = SDL_SCANCODE_ESCAPE, .sym = SDLK_ESCAPE}};

		// ============= fn buttons ===============
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
				fn_buttons[i].times_pressed = 0;
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
				fn_buttons[i].times_pressed = 0;
			}
		}

		SDL_Keysym arrow_keys[] = {
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_DOWN},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_LEFT},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_UP},
			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_RIGHT},
		};

		// =============== arrow buttons =================
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

		// ================ thrust buttons ================
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
		}

		// ================ dropdown buttons ================
		SDL_Color color_dropdown = {255, 255, 255, 255};
		size = 35;
		originY = 2;
		originX = screen_w - size;

		for (int i = 0; i < sizeof(dropdown_buttons) / sizeof(dropdown_buttons[0]); i++)
		{
			dropdown_buttons[i].index = i;
			switch (i)
			{
			case 0: // burger
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				break;
			case 1: // cycle touch controls
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				break;
			case 2: // zoom in
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				dropdown_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_EQUALS, .sym = SDLK_EQUALS};
				break;
			case 3: // zoom out
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				dropdown_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_MINUS, .sym = SDLK_MINUS};
				break;
			case 4: // p to pander to the officer ? such a strange fork of fe2
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				dropdown_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_P, .sym = SDLK_p};
				break;
			case 5: // c to center the map
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				dropdown_buttons[i].sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_C, .sym = SDLK_c};
				break;
			case 6: // m68k menu 
				dropdown_buttons[i].x = originX;
				dropdown_buttons[i].y = originY + (size * i) - 1;
				break;
			}
			dropdown_buttons[i].width = size;
			dropdown_buttons[i].height = size;
			dropdown_buttons[i].color = color_dropdown;
		};
	}
}

int fn_button_pressed(SDL_Event *event)
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
		{
			if ((i == 6 || i == 7) && current_view != CONTACT) // f7 and f6 need to be free as they are zoom buttons
			{
				continue;
			}

			int x = event->button.x;
			int y = event->button.y;

			if (x >= fn_buttons[i].x && x <= fn_buttons[i].x + fn_buttons[i].width && y >= screen_h - fn_buttons[i].height && y <= screen_h)
			{
				SDL_Keysym sdlkey = fn_buttons[i].sdlkey;

				if (i == 0 && (current_view == PAUSE)) // do not allow f1 to be pressed when paused
				{
					return 1;
				}

				// if (i == 0 && (current_view == CONTACT)) // do not allow f1 to be pressed while contacts is open
				// {
				// 	return 1;
				// }

				Keymap_KeyDown(&sdlkey);
				Keymap_KeyUp(&sdlkey);

				if (i == 0)
				{
					fn_buttons[i].active = 1;
				}

				current_button_index = i;

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
int handle_arrow_buttons_pressed(SDL_Event *event)
{
	int x = event->button.x;
	int y = event->button.y;

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		// Only allow arrow buttons if toggle is on
		if (toggle_arrow_keys_touch)
		{
			for (int i = 0; i < sizeof(arrow_buttons) / sizeof(arrow_buttons[0]); i++)
			{
				if (x >= arrow_buttons[i].x && x <= arrow_buttons[i].x + arrow_buttons[i].width &&
					y >= arrow_buttons[i].y && y <= arrow_buttons[i].y + arrow_buttons[i].height)
				{
					current_arrow_button = i;

					SDL_Keysym sdlkey = arrow_buttons[i].sdlkey;
					if (fn_buttons[0].active && !fn_buttons[1].active)
					{
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
	if (event->type == SDL_MOUSEBUTTONUP && current_arrow_button != -1)
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
		return 1;
	}

	// Release key if mouse dragged off
	if (event->type == SDL_MOUSEMOTION && current_arrow_button != -1)
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

			current_button_index = 10;

			return 1;
		}
	}
	return 0;
}

int play_button_pressed(SDL_Event *event)
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		int x = event->button.x;
		int y = event->button.y;

		if (x >= play_button.x && x <= play_button.x + play_button.width && y >= play_button.y && y <= play_button.y + play_button.height)
		{
			current_button_index = 11;

			return 0;
		}
	}
	return 0;
}

static int times_views_cycled = 0;
int dropdown_button_pressed(SDL_Event *event)
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		for (int i = 0; i < sizeof(dropdown_buttons) / sizeof(dropdown_buttons[0]); i++)
		{
			if (!toggle_dropdown_keys_touch && i != 0)
				continue;

			int x = event->button.x;
			int y = event->button.y;

			if (x >= dropdown_buttons[i].x && x <= dropdown_buttons[i].x + dropdown_buttons[i].width &&
				y >= dropdown_buttons[i].y && y <= dropdown_buttons[i].y + dropdown_buttons[i].height)
			{
				if (i == 0)
				{
					toggle_dropdown_keys_touch = !toggle_dropdown_keys_touch;
				}
				if (i == 1)
				{
					times_views_cycled++;

					if (times_views_cycled == 3)
					{
						times_views_cycled = 0;
					}

					if (times_views_cycled == 0)
					{
						current_view = FIRST_PERSON;
					}
					else if (times_views_cycled == 1)
					{
						current_view = THIRD_PERSON;
					}
					else if (times_views_cycled == 2)
					{
						current_view = UNKNOWN;
					}

					if (current_view == UNKNOWN)
					{
						toggle_arrow_keys_touch = 0;
						toggle_thrust_keys_touch = 0;
					}

					current_button_index = -1;

					if (current_view == FIRST_PERSON)
					{
						fn_buttons[0].times_pressed = 0;
					}
					else if (current_view == SECOND_PERSON)
					{
						fn_buttons[0].times_pressed = 1;
					}
					else if (current_view == THIRD_PERSON)
					{
						fn_buttons[0].times_pressed = 2;
					}
				}

				if (i == 6)
				{
					toggle_m68k_menu = !toggle_m68k_menu;
				}

				SDL_Keysym sdlkey = dropdown_buttons[i].sdlkey;

				if (i == 2 || i == 3)
				{
					Keymap_KeyDown(&sdlkey);
				}
				else
				{
					Keymap_KeyDown(&sdlkey);
					Keymap_KeyUp(&sdlkey);
				}

				return 1;
			}
		}
	}
	return 0;
}

/*  the views
	UNKNOWN
	FIRST_PERSON
	SECOND_PERSON
	THIRD_PERSON
	GALAXY_MAP
	GALAXY_MAP2
	GALAXY_MAP3
	SYSTEM_MAP
	SYSTEM_MAP2
	SYSTEM_MAP3
	INFO
	CONTACT
	WHOLE_GALAXY
	PAUSE
	PAUSE_MENU
	PLAY
*/

// used for debugging
char *view_names[] = {"UNKNOWN", "FIRST_PERSON", "SECOND_PERSON", "THIRD_PERSON", "GALAXY_MAP", "GALAXY_MAP2", "GALAXY_MAP3", "SYSTEM_MAP", "SYSTEM_MAP2", "SYSTEM_MAP3", "INFO", "CONTACT", "WHOLE_GALAXY", "PAUSE", "PAUSE_MENU", "PLAY"};

// we need to track the state of each button to know when to display certain touch buttons
void handle_button_state(int index)
{
	if (index == -1)
		return;

	switch (index)
	{
	case 0: // f1
		if (!(current_view == INFO || current_view == CONTACT || current_view == GALAXY_MAP || current_view == SYSTEM_MAP || current_view == WHOLE_GALAXY || current_view == GALAXY_MAP2 || current_view == SYSTEM_MAP2 || current_view == GALAXY_MAP3 || current_view == SYSTEM_MAP3))
		{
			if (!(current_view == PAUSE || current_view == PAUSE_MENU))
				fn_buttons[index].times_pressed++;

			if (fn_buttons[index].times_pressed == 3)
				fn_buttons[index].times_pressed = 0;
		}

		if (fn_buttons[index].times_pressed == 0)
		{
			current_view = FIRST_PERSON;
		}
		else if (fn_buttons[index].times_pressed == 1)
		{
			current_view = SECOND_PERSON;
		}
		else if (fn_buttons[index].times_pressed == 2)
		{
			current_view = THIRD_PERSON;
		}
		person_view = current_view;
		break;
	case 1: // f2
		if (current_view == FIRST_PERSON || current_view == SECOND_PERSON || current_view == THIRD_PERSON || current_view == INFO || current_view == CONTACT || current_view == SYSTEM_MAP3)
		{
			fn_buttons[index].times_pressed = 0;
		}

		fn_buttons[index].times_pressed++;

		if (fn_buttons[index].times_pressed == 2)
			fn_buttons[index].times_pressed = 0;

		if (fn_buttons[index].times_pressed == 0)
		{
			current_view = SYSTEM_MAP;
		}
		else if (fn_buttons[index].times_pressed == 1)
		{
			current_view = GALAXY_MAP;
		}
		break;
	case 2: // f3
		current_view = INFO;
		break;
	case 3: // f4
		current_view = CONTACT;
		break;
	case 4: // f10
		if (current_view == SYSTEM_MAP2)
		{
			current_view = SYSTEM_MAP3;
		}

		if (!(current_view == GALAXY_MAP) && !(current_view == WHOLE_GALAXY) && !(current_view == GALAXY_MAP2))
		{
			break;
		}

		fn_buttons[index].times_pressed++;

		if (fn_buttons[index].times_pressed == 2)
			fn_buttons[index].times_pressed = 0;

		if (fn_buttons[index].times_pressed == 0)
		{
			current_view = GALAXY_MAP3;
		}
		else if (fn_buttons[index].times_pressed == 1)
		{
			current_view = WHOLE_GALAXY;
		}
		break;
	case 7: // f5
		if (current_view == CONTACT)
		{
			current_view = person_view;
		}
		break;
	case 8: // f6
		if (!(current_view == GALAXY_MAP) && !(current_view == SYSTEM_MAP2) && !(current_view == GALAXY_MAP2) && !(current_view == GALAXY_MAP3) && !(current_view == WHOLE_GALAXY))
		{
			break;
		}

		if (current_view == GALAXY_MAP)
		{
			fn_buttons[index].times_pressed = 0;
		}

		fn_buttons[index].times_pressed++;

		if (fn_buttons[index].times_pressed == 2)
			fn_buttons[index].times_pressed = 0;

		if (fn_buttons[index].times_pressed == 0)
		{
			current_view = GALAXY_MAP2;
		}
		else if (fn_buttons[index].times_pressed == 1)
		{
			current_view = SYSTEM_MAP2;
		}
		break;
	case 10: // pause
		times_paused_pressed++;

		view_before_pause = current_view;

		if (times_paused_pressed == 2)
			times_paused_pressed = 0;

		if (times_paused_pressed == 0)
		{
			current_view = PAUSE_MENU;
		}
		else if (times_paused_pressed == 1)
		{
			current_view = PAUSE;
		}
		break;
	case 11: // play
		if (current_view == PAUSE)
		{
			current_view = PLAY;
		}
		if (current_view == PAUSE_MENU)
		{
			current_view = PLAY;
		}
		break;
	default:
		break;
	}

	// printf("view external %s\n", view_names[pixel_view_eval]);

	current_button_index = -1;
	// printf("view state %s\n", view_names[current_view]);

	public_view = current_view;
}

// ================== virtual joystick ==================
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
			// toggle_arrow_keys_touch = 0;
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
// =====================================================

int touch_buttons_pressed(SDL_Event *event)
{
	if (fn_button_pressed(event))
	{
		return 1;
	}
	if (pause_button_pressed(event))
	{
		return 1;
	}
	if (play_button_pressed(event))
	{
		return 1;
	}
	if (dropdown_button_pressed(event))
	{
		return 1;
	}
	return 0;
}

void handle_touch_inputs(SDL_Event *event)
{
	init_touch_buttons();			// only needs to be called once
	handle_virtual_joystick(event); // handle virtual joystick called every frame
	handle_arrow_buttons_pressed(event);
	handle_thrust_buttons_pressed(event);

	handle_button_state(current_button_index);

	if (current_view == UNKNOWN)
		return;

	if (current_view == THIRD_PERSON || current_view == GALAXY_MAP || current_view == GALAXY_MAP2 || current_view == GALAXY_MAP3)
	{
		toggle_arrow_keys_touch = 1;
	}
	else
	{
		toggle_arrow_keys_touch = 0;
	}

	if (current_view == FIRST_PERSON || current_view == SECOND_PERSON || current_view == THIRD_PERSON)
	{
		toggle_thrust_keys_touch = 1;
	}
	else
	{
		toggle_thrust_keys_touch = 0;
	}
}