/*
 * input.h - keyboard and mouse state handed to the emulated game.
 */
#ifndef INPUT_H
#define INPUT_H

#include "main.h"

#define SIZE_KEYBUF 16

typedef struct
{
	unsigned char key_buf[SIZE_KEYBUF]; /* ST scancodes waiting for the game */
	int buf_head, buf_tail;
	int cur_mousebut_state; /* bit 0 right, bit 1 left (ST order) */

	int motion_x, motion_y; /* mouse movement since the game last polled */
	int abs_x, abs_y;       /* pointer position, window pixels */
} CINPUT;

extern CINPUT input;

/* Queue an ST key press / release */
void Input_PressSTKey(unsigned char scancode, BOOL press);
void Input_MousePress(int sdl_button);
void Input_MouseRelease(int sdl_button);

/* Host calls: the game polls the mouse and keyboard */
void Call_GetMouseInput(void);
void Call_GetKeyboardEvent(void);

#endif /* INPUT_H */
