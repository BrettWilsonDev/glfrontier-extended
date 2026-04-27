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

#include "nuklear_impl.h"
#ifdef WITH_GL
#include "nuklear_sdl_gl3_impl.h"
#else
#include "nuklear_sdl_impl.h"
#endif

// #include "glad/glad.h"

#define FORCE_WORKING_DIR /* Set default directory to cwd */

BOOL bQuitProgram = FALSE; /* Flag to quit program cleanly */
BOOL bUseFullscreen = FALSE;
BOOL bEmulationActive = TRUE; /* Run emulation when started */
BOOL bAppActive = FALSE;
char szBootDiscImage[MAX_FILENAME_LENGTH] = {""};

char szWorkingDir[MAX_FILENAME_LENGTH] = {""};
char szCurrentDir[MAX_FILENAME_LENGTH] = {""};

bool toggle_right_click = FALSE; // used to toggle the mouse grab
bool toggle_touch_controls = FALSE;
bool toggle_m68k_menu = FALSE;
// bool toggle_m68k_menu = TRUE;
int dump_m68k_toggle = 0;
bool toggle_fps_draw = FALSE;

int emulation_speed = 20;

char *clslog = NULL;
static size_t clslog_size = 0;

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG "FE2"
#endif

void log_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	va_list args_copy;
	va_copy(args_copy, args);
	vprintf(fmt, args);

#ifdef ANDROID
	va_list args_logcat;
	va_copy(args_logcat, args_copy);
	__android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, fmt, args_logcat);
	va_end(args_logcat);
#endif

	static char log_buffer[1024];
	vsnprintf(log_buffer, sizeof(log_buffer), fmt, args_copy);
	va_end(args_copy);
	va_end(args);

	size_t message_len = strlen(log_buffer);
	size_t new_size = clslog_size + message_len + 2;
	char *new_clslog = realloc(clslog, new_size);
	if (!new_clslog)
	{
		printf("Error: unable to allocate memory for clslog\n");
		return;
	}
	clslog = new_clslog;
	if (clslog_size == 0)
		clslog[0] = '\0';
	strcat(clslog, log_buffer);
	strcat(clslog, "\n");
	clslog_size = new_size;
}

/*-----------------------------------------------------------------------*/
/*
  Error handler
*/
void Main_SysError(char *Error, char *Title)
{
	// log_printf(stderr, "%s : %s\n", Title, Error);
	log_printf("%s : %s\n", Title, Error);
}

/*-----------------------------------------------------------------------*/
/*
  Bring up message(handles full-screen as well as Window)
*/
int Main_Message(char *lpText, char *lpCaption /*,unsigned int uType*/)
{
	int Ret = 0;

	/* Show message */
	// log_printf(stderr, "%s: %s\n", lpCaption, lpText);
	log_printf("%s: %s\n", lpCaption, lpText);

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

int hit_region_clicked(SDL_Event *event, int x1, int y1, int x2, int y2)
{
	int lb_ox = Screen_GetGameOffsetX();
	int lb_oy = Screen_GetGameOffsetY();
	int lb_h = Screen_GetGameHeight();
	int lb_w = lb_h * 320 / 240;

	int gx = 320 * (event->button.x - lb_ox) / lb_w;
	int gy = 240 * (event->button.y - lb_oy) / lb_h;

	return gx >= x1 && gx <= x2 && gy >= y1 && gy <= y2;
}

/* Hacky fix for the system map view left click glitch in the original game.
 * Hit test is done in game coords (0..320, 0..240) so it works at any
 * window size with no offset/scale fiddling. */
static int systemview_button(SDL_Event *event)
{
	if (event->button.button != SDL_BUTTON_LEFT)
		return 0;

	if (hit_region_clicked(event, 18, 226, 31, 240))
	{
		SDL_Keysym sdlkey = {.scancode = SDL_SCANCODE_F2, .sym = SDLK_F2};
		Keymap_KeyDown(&sdlkey);
		Keymap_KeyUp(&sdlkey);
		return 1;
	}
	return 0;
}

static int settings_button(SDL_Event *event)
{
	if (event->button.button != SDL_BUTTON_LEFT)
		return 0;

	if (hit_region_clicked(event, 0, 0, 10, 14))
	{
		toggle_m68k_menu = !toggle_m68k_menu;
		return 1;
	}
	return 0;
}

/* ----------------------------------------------------------------------- */
/*
  Message handler
  Here we process the SDL events (keyboard, mouse, ...) and map it to
  Atari IKBD events.
*/
void Main_EventHandler(void)
{
	SDL_Event event;
#ifdef USE_NK
	nk_input_begin(nk_ctx);
#endif

	while (SDL_PollEvent(&event))
	{
#ifdef USE_NK
		nk_sdl_handle_event(&event);
#endif

		if ((event.type == SDL_FINGERDOWN ||
			 event.type == SDL_FINGERUP ||
			 event.type == SDL_FINGERMOTION) &&
			!toggle_touch_controls)
			toggle_touch_controls = 1;

		if (toggle_touch_controls)
		{
			if (handle_touch_inputs(&event))
				continue; /* event consumed by touch UI — skip emulator mouse handling */
		}

		switch (event.type)
		{
		case SDL_QUIT:
			bQuitProgram = TRUE;
			SDL_Quit();
			exit(0);
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
				event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				screen_w = event.window.data1;
				screen_h = event.window.data2;
				call_update_letterbox();
			}
			break;

		case SDL_MOUSEMOTION:
			/* Store raw physical pixels. input.c scales using letterbox dims. */
			input.motion_x += event.motion.xrel;
			input.motion_y += event.motion.yrel;
			input.abs_x = event.motion.x;
			input.abs_y = event.motion.y;
			break;

		case SDL_MOUSEBUTTONDOWN:
			input.abs_x = event.button.x;
			input.abs_y = event.button.y;

			if (!toggle_touch_controls && !toggle_m68k_menu)
			{
				if (settings_button(&event))
				{
					break;
				}
			}

			if (!systemview_button(&event))
			{
				Input_MousePress(event.button.button);
				break;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			input.abs_x = event.button.x;
			input.abs_y = event.button.y;
			Input_MouseRelease(event.button.button);
			break;

		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_m &&
				(event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
			{
				toggle_right_click = !toggle_right_click;
				if (toggle_right_click)
					Input_MousePress(SDL_BUTTON_RIGHT);
				else
					Input_MouseRelease(SDL_BUTTON_RIGHT);
			}
			if (event.key.keysym.sym == SDLK_f &&
				(event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)))
				toggle_m68k_menu = !toggle_m68k_menu;
			Keymap_KeyDown(&event.key.keysym);
			break;

		case SDL_KEYUP:
			Keymap_KeyUp(&event.key.keysym);
			break;
		}
	}

#ifdef USE_NK
	nk_input_end(nk_ctx);
#endif
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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

/*-----------------------------------------------------------------------*/
/*
  Initialise emulation
*/
void Main_Init(void)
{
#ifdef __EMSCRIPTEN__
	// EM_ASM(
	// 	FS.mkdir('/saves');
	// 	FS.mount(IDBFS, {}, '/saves');
	// 	FS.syncfs(true, function(err) {
	//         if (err) console.log('FS init error:', err); }););

	EM_ASM(
		// Create the saves directory
		FS.mkdir('/saves');

		// Mount IDBFS to the saves directory for persistence
		FS.mount(IDBFS, {}, '/saves');

		// Sync from IndexedDB to populate existing saves
		FS.syncfs(true, function(err) {
            if (err) {
                console.log('FS init error:', err);
            } else {
                console.log('Save data loaded from persistent storage');
            } }););
#endif

	/* Init SDL's video subsystem. Note: Audio and joystick subsystems
	   will be initialized later (failures there are not fatal). */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		// log_printf(stderr, "Could not initialize the SDL library:\n %s\n", SDL_GetError());
		log_printf("Could not initialize the SDL library:\n %s\n", SDL_GetError());
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
	interval = emulation_speed;

	FlagException(0);
	return interval;
}

void sig_handler(int signum)
{
	if (signum == SIGSEGV)
	{
		log_printf("Segfault! All is lost! Abandon ship!\n");
		Call_DumpDebug();
		// abort();
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

	log_printf("Logger started: %s\n", __TIME__);

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