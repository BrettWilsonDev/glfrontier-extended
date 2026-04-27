#ifdef USE_NK

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

#ifdef __EMSCRIPTEN__
#define NK_WEBGL_NO_MAP_BUFFER   // tell nuklear not to use glMapBufferRange
#endif

#include "nuklear.h"
#include "nuklear_sdl_gl3.h"

struct nk_context *nk_ctx = NULL;

/* Exported so render_nuklear.c can scale all pixel values consistently */
float nk_ui_scale = 1.0f;

void nuklear_init_sdl(SDL_Window *win)
{
    nk_ctx = nk_sdl_init(win);

    float ddpi, hdpi, vdpi;
    float scale = 1.0f;

    if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0) {
        scale = ddpi / 160.0f;          /* 160 = Android's "baseline" mdpi */
        if (scale < 1.0f) scale = 1.0f; /* never shrink below 1x */
        if (scale > 3.0f) scale = 3.0f; /* cap for xxxhdpi screens */
    }

    nk_ui_scale = scale;

    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);

    struct nk_font *font =
        nk_font_atlas_add_default(atlas, 13.0f * scale, NULL);

    nk_sdl_font_stash_end();

    nk_style_set_font(nk_ctx, &font->handle);

    nk_ctx->style.window.spacing    = nk_vec2(4.0f * scale, 6.0f * scale);
    nk_ctx->style.window.padding    = nk_vec2(8.0f * scale, 8.0f * scale);
    // nk_ctx->style.button.padding    = nk_vec2(8.0f * scale, 10.0f * scale); /* taller touch targets */
    nk_ctx->style.edit.row_padding  = 8.0f * scale;
}


void nuklear_clean_up(void)
{
    nk_sdl_shutdown();
}

#endif /* WITH_GL */

#endif // USE_NK