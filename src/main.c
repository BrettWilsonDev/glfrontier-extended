/*
  Hatari - main.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Main initialization and event handling routines.
*/

#include <time.h>
#include <signal.h>
#include <signal.h>

#include <SDL.h>

#include "main.h"
#include "audio.h"
#include "../m68000.h"
#include "hostcall.h"
#include "input.h"
#include "keymap.h"
#include "renderer.h"
#include "shortcut.h"

#include "touch_input.h"

#define FORCE_WORKING_DIR /* Set default directory to cwd */

BOOL bQuitProgram = FALSE; /* Flag to quit program cleanly */
BOOL bUseFullscreen = FALSE;
BOOL bEmulationActive = TRUE; /* Run emulation when started */
BOOL bAppActive = FALSE;
char szBootDiscImage[MAX_FILENAME_LENGTH] = {""};

char szWorkingDir[MAX_FILENAME_LENGTH] = {""};
char szCurrentDir[MAX_FILENAME_LENGTH] = {""};

bool toggleRightClick = FALSE; // used to toggle the mouse grab
bool toggleTouchControls = FALSE;

/*-----------------------------------------------------------------------*/
/*
  Error handler
*/
void Main_SysError(char *Error, char *Title)
{
	fprintf(stderr, "%s : %s\n", Title, Error);
}

/*-----------------------------------------------------------------------*/
/*
  Bring up message(handles full-screen as well as Window)
*/
int Main_Message(char *lpText, char *lpCaption /*,unsigned int uType*/)
{
	int Ret = 0;

	/* Show message */
	fprintf(stderr, "%s: %s\n", lpCaption, lpText);

	return (Ret);
}

/*-----------------------------------------------------------------------*/
/*
  Pause emulation, stop sound
*/
void Main_PauseEmulation(void)
{
	if (bEmulationActive)
	{
		Audio_EnableAudio(FALSE);
		bEmulationActive = FALSE;
	}
}

/*-----------------------------------------------------------------------*/
/*
  Start emulation
*/
void Main_UnPauseEmulation(void)
{
	if (!bEmulationActive)
	{
		Audio_EnableAudio(1);
		bEmulationActive = TRUE;
	}
}

// hacky fix for the system map view left click glitch in the original game
int systemview_button(SDL_Event *event) // only called if touch input is not active
{
	if (event->button.button == SDL_BUTTON_LEFT)
	{
		int x = event->button.x;
		int y = event->button.y;
		const int HitRegionSize = 30;

		if (x >= 33 && x <= 33 + HitRegionSize && y >= screen_h - HitRegionSize && y <= screen_h)
		{
			SDL_Keysym sdlkey = {.scancode = SDL_SCANCODE_F2, .sym = SDLK_F2};

			Keymap_KeyDown(&sdlkey);
			Keymap_KeyUp(&sdlkey);
			return 1;
		}
	}
	return 0;
}

/* ----------------------------------------------------------------------- */
/*
  Message handler
  Here we process the SDL events (keyboard, mouse, ...) and map it to
  Atari IKBD events.
*/
void Main_EventHandler()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		// Toggle touch controls on or off based if there is touch input
		if ((event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) && toggleTouchControls != 1)
		{
			toggleTouchControls = 1;
		}

		// Handle virtual joystick in one place
		if (toggleTouchControls)
		{
			handle_touch_inputs(&event);
		}

		switch (event.type)
		{
		case SDL_QUIT:
			bQuitProgram = TRUE;
			SDL_Quit();
			exit(0);
			break;

		case SDL_MOUSEMOTION:
			input.motion_x += event.motion.xrel;
			input.motion_y += event.motion.yrel;
			input.abs_x = event.motion.x;
			input.abs_y = event.motion.y;
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (toggleTouchControls == 0)
			{
				// hacky fix for the system map view left click
				if (systemview_button(&event))
				{
					break;
				}
			}
			else
			{
				if (touch_buttons_pressed(&event))
				{
					break;
				}
			}
			Input_MousePress(event.button.button);
			break;

		case SDL_MOUSEBUTTONUP:
			Input_MouseRelease(event.button.button);
			break;

		case SDL_KEYDOWN:
			if ((event.key.keysym.sym == SDLK_m) &&
				(event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
			{
				toggleRightClick = !toggleRightClick;

				if (toggleRightClick)
					Input_MousePress(SDL_BUTTON_RIGHT);
				else
					Input_MouseRelease(SDL_BUTTON_RIGHT);
			}
			Keymap_KeyDown(&event.key.keysym);
			break;

		case SDL_KEYUP:
			Keymap_KeyUp(&event.key.keysym);
			break;
		}
	}

	Input_Update();
}

/*-----------------------------------------------------------------------*/
/*
  Check for any passed parameters
*/
void Main_ReadParameters(int argc, char *argv[])
{
	int i;

	/* Scan for any which we can use */
	for (i = 1; i < argc; i++)
	{
		if (strlen(argv[i]) > 0)
		{
			if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
			{
				printf("Usage:\n frontier [options]\n"
					   "Where options are:\n"
					   "  --help or -h          Print this help text and exit.\n"
					   "  --fullscreen or -f    Try to use fullscreen mode.\n"
					   "  --nosound             Disable sound (faster!).\n"
					   "  --size w h            Start at specified window size.\n");
				exit(0);
			}
			else if (!strcmp(argv[i], "--fullscreen") || !strcmp(argv[i], "-f"))
			{
				bUseFullscreen = TRUE;
			}
			else if (!strcmp(argv[i], "--nosound"))
			{
				bDisableSound = TRUE;
			}
			else if (!strcmp(argv[i], "--size"))
			{
				screen_h = 0;
				if (++i < argc)
					screen_w = atoi(argv[i]);
				if (++i < argc)
					screen_h = atoi(argv[i]);
				/* fe2 likes 1.6 aspect ratio until i fix the mouse position
				 * to 3d object position code... */
				if (screen_h == 0)
					screen_h = 5 * screen_w / 8;
			}
			else
			{
				/* some time make it possible to read alternative
				 * names for fe2.bin from command line */
				fprintf(stderr, "Illegal parameter: %s\n", argv[i]);
			}
		}
	}
}

/*-----------------------------------------------------------------------*/
/*
  Initialise emulation
*/
void Main_Init(void)
{
	/* Init SDL's video subsystem. Note: Audio and joystick subsystems
	   will be initialized later (failures there are not fatal). */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		fprintf(stderr, "Could not initialize the SDL library:\n %s\n", SDL_GetError());
		exit(-1);
	}

	Screen_Init();
	Init680x0(); /* Init CPU emulation */
	Audio_Init();
	Keymap_Init();

	if (bQuitProgram)
	{
		SDL_Quit();
		exit(-2);
	}
}

/*-----------------------------------------------------------------------*/
/*
  Un-Initialise emulation
*/
void Main_UnInit(void)
{
	Audio_UnInit();
	Screen_UnInit();

	/* SDL uninit: */
	SDL_Quit();
}

static Uint32 vbl_callback(Uint32 interval, void *param)
{
	FlagException(0);
	return interval;
}

void sig_handler(int signum)
{
	if (signum == SIGSEGV)
	{
		printf("Segfault! All is lost! Abandon ship!\n");
		Call_DumpDebug();
		abort();
	}
}

/*-----------------------------------------------------------------------*/
/*
  Main
*/
int main(int argc, char *argv[])
{
	signal(SIGSEGV, sig_handler);

	/* Generate random seed */
	srand(time(NULL));

	/* Check for any passed parameters */
	Main_ReadParameters(argc, argv);

	/* Init emulator system */
	Main_Init();

	/* Switch immediately to fullscreen if user wants to */
	if (bUseFullscreen)
		Screen_ToggleFullScreen();

	// acts as a game loop of sorts 20ms is the speed of the original game
	SDL_AddTimer(20, &vbl_callback, NULL);

	/* Run emulation */
	Main_UnPauseEmulation();
	Start680x0(); /* Start emulation */

	/* Un-init emulation system */
	Main_UnInit();

	return (0);
}
