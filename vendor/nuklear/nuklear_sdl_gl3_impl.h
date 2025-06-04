#ifdef WITH_GL

#ifndef NUKLEAR_SDL_GL3_IMPL_H_
#define NUKLEAR_SDL_GL3_IMPL_H_

// #include <SDL.h>
#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

#include "nuklear_impl.h"

// extern struct nk_context *nk_ctx;

void nuklear_init_sdl(SDL_Window *win);
void nuklear_clean_up(void);

#endif /* NUKLEAR_SDL_GL3_IMPL_H_ */

#endif /* WITH_GL */