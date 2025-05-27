#ifndef NUKLEAR_SDL_IMPL_H_
#define NUKLEAR_SDL_IMPL_H_

// #include <SDL.h>
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

extern struct nk_context *nk_ctx;

void nuklear_init_sdl(SDL_Window *win, SDL_Renderer *renderer);
void nuklear_clean_up(void);

#endif /* NUKLEAR_SDL_IMPL_H_ */
