/*
 * touch_input.h - on-screen touch controls (see touch_input.c).
 */
#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include <stdbool.h>

#include <SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Handles an SDL event for the touch controls. Returns non-zero if the
 * event was used and should not reach the game. */
int handle_touch_inputs(SDL_Event *event);
/* Lays the buttons out again after the window / game area changed */
void reinit_touch_buttons(void);

typedef struct
{
	int index;
	int x, y, width, height; /* window pixels */
	SDL_Color color;         /* drawn outline */
	SDL_Color debug_color;   /* outline in debug draw mode */
	SDL_Keysym sdlkey;       /* key sent when pressed */
} touch_button;

extern touch_button fn_buttons[15];
extern touch_button arrow_buttons[4];
extern touch_button thrust_buttons[4];
/* Dropdown column, top right (the free camera is in the emulator menu) */
enum
{
	DD_MENU,  /* opens / closes the column */
	DD_PADS,  /* shows / hides the pads */
	DD_ZOOM_IN,
	DD_ZOOM_OUT,
	DD_P,
	DD_C,
	DD_SETTINGS, /* the emulator menu */
	DD_COUNT
};
extern touch_button dropdown_buttons[DD_COUNT];
extern touch_button pause_button;
extern touch_button shoot_button;

typedef struct
{
	int base_x, base_y; /* centre of the joystick */
	int radius;
	int knob_x, knob_y; /* current knob position */
	int active;         /* being touched */
	float dx, dy;       /* direction, -1..1 */
} VirtualJoystick;

extern VirtualJoystick vjoy;

/* Which pads are shown: worked out from the game's current screen by
 * touch_update_pads() */
extern int toggle_arrow_keys_touch;
extern int toggle_thrust_keys_touch;
extern int toggle_dropdown_keys_touch;

void touch_update_pads(void);
/* Once a frame: repeats held buttons */
void touch_input_tick(void);

/* For drawing: what is held down / switched on right now */
int touch_held_arrow(void);    /* arrow_buttons index, -1 = none */
int touch_held_thrust(void);   /* thrust_buttons index, -1 = none */
int touch_held_dropdown(void); /* dropdown_buttons index, -1 = none */
bool touch_pads_hidden(void);
void touch_set_pads_hidden(bool hidden); /* the dropdown's d-pad button */
Uint32 touch_fire_time(void); /* SDL_GetTicks() of the last shot, 0 = never */

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_INPUT_H */
