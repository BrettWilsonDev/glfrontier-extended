#ifndef RENDER_NUKLEAR_H_
#define RENDER_NUKLEAR_H_

#include "SDL.h"

void render_nuklear();

extern void nuklear_set_sdl_screen_data(SDL_Window *sdlWindowParam, SDL_Renderer *sdlRendererParam);

extern bool toggle_debug_draw;

#endif /* RENDER_NUKLEAR_H_ */