/*
 * settings.c - see settings.h.
 *
 * The file is plain "key=value" lines, unknown keys ignored, so it survives
 * settings being added or removed. Where it lives:
 *   desktop   user_config_dir(): Documents\My Games\GLFrontier on Windows,
 *             ~/.config/glfrontier on Linux (see user_dir.h)
 *   Android   the app's internal storage
 *   web       not kept yet (the save folder is only ready asynchronously)
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "freecam.h"
#include "main.h"
#include "mods.h"
#include "renderer.h"
#include "settings.h"
#include "touch_input.h"
#include "user_dir.h"

#define FILE_NAME "glfrontier.cfg"

typedef struct
{
	int keep_aspect;
	int renderer;
	int fullscreen;
	int freecam_invert_y;
	int freecam_show_help;
	int vanilla_saves;
	int emulation_speed;
	int touch_pads_hidden;
	int smoke_inherit;
	int smoke_cap;
	int mine_prox;
	int mine_radius;
} Settings;

static const struct
{
	const char *key;
	size_t offset;
} fields[] = {
	{"keep_aspect", offsetof(Settings, keep_aspect)},
	{"renderer", offsetof(Settings, renderer)},
	{"fullscreen", offsetof(Settings, fullscreen)},
	{"freecam_invert_y", offsetof(Settings, freecam_invert_y)},
	{"freecam_show_help", offsetof(Settings, freecam_show_help)},
	{"vanilla_saves", offsetof(Settings, vanilla_saves)},
	{"emulation_speed", offsetof(Settings, emulation_speed)},
	{"touch_pads_hidden", offsetof(Settings, touch_pads_hidden)},
	{"smoke_inherit", offsetof(Settings, smoke_inherit)},
	{"smoke_cap", offsetof(Settings, smoke_cap)},
	{"mine_prox", offsetof(Settings, mine_prox)},
	{"mine_radius", offsetof(Settings, mine_radius)},
};
#define N_FIELDS ((int)(sizeof(fields) / sizeof(fields[0])))

static Settings saved;
static bool loaded;

#ifndef __EMSCRIPTEN__
static const char *file_path(void)
{
	static char path[1024];
#ifdef ANDROID
	snprintf(path, sizeof(path), "%s/%s", SDL_AndroidGetInternalStoragePath(), FILE_NAME);
#else
	snprintf(path, sizeof(path), "%s%s", user_config_dir(), FILE_NAME);
#endif
	return path;
}

static FILE *settings_fopen(const char *path, const char *mode)
{
#ifdef ANDROID
	return fopen(path, mode);
#else
	return user_fopen(path, mode); /* UTF-8 paths on Windows */
#endif
}
#endif

static int *field(Settings *s, int i)
{
	return (int *)((char *)s + fields[i].offset);
}

static Settings current(void)
{
	Settings s;
	s.keep_aspect = Screen_GetLetterboxMode();
	s.renderer = (int)use_renderer;
	s.fullscreen = bInFullScreen ? 1 : 0;
	s.freecam_invert_y = freecam_invert_y;
	s.freecam_show_help = freecam_show_help;
	s.vanilla_saves = mods_vanilla_saves;
	s.emulation_speed = emulation_speed;
	s.touch_pads_hidden = touch_pads_hidden();
	s.smoke_inherit = smoke_inherit;
	s.smoke_cap = smoke_cap_ms;
	s.mine_prox = mine_prox;
	s.mine_radius = mine_radius_m;
	return s;
}

static void apply(const Settings *s)
{
	if (s->keep_aspect != Screen_GetLetterboxMode())
		Screen_SetLetterboxMode(s->keep_aspect != 0);
	if (s->renderer >= 0 && s->renderer < R_MAX)
		use_renderer = (enum RENDERERS)s->renderer;
#if !defined(ANDROID)
	if ((s->fullscreen != 0) != (bInFullScreen != 0))
		Screen_ToggleFullScreen();
#endif
	freecam_invert_y = s->freecam_invert_y != 0;
	freecam_show_help = s->freecam_show_help != 0;
	mods_vanilla_saves = s->vanilla_saves != 0;
	if (s->emulation_speed >= 1 && s->emulation_speed <= 100)
		emulation_speed = s->emulation_speed;
	touch_set_pads_hidden(s->touch_pads_hidden != 0);
	smoke_inherit = s->smoke_inherit != 0;
	if (s->smoke_cap >= 10 && s->smoke_cap <= 2000)
		smoke_cap_ms = s->smoke_cap;
	mine_prox = s->mine_prox != 0;
	if (s->mine_radius >= 10 && s->mine_radius <= 2000)
		mine_radius_m = s->mine_radius;
}

void settings_load(void)
{
	Settings s = current(); /* defaults for anything the file lacks */
	loaded = true;
#ifndef __EMSCRIPTEN__
	FILE *f = settings_fopen(file_path(), "r");
	if (f)
	{
		char line[128];
		while (fgets(line, sizeof(line), f))
		{
			char *eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq = 0;
			for (int i = 0; i < N_FIELDS; i++)
				if (!strcmp(line, fields[i].key))
					*field(&s, i) = atoi(eq + 1);
		}
		fclose(f);
		log_printf("Settings loaded from %s\n", file_path());
	}
#endif
	apply(&s);
	saved = current();
}

void settings_poll(void)
{
	if (!loaded)
		return;
	Settings now = current();
	if (!memcmp(&now, &saved, sizeof(now)))
		return;
	saved = now;
#ifndef __EMSCRIPTEN__
	FILE *f = settings_fopen(file_path(), "w");
	if (!f)
	{
		log_printf("Could not write settings to %s\n", file_path());
		return;
	}
	for (int i = 0; i < N_FIELDS; i++)
		fprintf(f, "%s=%d\n", fields[i].key, *field(&now, i));
	fclose(f);
#endif
}
