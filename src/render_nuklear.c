#ifdef USE_NK

#include "../m68000.h"
#include "main.h"
#include "keymap.h"
#include "input.h"
#include "touch_input.h"
#include "renderer.h"

#include "../host.h"

#include "render_nuklear.h"
#ifdef WITH_GL
#include "nuklear_sdl_gl3_impl.h"
#else
#include "nuklear_sdl_impl.h"
#endif

#ifndef NK_TEXT_WRAP
#define NK_TEXT_WRAP 0x10
#endif

#include "cheats.h"

extern float nk_ui_scale;

#define NK_S(x)           ((float)(x) * nk_ui_scale)   /* scale any value       */
#define NK_ROW(h)         NK_S(h)                       /* row / button height   */
#define NK_COMBO_ITEM_H   ((int)NK_S(25))               /* dropdown row height   */
#define NK_VEC2(w, h)     nk_vec2(NK_S(w), NK_S(h))    /* scaled nk_vec2        */

/* Convenience: dynamic single-column row at a scaled height.
 * Pass 0 to let Nuklear auto-size (font + button padding). */
static inline void nk_row(struct nk_context *ctx, float h, int cols)
{
    nk_layout_row_dynamic(ctx, h > 0.0f ? NK_ROW(h) : 0.0f, cols);
}

/* Convenience: static single-column row at a scaled height and width. */
static inline void nk_row_static(struct nk_context *ctx, float h, float w)
{
    nk_layout_row_begin(ctx, NK_STATIC, (int)NK_ROW(h), 1);
    nk_layout_row_push(ctx, NK_S(w));
}

/* ---------------------------------------------------------------------------
 * Globals
 * --------------------------------------------------------------------------- */
static int show_secondary_window = 0;

extern int dump_m68k_toggle;

BOOL toggle_debug_draw = FALSE;

static SDL_Window   *sdlWindow;
static SDL_Renderer *sdlRenderer;

void nuklear_set_sdl_screen_data(SDL_Window *sdlWindowParam, SDL_Renderer *sdlRendererParam)
{
    sdlWindow   = sdlWindowParam;
    sdlRenderer = sdlRendererParam;
}

enum section
{
    HOME,
    SETTINGS,
    ABOUT,
    DEBUG,
    DEBUG_SETTINGS,
    CHEATS,
};

enum scondary_window
{
    NONE,
    CONSOLE,
    MORE_CHEATS,
};

static enum section          current_section          = HOME;
static enum scondary_window  current_secondary_window = NONE;

/* ---------------------------------------------------------------------------
 * Virtual UI resolution → physical screen mapping
 * --------------------------------------------------------------------------- */
#define UI_BASE_W 320.0f
#define UI_BASE_H 240.0f

extern int Screen_GetGameWidth(void);
extern int Screen_GetGameHeight(void);

static float ui_scale_x(void)
{
    int gw = Screen_GetGameWidth();
    if (gw <= 0) return 1.0f;
    return (float)gw / UI_BASE_W;
}

static float ui_scale_y(void)
{
    int gh = Screen_GetGameHeight();
    if (gh <= 0) return 1.0f;
    return (float)gh / UI_BASE_H;
}

static float ui_to_screen_x(float x) { return (float)Screen_GetGameOffsetX() + x * ui_scale_x(); }
static float ui_to_screen_y(float y) { return (float)Screen_GetGameOffsetY() + y * ui_scale_y(); }
static float ui_to_screen_w(float w) { return w * ui_scale_x(); }
static float ui_to_screen_h(float h) { return h * ui_scale_y(); }

/* ---------------------------------------------------------------------------
 * Text helpers
 * --------------------------------------------------------------------------- */
void nk_text_multiline(struct nk_context *ctx, const char *text)
{
    const char *line_start = text;
    const char *p          = text;

    while (*p)
    {
        if (*p == '\n')
        {
            int len = (int)(p - line_start);
            nk_row(ctx, 15, 1);
            nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);
            p++;
            line_start = p;
        }
        else
        {
            p++;
        }
    }

    if (p != line_start)
    {
        int len = (int)(p - line_start);
        nk_row(ctx, 15, 1);
        nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);
    }
}

/* Scroll-panel multiline: forces a wide fixed row so text scrolls horizontally
 * inside a secondary window rather than wrapping. */
void nk_scroll_multiline(struct nk_context *ctx, const char *text)
{
    const char  *line_start  = text;
    const char  *p           = text;
    const float  forced_width = NK_S(1400.0f);

    while (*p)
    {
        if (*p == '\n')
        {
            int len = (int)(p - line_start);
            nk_layout_row_begin(ctx, NK_STATIC, (int)NK_ROW(8), 1);
            nk_layout_row_push(ctx, forced_width);
            nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);
            nk_layout_row_end(ctx);
            p++;
            line_start = p;
        }
        else
        {
            p++;
        }
    }

    if (p != line_start)
    {
        int len = (int)(p - line_start);
        nk_layout_row_begin(ctx, NK_STATIC, (int)NK_ROW(8), 1);
        nk_layout_row_push(ctx, forced_width);
        nk_text(ctx, line_start, len, NK_TEXT_ALIGN_LEFT);
        nk_layout_row_end(ctx);
    }
}

/* ---------------------------------------------------------------------------
 * Secondary window: console
 * --------------------------------------------------------------------------- */
void display_console(void)
{
    nk_row(nk_ctx, 25, 1);
    nk_scroll_multiline(nk_ctx, clslog);
}

/* ---------------------------------------------------------------------------
 * Section: ABOUT
 * --------------------------------------------------------------------------- */
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
        "minivorbis, glutess,\n"
        "and nuklear gui.\n\n"
        "Acknowledgments:\n"
        "Frontier Developments,\n"
        "Hatari emulator,\n"
        "Pcercuei and Kochise.\n";

    nk_text_multiline(nk_ctx, text);
}

/* ---------------------------------------------------------------------------
 * Section: DEBUG
 * --------------------------------------------------------------------------- */
void display_debug(SDL_Window *sdlWindow, SDL_Renderer *sdlRenderer)
{
    current_section = DEBUG;

    nk_row(nk_ctx, 0, 1);

    if (current_secondary_window == NONE || current_secondary_window != CONSOLE)
    {
        if (nk_button_label(nk_ctx, "SHOW CONSOLE"))
            current_secondary_window = CONSOLE;
    }
    if (current_secondary_window == CONSOLE)
    {
        if (nk_button_label(nk_ctx, "HIDE CONSOLE"))
            current_secondary_window = NONE;
    }

    char buf[64];
    if (sdlRenderer != NULL)
    {
        nk_row(nk_ctx, 15, 1);
        int windowWidth, windowHeight;
        SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
        snprintf(buf, sizeof(buf), "Window Width: %d", windowWidth);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "Window Height: %d", windowHeight);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        nk_label(nk_ctx, "SDL2 Internal", NK_TEXT_LEFT);
        nk_label(nk_ctx, "Renderer In Use.", NK_TEXT_LEFT);
        SDL_RendererInfo info;
        SDL_GetRendererInfo(sdlRenderer, &info);
        snprintf(buf, sizeof(buf), "Renderer: %s", info.name);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
    }
    else
    {
#ifdef WITH_GL
        nk_row(nk_ctx, 15, 1);
        int major, minor;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
        snprintf(buf, sizeof(buf), "OpenGL Version: %d.%d", major, minor);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);

        const char *(*glGetString)(GLenum) = (const char *(*)(GLenum))SDL_GL_GetProcAddress("glGetString");
        const char *renderer_name  = glGetString(GL_RENDERER);
        const char *vendor_name    = glGetString(GL_VENDOR);

        nk_label(nk_ctx, "What OpenGL Profile:", NK_TEXT_LEFT);

        int profile_mask;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile_mask);
        if (profile_mask & SDL_GL_CONTEXT_PROFILE_CORE)
            nk_label(nk_ctx, "Core Is Enabled", NK_TEXT_LEFT);
        else if (profile_mask & SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)
            nk_label(nk_ctx, "Legacy Is Enabled", NK_TEXT_LEFT);
        else
            nk_label(nk_ctx, "Unknown Profile", NK_TEXT_LEFT);

        snprintf(buf, sizeof(buf), "%s", vendor_name);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "%s", renderer_name);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
#endif
    }

    nk_row(nk_ctx, 15, 1);
    snprintf(buf, sizeof(buf), "Touch Controls: %d", toggle_touch_controls);
    nk_label(nk_ctx, buf, NK_TEXT_LEFT);

    if (toggle_touch_controls)
    {
        snprintf(buf, sizeof(buf), "Thrust Keys: %d", toggle_thrust_keys_touch);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "Arrow Keys: %d", toggle_arrow_keys_touch);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "Dropdown Keys: %d", toggle_dropdown_keys_touch);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
        snprintf(buf, sizeof(buf), "View: %s", view_names[public_view]);
        nk_label(nk_ctx, buf, NK_TEXT_LEFT);
    }
}

/* ---------------------------------------------------------------------------
 * Section: DEBUG SETTINGS
 * --------------------------------------------------------------------------- */
void display_debug_settings(void)
{
    current_section = DEBUG_SETTINGS;

    nk_row(nk_ctx, 15, 1);
    nk_label(nk_ctx, "WARNING: CRASH ZONE!", NK_TEXT_LEFT);

    nk_row(nk_ctx, 0, 1);

    if (nk_button_label(nk_ctx, "Draw FPS"))
    {
        toggle_fps_draw = !toggle_fps_draw;
        SDL_Keysym sdlkey = (SDL_Keysym){.scancode = SDL_SCANCODE_F, .sym = SDLK_f};
        Keymap_KeyDown(&sdlkey);
        Keymap_KeyUp(&sdlkey);
    }

    // if (sdlRenderer != NULL)
    {
        if (nk_button_label(nk_ctx, "Debug Draw"))
            toggle_debug_draw = !toggle_debug_draw;

        if (nk_button_label(nk_ctx, "Toggle Touch Controls"))
            toggle_touch_controls = !toggle_touch_controls;
    }

    if (nk_button_label(nk_ctx, "Dump M68k Memory"))
        dump_m68k_toggle = 1;

    nk_text_multiline(nk_ctx, "Emulator Speed\nDefault is 20ms.");

    nk_row(nk_ctx, 0, 1);
    nk_slider_int(nk_ctx, 0, &emulation_speed, 100, 1);

    if (emulation_speed == 0)
        emulation_speed = 1;

    char buffer[32];
    nk_row(nk_ctx, 15, 1);
    sprintf(buffer, "Current: %dms", emulation_speed);
    nk_label(nk_ctx, buffer, NK_TEXT_LEFT);
}

/* ---------------------------------------------------------------------------
 * Section: SETTINGS
 * --------------------------------------------------------------------------- */
void display_settings(void)
{
    current_section = SETTINGS;

    nk_row(nk_ctx, 15, 1);
    nk_label(nk_ctx, "Game Settings:", NK_TEXT_LEFT);

    nk_row(nk_ctx, 0, 1);

    if (nk_button_label(nk_ctx, "Fullscreen Toggle"))
        Screen_ToggleFullScreen();

    if (sdlRenderer == NULL)
    {
        if (nk_button_label(nk_ctx, "Cycle Renderer"))
            Screen_ToggleRenderer();
    }
}

/* ---------------------------------------------------------------------------
 * Section: CHEATS (cash buttons in the main panel)
 * --------------------------------------------------------------------------- */
void display_cheat_menu(void)
{
    current_section = CHEATS;

    nk_row(nk_ctx, 0, 1);

    if (current_secondary_window == NONE || current_secondary_window != MORE_CHEATS)
    {
        if (nk_button_label(nk_ctx, "MORE CHEATS"))
            current_secondary_window = MORE_CHEATS;
    }
    if (current_secondary_window == MORE_CHEATS)
    {
        if (nk_button_label(nk_ctx, "HIDE MORE CHEATS"))
            current_secondary_window = NONE;
    }

    nk_text_multiline(nk_ctx, "Set The CASH Value To?:");

    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "0"))
    {
        cash_value[0] = 0; cash_value[1] = 0; cash_value[2] = 0; cash_value[3] = 0;
        inject_cash_value_m68kram();
    }
    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "100"))
    {
        cash_value[0] = 0; cash_value[1] = 0; cash_value[2] = 3; cash_value[3] = 232;
        inject_cash_value_m68kram();
    }
    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "1,000"))
    {
        cash_value[0] = 0; cash_value[1] = 0; cash_value[2] = 39; cash_value[3] = 16;
        inject_cash_value_m68kram();
    }
    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "1,000,000"))
    {
        cash_value[0] = 0; cash_value[1] = 152; cash_value[2] = 150; cash_value[3] = 128;
        inject_cash_value_m68kram();
    }
    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "10,000,000"))
    {
        cash_value[0] = 5; cash_value[1] = 245; cash_value[2] = 225; cash_value[3] = 0;
        inject_cash_value_m68kram();
    }
    nk_row(nk_ctx, 0, 1);
    if (nk_button_label(nk_ctx, "MAX AMOUNT POSSIBLE"))
    {
        cash_value[0] = 127; cash_value[1] = 255; cash_value[2] = 255; cash_value[3] = 255;
        inject_cash_value_m68kram();
    }
}

/* ---------------------------------------------------------------------------
 * Secondary window: MORE CHEATS (inventory + ship editor)
 * --------------------------------------------------------------------------- */
void display_cheats(void)
{
    static int edit_player_or_ship = 0;

    nk_row_static(nk_ctx, 30, 200);

    if (edit_player_or_ship == 0)
    {
        if (nk_button_label(nk_ctx, "Show Ship Cheats"))
            edit_player_or_ship = 1;
    }
    else
    {
        if (nk_button_label(nk_ctx, "Show Player Cheats"))
            edit_player_or_ship = 0;
    }

    /* ---- Player inventory ---- */
    if (edit_player_or_ship == 0)
    {
        const char *items[] = {
            "", "", "", "", "", "", "", "", "", "",
            "", "", "", "", "", "", "", "", "", "",
            "", "", "", "", "", "", "", "", "", "",
            "", "", "", "Water", "", "Liquid Oxygen", "", "Grain", "", "Fruit And Veg.",
            "", "Animal Meat", "", "Synthetic Meat", "", "Liquor", "", "Narcotics", "", "Medicines",
            "", "Fertilizer", "", "Animal Skins", "", "Live Animals", "", "Slaves", "", "Luxury Goods",
            "", "Heavy Plastics", "", "Metal Alloys", "", "Precious Metals", "", "Gem Stones", "", "Minerals",
            "", "Hydrogen Fuel", "", "Military Fuel", "", "Hand Weapons", "", "Battle Weapons", "", "Nerve Gas",
            "", "Industrial Parts", "", "Computers", "", "Air Processors", "", "Farm Machinery", "", "Robots",
            "", "Radioactives", "", "Rubbish"
        };
        const int item_count = NK_LEN(items);
        static int selected_index = 0;

        /* Build a filtered list of non-empty items */
        int visible_indices[NK_LEN(items)];
        int visible_count = 0;
        for (int i = 0; i < item_count; ++i)
        {
            if (items[i][0] != '\0')
                visible_indices[visible_count++] = i;
        }

        const char *visible_items[NK_LEN(items)];
        for (int i = 0; i < visible_count; ++i)
            visible_items[i] = items[visible_indices[i]];

        int current_visible_index = 0;
        for (int i = 0; i < visible_count; ++i)
        {
            if (visible_indices[i] == selected_index)
            {
                current_visible_index = i;
                break;
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        nk_label(nk_ctx, "Pick An Inventory Item To Add", NK_TEXT_LEFT);

        int new_visible_index = nk_combo(
            nk_ctx,
            visible_items, visible_count,
            current_visible_index,
            NK_COMBO_ITEM_H,
            NK_VEC2(200, 200));
        selected_index = visible_indices[new_visible_index];

        char formatted_string[30];

        nk_row_static(nk_ctx, 30, 200);
        sprintf(formatted_string, "+1 %s", items[selected_index]);
        if (nk_button_label(nk_ctx, formatted_string))
        {
            update_player_cheat_values_m68kram();
            if (cheat_player_values[selected_index] != 255)
            {
                cheat_player_values[selected_index]++;
                inject_player_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        sprintf(formatted_string, "+256 %s", items[selected_index]);
        if (nk_button_label(nk_ctx, formatted_string))
        {
            update_player_cheat_values_m68kram();
            if (cheat_player_values[selected_index] != 255)
            {
                cheat_player_values[selected_index - 1]++;
                inject_player_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        sprintf(formatted_string, "-1 %s", items[selected_index]);
        if (nk_button_label(nk_ctx, formatted_string))
        {
            update_player_cheat_values_m68kram();
            if (cheat_player_values[selected_index] > 0)
            {
                cheat_player_values[selected_index]--;
                inject_player_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        sprintf(formatted_string, "-256 %s", items[selected_index]);
        if (nk_button_label(nk_ctx, formatted_string))
        {
            update_player_cheat_values_m68kram();
            if (cheat_player_values[selected_index - 1] > 0)
            {
                cheat_player_values[selected_index - 1]--;
                inject_player_cheat_values_m68kram();
            }
        }

        /* ---- Elite rank ---- */
        const char *elite_ranks[] = {
            "ELITE", "Deadly", "Dangerous", "Competent", "Average",
            "Below Average", "Poor", "Mostly Harmless", "Harmless"
        };
        static int selected_rank = 0;

        nk_row_static(nk_ctx, 30, 200);
        nk_label(nk_ctx, "Set the player elite rank to:", NK_TEXT_LEFT);

        selected_rank = nk_combo(
            nk_ctx,
            elite_ranks, NK_LEN(elite_ranks),
            selected_rank,
            NK_COMBO_ITEM_H,
            NK_VEC2(200, 200));

        nk_row_static(nk_ctx, 30, 200);
        if (nk_button_label(nk_ctx, "Apply Rank"))
        {
            update_player_cheat_values_m68kram();
            switch (selected_rank)
            {
            case 0: cheat_player_values[102] = 1; cheat_player_values[103] = 0;  cheat_player_values[104] = 0;  break;
            case 1: cheat_player_values[102] = 0; cheat_player_values[103] = 20; cheat_player_values[104] = 0;  break;
            case 2: cheat_player_values[102] = 0; cheat_player_values[103] = 10; cheat_player_values[104] = 0;  break;
            case 3: cheat_player_values[102] = 0; cheat_player_values[103] = 1;  cheat_player_values[104] = 0;  break;
            case 4: cheat_player_values[102] = 0; cheat_player_values[103] = 0;  cheat_player_values[104] = 40; break;
            case 5: cheat_player_values[102] = 0; cheat_player_values[103] = 0;  cheat_player_values[104] = 20; break;
            case 6: cheat_player_values[102] = 0; cheat_player_values[103] = 0;  cheat_player_values[104] = 10; break;
            case 7: cheat_player_values[102] = 0; cheat_player_values[103] = 0;  cheat_player_values[104] = 4;  break;
            case 8: cheat_player_values[102] = 0; cheat_player_values[103] = 0;  cheat_player_values[104] = 0;  break;
            }
            inject_player_cheat_values_m68kram();
        }
    }
    else /* ---- Ship cheats ---- */
    {
        int ship_id_count = find_and_store_ship_ids();

        static int run_once_ship_id     = 1;
        static int current_ship_id_count = 0;

        if (ship_id_count != current_ship_id_count)
        {
            run_once_ship_id      = 1;
            current_ship_id_count = ship_id_count;
        }

        if (run_once_ship_id)
        {
            run_once_ship_id      = 0;
            ship_id_index         = ship_id_count - 1;
            current_ship_id_count = ship_id_count;
        }

        if (ship_id_count == 0)
        {
            log_printf("Could not even find the player ship ID something is very wrong\n");
        }

        nk_row_static(nk_ctx, 30, 200);
        nk_label(nk_ctx, "Pick A Ship ID To Modify", NK_TEXT_LEFT);
        nk_label(nk_ctx, "Preferably Your Ship", NK_TEXT_LEFT);

        ship_id_index = nk_combo(
            nk_ctx,
            (const char *const *)ship_ids, ship_id_count,
            ship_id_index,
            NK_COMBO_ITEM_H,
            NK_VEC2(200, 200));

        nk_row_static(nk_ctx, 30, 200);
        if (nk_button_label(nk_ctx, "Increase JumpDrive Type"))
        {
            update_ship_cheat_values_m68kram(ship_id_index);
            if (cheat_ship_values[20] != 13)
            {
                cheat_ship_values[20] += 1;
                inject_ship_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        if (nk_button_label(nk_ctx, "Decrease JumpDrive Type"))
        {
            update_ship_cheat_values_m68kram(ship_id_index);
            if (cheat_ship_values[20] != 0)
            {
                cheat_ship_values[20] -= 1;
                inject_ship_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        if (nk_button_label(nk_ctx, "Increase Hull health"))
        {
            update_ship_cheat_values_m68kram(ship_id_index);
            if (cheat_ship_values[39] != 255)
            {
                cheat_ship_values[39] += 1;
                inject_ship_cheat_values_m68kram();
            }
        }

        nk_row_static(nk_ctx, 30, 200);
        if (nk_button_label(nk_ctx, "Decrease Hull health"))
        {
            update_ship_cheat_values_m68kram(ship_id_index);
            if (cheat_ship_values[39] != 0)
            {
                cheat_ship_values[39] -= 1;
                inject_ship_cheat_values_m68kram();
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Main render entry point
 * --------------------------------------------------------------------------- */
void nk_render(void)
{
    int gw = Screen_GetGameWidth();
    int gh = Screen_GetGameHeight();

    if (gw <= 0 || gh <= 0)
        return;

    float x = ui_to_screen_x(0);
    float y = ui_to_screen_y(0);
    float w = ui_to_screen_w(120);
    float h = ui_to_screen_h(180);

    if (nk_begin(nk_ctx, "Emulator Menu",
                 nk_rect(x, y, w, h),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        nk_row(nk_ctx, 0, 1);

        if (current_section == ABOUT      ||
            current_section == DEBUG      ||
            current_section == DEBUG_SETTINGS ||
            current_section == CHEATS     ||
            current_section == SETTINGS)
        {
            if (nk_button_label(nk_ctx, "BACK"))
            {
                current_section = (current_section == DEBUG_SETTINGS) ? DEBUG : HOME;
            }
            if (current_section == DEBUG)
            {
                nk_row(nk_ctx, 0, 1);
                if (nk_button_label(nk_ctx, "OPTIONS"))
                    current_section = DEBUG_SETTINGS;
            }
        }
        else
        {
            nk_spacing(nk_ctx, 1);
            if (nk_button_label(nk_ctx, "CLOSE"))
                toggle_m68k_menu = 0;

            nk_row(nk_ctx, 0, 1);
            if (nk_button_label(nk_ctx, "SETTINGS"))
                current_section = SETTINGS;

            nk_row(nk_ctx, 0, 1);
            if (nk_button_label(nk_ctx, "CHEATS"))
                current_section = CHEATS;

            nk_row(nk_ctx, 0, 1);
            if (nk_button_label(nk_ctx, "DEBUG"))
                current_section = DEBUG;

            nk_row(nk_ctx, 0, 1);
            if (nk_button_label(nk_ctx, "ABOUT"))
                current_section = ABOUT;
        }

        switch (current_section)
        {
        case HOME:
            current_secondary_window = NONE;
            break;
        case SETTINGS:
            display_settings();
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
        case ABOUT:
            display_about();
            break;
        default:
            printf("Unknown section: %d\n", current_section);
            current_section = HOME;
            break;
        }
    }
    nk_end(nk_ctx);

    if (current_secondary_window != NONE)
    {
        if (nk_begin(nk_ctx, "",
                     nk_rect(ui_to_screen_x(120), ui_to_screen_y(0),
                             ui_to_screen_w(170), ui_to_screen_h(180)),
                     NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCROLL_AUTO_HIDE))
        {
            switch (current_secondary_window)
            {
            case NONE:
                break;
            case CONSOLE:
                display_console();
                break;
            case MORE_CHEATS:
                display_cheats();
                break;
            default:
                break;
            }
        }
        nk_end(nk_ctx);
    }
}

/* ---------------------------------------------------------------------------
 * Render back-ends
 * --------------------------------------------------------------------------- */
void sdl_nk_render(void)
{
    nk_render();
#ifndef WITH_GL
    nk_sdl_render(NK_ANTI_ALIASING_ON);
#endif
}

void gl3_nk_render(void)
{
    nk_render();

#define MAX_VERTEX_MEMORY  512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

#ifdef WITH_GL
    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);

    /* Restore full-drawable viewport and GL state after nuklear render */
    int dw, dh;
    SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &dw, &dh);
    glViewport(0, 0, dw, dh);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
#endif
}

void render_nuklear(void)
{
#ifdef WITH_GL
    gl3_nk_render();
#else
    if (sdlRenderer != NULL)
        sdl_nk_render();
#endif
}

#endif // USE_NK