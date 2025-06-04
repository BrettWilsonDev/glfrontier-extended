#ifdef WITH_GL

#include <stdint.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

struct nk_context *nk_ctx = NULL;

void nuklear_init_sdl(SDL_Window *win)
{
    nk_ctx = nk_sdl_init(win);
    // Set default font
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    struct nk_font *font = nk_font_atlas_add_default(atlas, 13.0f, NULL);
    nk_sdl_font_stash_end();
    nk_style_set_font(nk_ctx, &font->handle);
}

void nuklear_clean_up(void)
{
    nk_sdl_shutdown();
}

#endif /* WITH_GL */