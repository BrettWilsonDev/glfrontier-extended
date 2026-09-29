/*
 * shortcut.c - Ctrl+key emulator shortcuts:
 *   Ctrl-F11 fullscreen, Ctrl-M mouse grab, Ctrl-Q quit,
 *   Ctrl-D debug dump, Ctrl-E cycle renderer.
 */
#include <SDL.h>

#include "hostcall.h"
#include "renderer.h"
#include "shortcut.h"

SHORTCUT_KEY ShortCutKey;

static void toggle_mouse_grab(void)
{
	bGrabMouse = !bGrabMouse;
	if (!bInFullScreen)
		SDL_SetRelativeMouseMode(bGrabMouse ? SDL_TRUE : SDL_FALSE);
}

void ShortCut_CheckKeys(void)
{
	switch (ShortCutKey.Key)
	{
	case SDLK_F11:
		Screen_ToggleFullScreen();
		break;
	case SDLK_m:
		toggle_mouse_grab();
		break;
	case SDLK_q:
		bQuitProgram = TRUE;
		SDL_Quit();
		exit(0);
	case SDLK_d:
		Call_DumpDebug();
		break;
	case SDLK_e:
		Screen_ToggleRenderer();
		break;
	default:
		break;
	}
	memset(&ShortCutKey, 0, sizeof(ShortCutKey));
}
