#ifndef WITH_GL

// This makes use of SDL the internal sdl renderer to draw the software renderer.

#include <SDL.h>
// #include <SDL_image.h>

#include "main.h"
#include "../m68000.h"
#include "renderer.h"
#include "input.h"

#include "touch_input.h"

unsigned long VideoBase;	/* Base address in ST Ram for screen(read on each VBL) */
unsigned char *VideoRaster; /* Pointer to Video raster, after VideoBase in PC address space. Use to copy data on HBL */

int len_main_palette;
unsigned short MainPalette[256];
unsigned short CtrlPalette[16];
int fe2_bgcol;

unsigned int MainRGBPalette[256];
unsigned int CtrlRGBPalette[16];

unsigned long logscreen, logscreen2, physcreen, physcreen2;

static SDL_Window *sdlWindow = NULL;
static SDL_Renderer *sdlRenderer = NULL;
static SDL_Texture *sdlTexture = NULL;
// static SDL_Surface *cursor = NULL;

BOOL bGrabMouse = FALSE; /* Grab the mouse cursor in the window */
BOOL bInFullScreen = FALSE;

/* new stuff */
enum RENDERERS use_renderer = R_OLD;
/* mouse shown this frame? */
int mouse_shown = 0;

/*-----------------------------------------------------------------------*/
/*
  Set window size
*/
// int screen_w = 320;
// int screen_h = 200;

int screen_w = 640;
int screen_h = 480;

static void change_vidmode()
{
	if (sdlWindow == NULL)
	{
		sdlWindow = SDL_CreateWindow(PROG_NAME,
									 SDL_WINDOWPOS_CENTERED, // Center the window
									 SDL_WINDOWPOS_CENTERED,
									 screen_w, screen_h,
									 SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | (bInFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | SDL_WINDOW_ALLOW_HIGHDPI);
		if (!sdlWindow)
		{
			fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
			SDL_Quit();
			exit(-1);
		}

		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
		if (!sdlRenderer)
		{
			fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
			SDL_Quit();
			exit(-1);
		}

		// Create texture for the original game resolution (320x200)
		sdlTexture = SDL_CreateTexture(sdlRenderer,
									   SDL_PIXELFORMAT_RGB565, // TODO This could be SDL_PIXELFORMAT_ARGB8888 for better performance
									   SDL_TEXTUREACCESS_STREAMING,
									   320, 200);
		if (!sdlTexture)
		{
			fprintf(stderr, "Texture creation failed: %s\n", SDL_GetError());
			SDL_Quit();
			exit(-1);
		}
		SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_NONE);

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
		SDL_RenderSetLogicalSize(sdlRenderer, screen_w, screen_h);
	}
	else
	{
		SDL_SetWindowSize(sdlWindow, screen_w, screen_h);
		SDL_SetWindowFullscreen(sdlWindow, bInFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		SDL_RenderSetLogicalSize(sdlRenderer, screen_w, screen_h);
	}
}

void Screen_Init(void)
{
	SDL_ShowCursor(SDL_DISABLE);
	change_vidmode();

	/* Configure some SDL stuff: */
	SDL_SetWindowTitle(sdlWindow, PROG_NAME);
	SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
	SDL_ShowCursor(SDL_ENABLE);

	int windowW, windowH;
	SDL_GetWindowSize(sdlWindow, &windowW, &windowH);

	int drawableW, drawableH;
	SDL_GetRendererOutputSize(sdlRenderer, &drawableW, &drawableH);

	float scaleX = (float)drawableW / windowW;
	float scaleY = (float)drawableH / windowH;

	SDL_RenderSetScale(sdlRenderer, scaleX, scaleY);
}

void Screen_UnInit(void)
{
	if (sdlTexture)
	{
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = NULL;
	}
	if (sdlRenderer)
	{
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;
	}
	if (sdlWindow)
	{
		SDL_DestroyWindow(sdlWindow);
		sdlWindow = NULL;
	}
}

void Screen_ToggleFullScreen()
{
	bInFullScreen = !bInFullScreen;
	change_vidmode();
}

static const unsigned char font_bmp[] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0,
	0x80, 0x0, 0x0, 0x2, 0xa0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x50,
	0xf8, 0x50, 0x50, 0xf8, 0x50, 0x0, 0x0, 0x6, 0x20, 0xf0, 0xa0, 0xa0, 0xa0, 0xa0, 0xf0, 0x20,
	0x0, 0x5, 0x0, 0xc8, 0xd8, 0x30, 0x60, 0xd8, 0x98, 0x0, 0x0, 0x6, 0xa0, 0x0, 0xe0, 0xa0,
	0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2,
	0xc0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xc0, 0x0, 0x3, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0xc0, 0x0, 0x3, 0x0, 0x0, 0x20, 0xf8, 0x50, 0xf8, 0x20, 0x0, 0x0, 0x6, 0x0, 0x0,
	0x40, 0xe0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x80,
	0x0, 0x2, 0x0, 0x0, 0x0, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x80, 0x0, 0x0, 0x2, 0x0, 0x8, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x0, 0x0, 0x6,
	0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x40, 0xc0, 0x40, 0x40, 0x40, 0x40,
	0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0xe0, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20,
	0x20, 0xe0, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0x80, 0x80, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0x0,
	0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0,
	0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x0, 0x0, 0x4,
	0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0xa0, 0xa0, 0xe0, 0x20, 0x20,
	0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x80, 0x0, 0x80, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0,
	0x0, 0x80, 0x0, 0x0, 0x80, 0x80, 0x0, 0x2, 0xe0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0x0,
	0x0, 0x4, 0x0, 0x0, 0xe0, 0x0, 0xe0, 0x0, 0x0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xe0, 0xa0,
	0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0xe0, 0x80, 0x0, 0x80, 0x0, 0x0, 0x4,
	0xfe, 0x82, 0xba, 0xa2, 0xba, 0x82, 0xfe, 0x0, 0x0, 0x8, 0xf0, 0x90, 0x90, 0x90, 0xf0, 0x90,
	0x90, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0xf8, 0x88, 0x88, 0xf8, 0x0, 0x0, 0x6, 0xe0, 0x80,
	0x80, 0x80, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xf8, 0x48, 0x48, 0x48, 0x48, 0x48, 0xf8, 0x0,
	0x0, 0x6, 0xf0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0xf0, 0x0, 0x0, 0x5, 0xf0, 0x80, 0x80, 0xe0,
	0x80, 0x80, 0x80, 0x0, 0x0, 0x4, 0xf0, 0x80, 0x80, 0x80, 0xb0, 0x90, 0xf0, 0x0, 0x0, 0x5,
	0x90, 0x90, 0x90, 0xf0, 0x90, 0x90, 0x90, 0x0, 0x0, 0x5, 0xe0, 0x40, 0x40, 0x40, 0x40, 0x40,
	0xe0, 0x0, 0x0, 0x4, 0xf0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0x90, 0xb0,
	0xe0, 0xc0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xe0, 0x0,
	0x0, 0x4, 0x88, 0xd8, 0xf8, 0xa8, 0x88, 0x88, 0x88, 0x0, 0x0, 0x6, 0x90, 0xd0, 0xf0, 0xb0,
	0x90, 0x90, 0x90, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0x90, 0x90, 0x90, 0xf0, 0x0, 0x0, 0x5,
	0xf0, 0x90, 0x90, 0xf0, 0x80, 0x80, 0x80, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0x90, 0x90, 0xb0,
	0xf0, 0x18, 0x0, 0x5, 0xf0, 0x90, 0x90, 0xf0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0xf0, 0x80,
	0x80, 0xf0, 0x10, 0x10, 0xf0, 0x0, 0x0, 0x5, 0xe0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x0,
	0x0, 0x3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xf0, 0x0, 0x0, 0x5, 0x90, 0x90, 0x90, 0xb0,
	0xe0, 0xc0, 0x80, 0x0, 0x0, 0x5, 0x88, 0x88, 0x88, 0xa8, 0xf8, 0xd8, 0x88, 0x0, 0x0, 0x6,
	0x88, 0xd8, 0x70, 0x20, 0x70, 0xd8, 0x88, 0x0, 0x0, 0x6, 0x90, 0x90, 0x90, 0xf0, 0x20, 0x20,
	0x20, 0x0, 0x0, 0x5, 0xf0, 0x10, 0x30, 0x60, 0xc0, 0x80, 0xf0, 0x0, 0x0, 0x5, 0xa0, 0x0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x80, 0xc0, 0x60, 0x30, 0x18, 0x8, 0x0,
	0x0, 0x6, 0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0, 0x80, 0x80, 0x4, 0xe0, 0xa0, 0xe0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x0, 0x6,
	0xa0, 0x0, 0xe0, 0x20, 0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xe0, 0x20, 0xe0, 0xa0,
	0xe0, 0x0, 0x0, 0x4, 0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0,
	0xc0, 0x80, 0x80, 0x80, 0xc0, 0x0, 0x0, 0x3, 0x20, 0x20, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0,
	0x0, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0x80, 0x80, 0xc0,
	0x80, 0x80, 0x80, 0x0, 0x0, 0x3, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x20, 0xe0, 0x4,
	0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0x80, 0x0, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x0, 0x0, 0x2, 0x40, 0x0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xc0, 0x0, 0x3, 0x80, 0x80,
	0xb0, 0xe0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0,
	0x0, 0x2, 0x0, 0x0, 0xf8, 0xa8, 0xa8, 0xa8, 0xa8, 0x0, 0x0, 0x6, 0x0, 0x0, 0xe0, 0xa0,
	0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4,
	0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x80, 0x80, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x20, 0x30, 0x4, 0x0, 0x0, 0xc0, 0x80, 0x80, 0x80, 0x80, 0x0, 0x0, 0x3, 0x0, 0x0,
	0xc0, 0x80, 0xc0, 0x40, 0xc0, 0x0, 0x0, 0x3, 0x80, 0x80, 0xc0, 0x80, 0x80, 0x80, 0xc0, 0x0,
	0x0, 0x3, 0x0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xa0, 0xa0,
	0xe0, 0xc0, 0x80, 0x0, 0x0, 0x4, 0x0, 0x0, 0x88, 0xa8, 0xf8, 0xd8, 0x88, 0x0, 0x0, 0x6,
	0x0, 0x0, 0xa0, 0xe0, 0x40, 0xe0, 0xa0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x20, 0xe0, 0x4, 0x0, 0x0, 0xf0, 0x30, 0x60, 0xc0, 0xf0, 0x0, 0x0, 0x5, 0x81, 0x8d,
	0xe1, 0xa0, 0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x9, 0x2, 0x1a, 0xc2, 0x80, 0xc0, 0x40, 0xc0, 0x0,
	0x0, 0x8, 0xfe, 0xfc, 0xf8, 0xfc, 0xfe, 0xdf, 0x8e, 0x4, 0x0, 0x7, 0x7f, 0x3f, 0x1f, 0x3f,
	0x7f, 0xfb, 0x71, 0x20, 0x0, 0x8, 0x4, 0x8e, 0xdf, 0xfe, 0xfc, 0xf8, 0xfc, 0xfe, 0x0, 0x8,
	0x20, 0x71, 0xfb, 0x7f, 0x3f, 0x1f, 0x3f, 0x7f, 0x0, 0x7, 0xff, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0xff, 0x0, 0x9, 0x0, 0x0, 0xe0, 0x80, 0x80, 0x80, 0xe0, 0x40, 0xc0, 0x4, 0x60, 0x0,
	0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0,
	0x0, 0x4, 0x40, 0xa0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x0, 0x0, 0x4, 0x40, 0xa0, 0xe0, 0x20,
	0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x40, 0xa0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4,
	0x40, 0xa0, 0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xe0, 0x20, 0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0xa0,
	0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0xc0, 0xa0, 0xa0, 0xc0, 0xa0, 0xa0, 0xc0, 0x0,
	0x0, 0x4, 0xe0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xc0, 0x0, 0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4,
	0xe0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0x80, 0x0, 0x0, 0x4};

static int DrawChar(int col, int xoffset, char *scrline, int chr)
{
	const char *font_pos;
	char *pix;
	int i;

	font_pos = (const char *)font_bmp;
	font_pos += (chr & 0xff) * 10;
	scrline += xoffset;

	if (xoffset < 0)
	{
		font_pos += 9;
		return xoffset + *font_pos;
	}

	for (i = 0; i < 8; i++, font_pos++, scrline += SCREENBYTES_LINE)
	{
		pix = scrline;
		if (xoffset > 319)
			continue;
		if (*font_pos & 0x80)
			*pix = col;
		pix++;
		if (xoffset + 1 > 319)
			continue;
		if (*font_pos & 0x40)
			*pix = col;
		pix++;
		if (xoffset + 2 > 319)
			continue;
		if (*font_pos & 0x20)
			*pix = col;
		pix++;
		if (xoffset + 3 > 319)
			continue;
		if (*font_pos & 0x10)
			*pix = col;
		pix++;
		if (xoffset + 4 > 319)
			continue;
		if (*font_pos & 0x8)
			*pix = col;
		pix++;
		if (xoffset + 5 > 319)
			continue;
		if (*font_pos & 0x4)
			*pix = col;
		pix++;
		if (xoffset + 6 > 319)
			continue;
		if (*font_pos & 0x2)
			*pix = col;
		pix++;
		if (xoffset + 7 > 319)
			continue;
		if (*font_pos & 0x1)
			*pix = col;
	}
	/* width of character */
	font_pos++;
	i = *font_pos;
	return xoffset + i;
}

#define MAX_QUEUED_STRINGS 200
struct QueuedString
{
	int x, y, col;
	unsigned char str[64];
} queued_strings[MAX_QUEUED_STRINGS];
int queued_string_pos;

void Nu_QueueDrawStr()
{
	assert(queued_string_pos < MAX_QUEUED_STRINGS);
	// strncpy(queued_strings[queued_string_pos].str, GetReg(REG_A0) + STRam, 64);
	strncpy((char *)queued_strings[queued_string_pos].str, (char *)(GetReg(REG_A0) + STRam), 64);
	// strncpy_s((char*)queued_strings[queued_string_pos].str, 64, GetReg(REG_A0) + STRam, 64);
	queued_strings[queued_string_pos].x = GetReg(REG_D1);
	queued_strings[queued_string_pos].y = GetReg(REG_D2);
	queued_strings[queued_string_pos++].col = GetReg(REG_D0);
}

int DrawStr(int xpos, int ypos, int col, unsigned char *str, bool shadowed)
{
	int x, y, chr;
	char *screen;

	x = xpos;
	y = ypos;

	if ((y > 192) || (y < 0))
		return x;
set_line:
	screen = LOGSCREEN2;
	screen += SCREENBYTES_LINE * y;

	while (*str)
	{
		chr = *(str++);

		if (chr < 0x1e)
		{
			if (chr == '\r')
			{
				y += 10;
				x = xpos;
				goto set_line;
			}
			else if (chr == 1)
				col = *(str++);
			continue;
		}
		else if (chr == 0x1e)
		{
			/* read new xpos */
			x = *(str++);
			x *= 2;
			continue;
		}
		else if (chr < 0x20)
		{
			/* Read new position */
			x = *(str++);
			x *= 2;
			y = *(str++);
			goto set_line;
		}

		if (shadowed)
		{
			DrawChar(0, x + 1, screen + SCREENBYTES_LINE, chr - 0x20);
		}
		x = DrawChar(col, x, screen, chr - 0x20);
	}

	return x;
}

void Screen_ToggleRenderer()
{
	use_renderer++;
	if (use_renderer >= R_MAX)
		use_renderer = 0;
}

// ======================================================================================================

void DrawThickCircleOutline(SDL_Renderer *renderer, int cx, int cy, int radius, int thickness, SDL_Color color)
{
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	const int segments = 64;

	for (int i = 0; i < segments; i++)
	{
		float theta = (float)i / segments * 2.0f * M_PI;
		int x = cx + (int)(radius * cosf(theta));
		int y = cy + (int)(radius * sinf(theta));

		// Draw a small filled rectangle/circle at each point
		SDL_Rect dot = {x - thickness / 2, y - thickness / 2, thickness, thickness};
		SDL_RenderFillRect(renderer, &dot);
	}
}

void RenderVirtualJoystick(SDL_Renderer *renderer)
{
	SDL_Color knobColor = {139, 137, 139, 255};

	// int baseThickness = 3;
	int knobThickness = 4;

	if (vjoy.active)
	{
		// Draw knob outline
		DrawThickCircleOutline(renderer, vjoy.knob_x, vjoy.knob_y, vjoy.radius / 2, knobThickness, knobColor);
	}
}

// Function to draw an arrow
void drawArrow(SDL_Renderer *renderer, int x, int y, int width, int height, SDL_Color color, int direction, int lineWidth)
{
	SDL_Rect square = {x, y, width, height};
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderDrawRect(renderer, &square);

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	// Use float for safe math
	float centerX = x + width / 2.0f;
	float centerY = y + height / 2.0f;
	float padding = fmaxf(2.0f, fminf(width, height) / 6.0f); // Never less than 2px

	float aw = fmaxf(3.0f, (width - 2 * padding) / 2.0f);  // Arrow half-width
	float ah = fmaxf(3.0f, (height - 2 * padding) / 2.0f); // Arrow half-height

	for (int i = -lineWidth / 2; i <= lineWidth / 2; i++)
	{
		float offset = (float)i;

		switch (direction)
		{
		case 0: // Up
			SDL_RenderDrawLineF(renderer, centerX + offset, y + height - padding, centerX - aw + offset, centerY);
			SDL_RenderDrawLineF(renderer, centerX + offset, y + height - padding, centerX + aw + offset, centerY);
			SDL_RenderDrawLineF(renderer, centerX - aw + offset, centerY, centerX + aw + offset, centerY);
			break;

		case 1: // Right
			SDL_RenderDrawLineF(renderer, x + padding, centerY + offset, centerX, centerY - ah + offset);
			SDL_RenderDrawLineF(renderer, x + padding, centerY + offset, centerX, centerY + ah + offset);
			SDL_RenderDrawLineF(renderer, centerX, centerY - ah + offset, centerX, centerY + ah + offset);
			break;

		case 2: // Down
			SDL_RenderDrawLineF(renderer, centerX + offset, y + padding, centerX - aw + offset, centerY);
			SDL_RenderDrawLineF(renderer, centerX + offset, y + padding, centerX + aw + offset, centerY);
			SDL_RenderDrawLineF(renderer, centerX - aw + offset, centerY, centerX + aw + offset, centerY);
			break;

		case 3: // Left
			SDL_RenderDrawLineF(renderer, x + width - padding + offset, centerY, centerX, centerY - ah + offset);
			SDL_RenderDrawLineF(renderer, x + width - padding + offset, centerY, centerX, centerY + ah + offset);
			SDL_RenderDrawLineF(renderer, centerX, centerY - ah + offset, centerX, centerY + ah + offset);
			break;
		}
	}
}

void drawThrustSymbol(SDL_Renderer *renderer, int x, int y, int width, int height, SDL_Color color, int direction, int lineWidth)
{
	SDL_Rect square = {x, y, width, height};
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // transparent outline
	SDL_RenderDrawRect(renderer, &square);

	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	int centerX = x + width / 2;
	int centerY = y + height / 2;
	int padding = 6; // a bit more padding so flames stay nicely inside

	// Make flames bigger:
	int baseFlameWidth = width / 4;	  // wider base
	int baseFlameLength = height / 2; // longer flames

	// Flicker effect lengths (varying sizes)
	int flameLengths[3] = {
		baseFlameLength,
		(int)(baseFlameLength * 0.8),
		(int)(baseFlameLength * 0.6)};

	// Smaller spacing to keep flames grouped nicely
	int spacing = baseFlameWidth / 1.5;

	for (int j = -1; j <= 1; ++j)
	{
		int flameLength = flameLengths[j + 1]; // pick different lengths

		SDL_Point points[4];

		switch (direction)
		{
		case 0: // UP
		{
			int fx = centerX + j * spacing;
			int fy = y + padding;

			points[0].x = fx - baseFlameWidth / 2;
			points[0].y = fy + flameLength;
			points[1].x = fx;
			points[1].y = fy;
			points[2].x = fx + baseFlameWidth / 2;
			points[2].y = fy + flameLength;
			break;
		}
		case 1: // RIGHT
		{
			int fx = x + width - padding - flameLength;
			int fy = centerY + j * spacing;

			points[0].x = fx;
			points[0].y = fy - baseFlameWidth / 2;
			points[1].x = fx + flameLength;
			points[1].y = fy;
			points[2].x = fx;
			points[2].y = fy + baseFlameWidth / 2;
			break;
		}
		case 2: // DOWN
		{
			int fx = centerX + j * spacing;
			int fy = y + height - padding - flameLength;

			points[0].x = fx - baseFlameWidth / 2;
			points[0].y = fy;
			points[1].x = fx;
			points[1].y = fy + flameLength;
			points[2].x = fx + baseFlameWidth / 2;
			points[2].y = fy;
			break;
		}
		case 3: // LEFT
		{
			int fx = x + padding;
			int fy = centerY + j * spacing;

			points[0].x = fx + flameLength;
			points[0].y = fy - baseFlameWidth / 2;
			points[1].x = fx;
			points[1].y = fy;
			points[2].x = fx + flameLength;
			points[2].y = fy + baseFlameWidth / 2;
			break;
		}
		}

		points[3] = points[0];
		SDL_RenderDrawLines(renderer, points, 4);
	}
}

void draw_touch_controls()
{
	RenderVirtualJoystick(sdlRenderer);

	if (toggle_arrow_keys_touch)
	{
		SDL_Color color = arrow_buttons[0].color;
		// Define the origin point (x, y) of the arrow group
		int originX = arrow_buttons[0].x;
		int originY = arrow_buttons[0].y;
		int size = arrow_buttons[0].width;

		// Define the offset of each arrow from the origin
		int arrowSpacing = arrow_buttons[0].height;

		// Draw the arrows relative to the origin
		drawArrow(sdlRenderer, originX, originY, size, size, color, 0, 2);				  // down arrow
		drawArrow(sdlRenderer, originX - arrowSpacing, originY, size, size, color, 1, 2); // left arrow
		drawArrow(sdlRenderer, originX, originY - arrowSpacing, size, size, color, 2, 2); // up arrow
		drawArrow(sdlRenderer, originX + arrowSpacing, originY, size, size, color, 3, 2); // right arrow
	}

	if (toggle_thrust_keys_touch)
	{
		SDL_Color color = thrust_buttons[0].color;
		// Define the origin point (x, y) of the arrow group
		int originX = thrust_buttons[0].x;
		int originY = thrust_buttons[0].y;
		int size = thrust_buttons[0].width;

		// Define the offset of each arrow from the origin
		int spacing = thrust_buttons[0].height;

		// Draw the arrows relative to the origin
		drawThrustSymbol(sdlRenderer, originX, originY, size, size, color, 0, 2);				   // down arrow
		drawThrustSymbol(sdlRenderer, originX - spacing, originY - (30), size, size, color, 1, 2); // left arrow
		drawThrustSymbol(sdlRenderer, originX, originY - spacing, size, size, color, 2, 2);		   // up arrow
		drawThrustSymbol(sdlRenderer, originX + spacing, originY - (30), size, size, color, 3, 2); // right arrow
																								   // }
	}
}

void draw_debug_blocks()
{
	for (int i = 0; i < sizeof(fn_buttons) / sizeof(fn_buttons[0]); i++)
	{
		SDL_SetRenderDrawColor(sdlRenderer, fn_buttons[i].debug_color.r, fn_buttons[i].debug_color.g, fn_buttons[i].debug_color.b, fn_buttons[i].debug_color.a);
		SDL_Rect blockRect = {fn_buttons[i].x, fn_buttons[i].y, fn_buttons[i].width, fn_buttons[i].height};
		SDL_RenderDrawRect(sdlRenderer, &blockRect);
	}

	SDL_SetRenderDrawColor(sdlRenderer, pause_button.debug_color.r, pause_button.debug_color.g, pause_button.debug_color.b, pause_button.debug_color.a);
	SDL_Rect pauseRect = {pause_button.x, pause_button.y, pause_button.width, pause_button.height};
	SDL_RenderDrawRect(sdlRenderer, &pauseRect);
}

static void draw_on_top_of_screen()
{
	if (toggleTouchControls)
		draw_touch_controls();
	// draw_debug_blocks();
}

// ======================================================================================================

static void draw_control_panel()
{
	// Draw text (unchanged)
	int temp = logscreen2;
	logscreen2 = physcreen;
	for (int i = 0; i < queued_string_pos; i++)
	{
		DrawStr(queued_strings[i].x, queued_strings[i].y, queued_strings[i].col, queued_strings[i].str, FALSE);
	}
	logscreen2 = temp;

	// Lock texture
	void *pixels;
	int pitch;
	if (SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch))
	{
		return;
	}

	uint16_t *dst = (uint16_t *)pixels;
	uint8_t *src = VideoRaster;
	// uint8_t *src;

	// Precompute RGB565 palettes ONCE (reuse MainRGBPalette)
	static uint16_t main_pal_rgb565[256], ctrl_pal_rgb565[16];
	for (int i = 0; i < 256; i++)
	{
		uint32_t col = MainRGBPalette[i];
		main_pal_rgb565[i] = ((col >> 8) & 0xF800) | ((col >> 5) & 0x07E0) | ((col >> 3) & 0x001F);
	}
	for (int i = 0; i < 16; i++)
	{
		uint32_t col = CtrlRGBPalette[i];
		ctrl_pal_rgb565[i] = ((col >> 8) & 0xF800) | ((col >> 5) & 0x07E0) | ((col >> 3) & 0x001F);
	}
	main_pal_rgb565[255] = 0x0000; // Force transparency

	// Process all lines
	for (int y = 0; y < 200; y++)
	{
		uint16_t *row = dst + y * (pitch / 2);
		const uint16_t *pal = (y < 168) ? main_pal_rgb565 : ctrl_pal_rgb565;

		// Process 4 pixels at a time (reduces loop overhead)
		for (int x = 0; x < 320; x += 4)
		{
			row[x + 0] = pal[src[y * 320 + x + 0]];
			row[x + 1] = pal[src[y * 320 + x + 1]];
			row[x + 2] = pal[src[y * 320 + x + 2]];
			row[x + 3] = pal[src[y * 320 + x + 3]];
		}
	}

	SDL_UnlockTexture(sdlTexture);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);

	draw_on_top_of_screen();
}

void BuildRGBPalette(unsigned int *rgb, unsigned short *st, int len)
{
	for (int i = 0; i < len; i++)
	{
		unsigned short st_col = st[i];
		unsigned int r = ((st_col >> 8) & 0x0f) * 17;
		unsigned int g = ((st_col >> 4) & 0x0f) * 17;
		unsigned int b = (st_col & 0x0f) * 17;
		rgb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
	}
}

void Nu_IsGLRenderer()
{
	SetReg(0, 0);
}

void Nu_DrawScreen()
{
	int y;

	BuildRGBPalette(MainRGBPalette, MainPalette, len_main_palette);
	BuildRGBPalette(CtrlRGBPalette, CtrlPalette, 16);

	y = logscreen2;
	logscreen2 = physcreen;
	logscreen2 = y;

	draw_control_panel();

	if (mouse_shown)
	{
		SDL_ShowCursor(SDL_ENABLE);
		mouse_shown = 0;
	}
	else
	{
		SDL_ShowCursor(SDL_DISABLE);
	}

	SDL_RenderPresent(sdlRenderer);
}

#endif