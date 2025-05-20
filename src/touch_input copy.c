// #include "touch_input.h"

// #include "main.h"
// #include "keymap.h"
// #include "renderer.h"
// #include "input.h"

// #include "SDL_keyboard.h"

// // bool toggleRightClickTouch = FALSE;

// int toggle_arrow_keys_touch = FALSE;

// VirtualJoystick vjoy = {550, 300, 60, 550, 300, 0, 0.0f, 0.0f};

// touch_button fn_buttons[] = {}; // initialized in init_fn_buttons

// touch_button pause_button = {};
// static int pause_button_ctr = 0;

// // used to determine if init_fn_buttons has been called once only
// static int init_touch_buttons_called = 0;
// /**
//  * Initialize the fn_buttons array.
//  *
//  * This function only needs to be called once, and it is automatically
//  * called by handle_touch_inputs() if it hasn't been called yet.
//  *
//  * The array is initialized with the positions, sizes, and colors of the
//  * function buttons on the screen.
//  */
// void init_touch_buttons()
// {

// 	if (init_touch_buttons_called == 0)
// 	{
// 		init_touch_buttons_called = 1;

// 		pause_button = (touch_button){0, 0, screen_h - 53, 20, 20, {0, 0, 0, 0}, {0, 255, 0, 255}, 0, 0, 0};

// 		const int left_size = 33;
// 		const int right_size = 32;

// 		SDL_Color colors[] = {
// 			{0, 255, 0, 255},
// 			{255, 0, 0, 255},
// 			{0, 0, 255, 255},
// 			{0, 255, 255, 255},
// 			{255, 255, 0, 255},
// 		};

// 		SDL_Keysym keys[] = {
// 			// left side
// 			{.scancode = SDL_SCANCODE_F1, .sym = SDLK_F1},
// 			{.scancode = SDL_SCANCODE_F2, .sym = SDLK_F2},
// 			{.scancode = SDL_SCANCODE_F3, .sym = SDLK_F3},
// 			{.scancode = SDL_SCANCODE_F4, .sym = SDLK_F4},
// 			// right side
// 			{.scancode = SDL_SCANCODE_F10, .sym = SDLK_F10},
// 			{.scancode = SDL_SCANCODE_F9, .sym = SDLK_F9},
// 			{.scancode = SDL_SCANCODE_F8, .sym = SDLK_F8},
// 			{.scancode = SDL_SCANCODE_F7, .sym = SDLK_F7},
// 			{.scancode = SDL_SCANCODE_F6, .sym = SDLK_F6},
// 		};

// 		int pressed_max[] = {
// 			// left side
// 			2, // f1 0
// 			1, // f2 1 galaxy map
// 			0, // f3 2
// 			0, // f4 3
// 			// right side
// 			0, // f10 4
// 			0, // f9 5
// 			0, // f8 6
// 			0, // f7 7
// 			0, // f6 8
// 		};

// 		for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
// 		{
// 			if (i < 4)
// 			{
// 				// left side
// 				fn_buttons[i].index = i;
// 				fn_buttons[i].x = left_size * i;
// 				fn_buttons[i].y = screen_h - left_size;
// 				fn_buttons[i].width = left_size;
// 				fn_buttons[i].height = left_size;
// 				fn_buttons[i].debug_color = colors[i];
// 				fn_buttons[i].sdlkey = keys[i];
// 				fn_buttons[i].times_pressed = 0;
// 				fn_buttons[i].times_pressed_max = pressed_max[i];
// 			}
// 			else
// 			{
// 				// right side
// 				fn_buttons[i].index = i;
// 				fn_buttons[i].x = screen_w - right_size * (i - 3);
// 				fn_buttons[i].y = screen_h - right_size;
// 				fn_buttons[i].width = right_size;
// 				fn_buttons[i].height = right_size;
// 				fn_buttons[i].debug_color = colors[i - 4];
// 				fn_buttons[i].sdlkey = keys[i];
// 				fn_buttons[i].times_pressed = 0;
// 				fn_buttons[i].times_pressed_max = pressed_max[i];
// 			}
// 		}
// 	}
// }

// static int last_button_index = 0;
// int fn_button_pressed(SDL_Event *event)
// {
// 	if (pause_button_ctr == 1)
// 	{
// 		// Pause is active, block fn buttons
// 		return 0;
// 	}

// 	if (event->button.button == SDL_BUTTON_LEFT)
// 	{
// 		for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
// 		{
// 			if (i == 6 || i == 7) // f7 and f6 need to be free as they are zoom buttons
// 			{
// 				continue;
// 			}
// 			if (i == 4 || i == 5 || i == 8)
// 			{
// 				continue;
// 			}

// 			int x = event->button.x;
// 			int y = event->button.y;
// 			// const int HitRegionSize = fn_buttons[i].width;

// 			if (x >= fn_buttons[i].x && x <= fn_buttons[i].x + fn_buttons[i].width && y >= screen_h - fn_buttons[i].height && y <= screen_h)
// 			{
// 				SDL_Keysym sdlkey = fn_buttons[i].sdlkey;

// 				Keymap_KeyDown(&sdlkey);
// 				Keymap_KeyUp(&sdlkey);

// 				fn_buttons[i].active = 1;

// 				if (i == 0)
// 				{
// 					if (last_button_index == 0)
// 					{

// 						pause_button_ctr = 0;
// 						fn_buttons[i].times_pressed++;
// 						if (fn_buttons[i].times_pressed > fn_buttons[i].times_pressed_max)
// 							fn_buttons[i].times_pressed = 0;
// 					}
// 				}
// 				else if (i == 1)
// 				{
// 					if (last_button_index == 1)
// 					{
// 						fn_buttons[i].times_pressed++;
// 						if (fn_buttons[i].times_pressed > fn_buttons[i].times_pressed_max)
// 							fn_buttons[i].times_pressed = 0;
// 					}
// 				}
// 				else
// 				{
// 					fn_buttons[1].times_pressed = 0;
// 					if (last_button_index != 1)
// 					{
// 						fn_buttons[i].times_pressed++;
// 						if (fn_buttons[i].times_pressed > fn_buttons[i].times_pressed_max)
// 							fn_buttons[i].times_pressed = 0;
// 					}
// 				}

// 				if (i == 0 && fn_buttons[0].times_pressed == 2)
// 				{
// 					toggle_arrow_keys_touch = 1;
// 				}
// 				else if (i == 1 && fn_buttons[1].times_pressed == 0)
// 				{
// 					toggle_arrow_keys_touch = 1;
// 				}
// 				else
// 				{
// 					toggle_arrow_keys_touch = 0;
// 				}

// 				last_button_index = i;

// 				printf("Button %d pressed sdlkey: %s times pressed ctr %d max ctr %d\n", i, SDL_GetKeyName(fn_buttons[i].sdlkey.sym), fn_buttons[i].times_pressed, fn_buttons[i].times_pressed_max);

// 				fn_buttons[i].active = 0;

// 				return 1;
// 			}
// 		}
// 	}
// 	return 0;
// }

// int pause_button_pressed(SDL_Event *event)
// {
// 	if (event->button.button == SDL_BUTTON_LEFT)
// 	{
// 		int x = event->button.x;
// 		int y = event->button.y;
// 		// const int HitRegionSize = pause_button.width;

// 		if (x >= pause_button.x && x <= pause_button.x + pause_button.width && y >= pause_button.y && y <= pause_button.y + pause_button.height)
// 		{
// 			SDL_Keysym sdlkey = {.scancode = SDL_SCANCODE_ESCAPE, .sym = SDLK_ESCAPE};

// 			Keymap_KeyDown(&sdlkey);
// 			Keymap_KeyUp(&sdlkey);

// 			pause_button_ctr++;
// 			if (pause_button_ctr == 2)
// 			{
// 				pause_button_ctr = 0;
// 			}

// 			// pause_button_ctr++;

// 			// if (pause_button_ctr == 2)
// 			// {
// 			// 	pause_button_ctr = 0;
// 			// }

// 			// pause_button_ctr = 1;

// 			// if (fn_buttons[0].times_pressed == 2)
// 			// {
// 			// 	fn_buttons[0].times_pressed = 0;
// 			// }

// 			// if (pause_button_ctr == 1)
// 			// {
// 			// 	toggle_arrow_keys_touch = 0;
// 			// }

// 			printf("pressed pause %d\n", pause_button_ctr);
// 			return 1;
// 		}
// 	}
// 	return 0;
// }

// void update_virtual_joystick(int x, int y)
// {
// 	// if ((x >= vjoy.base_x - vjoy.radius) &&
// 	//     (x <= vjoy.base_x + vjoy.radius) &&
// 	//     (y >= vjoy.base_y - vjoy.radius) &&
// 	//     (y <= vjoy.base_y + vjoy.radius))
// 	// if(1)
// 	// {
// 	vjoy.active = 1;
// 	vjoy.knob_x = x;
// 	vjoy.knob_y = y;
// 	vjoy.dx = (x - vjoy.base_x) / (float)vjoy.radius;
// 	vjoy.dy = (y - vjoy.base_y) / (float)vjoy.radius;

// 	// printf("Joystick updated: dx: %.2f, dy: %.2f\n", vjoy.dx, vjoy.dy);
// 	// Input_MousePress(SDL_BUTTON_RIGHT);

// 	SDL_SetRelativeMouseMode(SDL_TRUE);
// 	input.cur_mousebut_state |= 0x1;
// 	input.cur_mousebut_state &= ~0x2;

// 	return;
// 	// }
// 	// else {
// 	//     vjoy.active = 0;
// 	//     // Input_UnMousePress(SDL_BUTTON_RIGHT);
// 	// }
// }

// void handle_virtual_joystick(SDL_Event *event)
// {
// 	switch (event->type)
// 	{
// 	case SDL_MOUSEMOTION:
// 		if (vjoy_mouse_down)
// 		{
// 			update_virtual_joystick(event->motion.x, event->motion.y);
// 			// input.motion_x = vjoy.dx;
// 			// input.motion_y = vjoy.dy;
// 			// input.abs_x = vjoy.dx;
// 			// input.abs_y = vjoy.dy;
// 		}
// 		break;

// 	case SDL_MOUSEBUTTONDOWN:
// 		if (event->button.button == SDL_BUTTON_LEFT)
// 		{
// 			vjoy_mouse_down = 1;
// 		}
// 		break;

// 	case SDL_MOUSEBUTTONUP:
// 		if (event->button.button == SDL_BUTTON_LEFT)
// 		{
// 			vjoy_mouse_down = 0;
// 			vjoy.active = 0; // reset joystick when mouse is released

// 			vjoy.knob_x = vjoy.base_x;
// 			vjoy.knob_y = vjoy.base_y;
// 			vjoy.dx = 0.0f;
// 			vjoy.dy = 0.0f;

// 			SDL_SetRelativeMouseMode(SDL_FALSE);
// 			input.cur_mousebut_state &= ~0x1;
// 			input.cur_mousebut_state |= 0x2;
// 		}
// 		break;
// 	}
// }

// void handle_touch_inputs(SDL_Event *event)
// {
// 	init_touch_buttons();			// only needs to be called once
// 	handle_virtual_joystick(event); // handle virtual joystick called every frame
// }