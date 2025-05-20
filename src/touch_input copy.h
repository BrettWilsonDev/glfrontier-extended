// #ifndef TOUCHCONTROLS_H
// #define TOUCHCONTROLS_H

// #include <SDL.h>

// extern void handle_touch_inputs(SDL_Event *event);
// extern int fn_button_pressed(SDL_Event *event);
// extern int pause_button_pressed(SDL_Event *event);
// extern int touch_grab_mouse_button(SDL_Event *event);
// extern void update_virtual_joystick(int x, int y);
// extern void handle_virtual_joystick(SDL_Event *event);

// typedef struct
// {
//     int index;
//     int x; // x position of the button
//     int y; // y position of the button
//     int width; // width of the button
//     int height; // height of the button
//     SDL_Color color; // color of the release view
//     SDL_Color debug_color; // color of the debug view
//     int times_pressed; // number of times the button has been pressed
//     int times_pressed_max; // max number of times the button can be pressed
//     int active; // is the button being touched (bool)
//     SDL_Keysym sdlkey; // SDL_Keysym of the button*
// } touch_button;

// extern touch_button fn_buttons[9];
// extern touch_button pause_button;

// typedef struct {
//     int base_x, base_y;     // center of the joystick base
//     int radius;             // radius of joystick base
//     int knob_x, knob_y;     // current knob position
//     int active;             // is joystick being touched
//     float dx, dy;           // direction vector (-1 to 1)
// } VirtualJoystick;

// extern VirtualJoystick vjoy;

// static int vjoy_mouse_down = 0;

// extern int toggle_arrow_keys_touch;

// #endif // TOUCHCONTROLS_H