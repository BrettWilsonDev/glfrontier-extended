#include "../m68000.h"
#include "main.h"
#include "keymap.h"
#include "input.h"
#include "touch_input.h"

#include "render_nuklear.h"
#ifdef WITH_GL
#include "nuklear_sdl_gl3_impl.h"
#else
#include "nuklear_sdl_impl.h"
#endif

#ifndef NK_TEXT_WRAP
#define NK_TEXT_WRAP 0x10
#endif

int dump_m68k_toggle = 0;
int inject_cash_value = 0;
int cash_value[4] = {0, 0, 0, 0};

bool toggle_debug_draw = FALSE;

static SDL_Window *sdlWindow;
static SDL_Renderer *sdlRenderer;

void nuklear_set_sdl_screen_data(SDL_Window *sdlWindowParam, SDL_Renderer *sdlRendererParam)
{
    sdlWindow = sdlWindowParam;
    sdlRenderer = sdlRendererParam;
}

enum section
{
    HOME,
    ABOUT,
    DEBUG,
    DEBUG_SETTINGS,
    CHEATS,
};

static enum section current_section = HOME;

void nk_text_multiline(struct nk_context *ctx, const char *text)
{
    const char *line_start = text;
    const char *p = text;

    while (*p)
    {
        if (*p == '\n')
        {
            int len = p - line_start;
            nk_layout_row_dynamic(nk_ctx, 15, 1); // 0 = auto height
            nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);

            p++; // skip newline
            line_start = p;
        }
        else
        {
            p++;
        }
    }

    // Draw the last line
    if (p != line_start)
    {
        int len = p - line_start;
        nk_layout_row_dynamic(nk_ctx, 15, 1); // 0 = auto height
        nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);
    }
}

void display_about(void)
{
    current_section = ABOUT;

    const char *text =
        "Original author:\n"
        "Tom Morton.\n"
        "Extended by\n"
        "Brett Wilson.\n\n"
        "Libraries used:\n"
        "SDL2, physFs,\n"
        "minivorbis, glutess\n"
        "and nuklear.\n\n"
        "Acknowledgments:\n"
        "Hatari emulator\n"
        "Pcercuei, Kochise.\n";

    nk_text_multiline(nk_ctx, text);
}

void display_debug(SDL_Window *sdlWindow, SDL_Renderer *sdlRenderer)
{
    current_section = DEBUG;

    char buf[64];

    if (sdlRenderer != NULL)
    {
        nk_layout_row_dynamic(nk_ctx, 15, 1);
        int windowWidth, windowHeight;
        SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
        snprintf(buf, sizeof(buf), "window width: %d", windowWidth);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "window height: %d", windowHeight);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        nk_label(nk_ctx, "SDL2 Internal", NK_TEXT_LEFT);
        nk_label(nk_ctx, "Renderer In Use.", NK_TEXT_LEFT);
        SDL_RendererInfo info;
        SDL_GetRendererInfo(sdlRenderer, &info);
        snprintf(buf, sizeof(buf), "Renderer: %s", info.name);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "touch controls: %d", toggle_touch_controls);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        if (toggle_touch_controls)
        {
            snprintf(buf, sizeof(buf), "thrust keys: %d", toggle_thrust_keys_touch);
            nk_label(nk_ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "arrow keys: %d", toggle_arrow_keys_touch);
            nk_label(nk_ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "dropdown keys: %d", toggle_dropdown_keys_touch);
            nk_label(nk_ctx, buf, NK_TEXT_LEFT);

            snprintf(buf, sizeof(buf), "view: %s", view_names[public_view]);
            nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        }
    }
}

void display_debug_settings()
{
    current_section = DEBUG_SETTINGS;

    nk_label(nk_ctx, "WARNING: CRASH ZONE!", NK_TEXT_LEFT);

    if (nk_button_label(nk_ctx, "Draw FPS"))
    {
        toggle_fps_draw = !toggle_fps_draw;

        SDL_Keysym sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_F, .sym = SDLK_f};
        Keymap_KeyDown(&sdlkey);
        Keymap_KeyUp(&sdlkey);
    }
    if (nk_button_label(nk_ctx, "Debug Draw"))
    {
        toggle_debug_draw = !toggle_debug_draw;
    }
    if (nk_button_label(nk_ctx, "toggle touch controls"))
    {
        toggle_touch_controls = !toggle_touch_controls;
    }

    if (nk_button_label(nk_ctx, "Dump m68k memory"))
    {
        dump_m68k_toggle = 1;
    }

    nk_text_multiline(nk_ctx, "Emulator Speed\nDefault is 20ms.");
    nk_slider_int(nk_ctx, 0.0, &emulation_speed, 100, 1);
    char buffer[32];
    if (emulation_speed == 0)
    {
        emulation_speed = 1;
    }
    sprintf(buffer, "Current: %dms", emulation_speed);
    nk_label(nk_ctx, buffer, NK_TEXT_LEFT);
}

int value1 = 0;
void display_cheat_menu()
{
    current_section = CHEATS;

    nk_layout_row_dynamic(nk_ctx, 15, 1);
    nk_text_multiline(nk_ctx, "Increase CASH Amount:");

    nk_text_multiline(nk_ctx, "Set The CASH Value To?:");

    if (nk_button_label(nk_ctx, "0"))
    {
        cash_value[0] = 0;
        cash_value[1] = 0;
        cash_value[2] = 0;
        cash_value[3] = 0;
        inject_cash_value = 1;
    }
    if (nk_button_label(nk_ctx, "100"))
    {
        cash_value[0] = 0;
        cash_value[1] = 0;
        cash_value[2] = 3;
        cash_value[3] = 232;
        inject_cash_value = 1;
    }
    if (nk_button_label(nk_ctx, "1,000"))
    {
        cash_value[0] = 0;
        cash_value[1] = 0;
        cash_value[2] = 39;
        cash_value[3] = 16;
        inject_cash_value = 1;
    }
    if (nk_button_label(nk_ctx, "1,000,000"))
    {
        cash_value[0] = 0;
        cash_value[1] = 152;
        cash_value[2] = 150;
        cash_value[3] = 128;
        inject_cash_value = 1;
    }
    if (nk_button_label(nk_ctx, "10,000,000"))
    {
        cash_value[0] = 5;
        cash_value[1] = 245;
        cash_value[2] = 225;
        cash_value[3] = 0;
        inject_cash_value = 1;
    }
    if (nk_button_label(nk_ctx, "MAX AMOUNT POSSIBLE"))
    {
        cash_value[0] = 127;
        cash_value[1] = 255;
        cash_value[2] = 255;
        cash_value[3] = 255;
        inject_cash_value = 1;
    }
}

void nk_render()
{
    if (nk_begin(nk_ctx, "M68k Menu", nk_rect(0, 40, 180, 330),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(nk_ctx, 0, 1);

        if (current_section == ABOUT || current_section == DEBUG || current_section == DEBUG_SETTINGS || current_section == CHEATS)
        {
            if (nk_button_label(nk_ctx, "BACK"))
            {
                if (current_section == DEBUG_SETTINGS)
                {
                    current_section = DEBUG;
                }
                else
                {
                    current_section = HOME;
                }
            }
            if (current_section == DEBUG)
            {
                if (nk_button_label(nk_ctx, "OPTIONS"))
                {
                    current_section = DEBUG_SETTINGS;
                }
            }
        }
        else
        {
            if ((nk_button_label(nk_ctx, "ABOUT")) || current_section == ABOUT)
            {
                current_section = ABOUT;
            }
            if ((nk_button_label(nk_ctx, "CHEATS")) || current_section == CHEATS)
            {
                current_section = CHEATS;
            }
            if ((nk_button_label(nk_ctx, "DEBUG")) || current_section == DEBUG)
            {
                current_section = DEBUG;
            }
            if ((nk_button_label(nk_ctx, "CLOSE")))
            {
                toggle_m68k_menu = 0;
            }
        }

        switch (current_section)
        {
        case HOME:
            break;
        case ABOUT:
            display_about();
            break;
        case CHEATS:
            display_cheat_menu();
            break;
        case DEBUG:
            display_debug(sdlWindow, sdlRenderer);
            break;
        case DEBUG_SETTINGS:
            display_debug_settings();
            break;
        default:
            printf("Unknown section: %d\n", current_section);
            current_section = HOME;
            break;
        }
    }
    nk_end(nk_ctx);
}

void sdl_nk_render()
{
    nk_render();

#ifndef WITH_GL
    nk_sdl_render(NK_ANTI_ALIASING_ON);
#endif
}

void gl3_nk_render()
{
    nk_render();

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}

void render_nuklear()
{
#ifdef WITH_GL
    gl3_nk_render();
#else
    if (sdlRenderer != NULL)
    {
        sdl_nk_render();
    }
#endif
}
