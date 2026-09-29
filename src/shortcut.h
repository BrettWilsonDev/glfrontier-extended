/*
 * shortcut.h - Ctrl+key emulator shortcuts (fullscreen, mouse grab, quit...).
 */
#ifndef SHORTCUT_H
#define SHORTCUT_H

#include "main.h"

typedef struct
{
	int Key; /* SDL keycode, 0 = none pending */
	BOOL bShiftPressed;
	BOOL bCtrlPressed;
} SHORTCUT_KEY;

/* Set by the keymap when a Ctrl+key combination is pressed */
extern SHORTCUT_KEY ShortCutKey;

/* Runs the pending shortcut, if any (called regularly by the game loop) */
void ShortCut_CheckKeys(void);

#endif /* SHORTCUT_H */
