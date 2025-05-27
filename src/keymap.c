/*
  keymap.c

  SDL2 Keycode-based key mapping implementation
*/

#include "main.h"
#include "keymap.h"
#include "input.h"
#include "shortcut.h"
#include "renderer.h"
#include <SDL_keyboard.h>

/*-----------------------------------------------------------------------*/
/* SDL Keycode to ST scancode mapping table */
typedef struct {
    SDL_Keycode sdl_key;
    uint8_t st_scancode;
} KeyMapping;

static const KeyMapping KeyMappingTable[] = {
    /* Main keyboard */
    {SDLK_BACKSPACE, 0x0E},
    {SDLK_TAB, 0x0F},
    {SDLK_RETURN, 0x1C},
    {SDLK_ESCAPE, 0x01},
    {SDLK_SPACE, 0x39},
    {SDLK_QUOTE, 0x28},
    {SDLK_COMMA, 0x33},
    {SDLK_MINUS, 0x0C},
    {SDLK_PERIOD, 0x34},
    {SDLK_SLASH, 0x35},
    {SDLK_SEMICOLON, 0x27},
    {SDLK_EQUALS, 0x0D},
    {SDLK_LEFTBRACKET, 0x1A},
    {SDLK_BACKSLASH, 0x2B},
    {SDLK_RIGHTBRACKET, 0x1B},
    {SDLK_BACKQUOTE, 0x29},
    
    /* Numbers */
    {SDLK_0, 0x0B},
    {SDLK_1, 0x02},
    {SDLK_2, 0x03},
    {SDLK_3, 0x04},
    {SDLK_4, 0x05},
    {SDLK_5, 0x06},
    {SDLK_6, 0x07},
    {SDLK_7, 0x08},
    {SDLK_8, 0x09},
    {SDLK_9, 0x0A},
    
    /* Letters */
    {SDLK_a, 0x1E},
    {SDLK_b, 0x30},
    {SDLK_c, 0x2E},
    {SDLK_d, 0x20},
    {SDLK_e, 0x12},
    {SDLK_f, 0x21},
    {SDLK_g, 0x22},
    {SDLK_h, 0x23},
    {SDLK_i, 0x17},
    {SDLK_j, 0x24},
    {SDLK_k, 0x25},
    {SDLK_l, 0x26},
    {SDLK_m, 0x32},
    {SDLK_n, 0x31},
    {SDLK_o, 0x18},
    {SDLK_p, 0x19},
    {SDLK_q, 0x10},
    {SDLK_r, 0x13},
    {SDLK_s, 0x1F},
    {SDLK_t, 0x14},
    {SDLK_u, 0x16},
    {SDLK_v, 0x2F},
    {SDLK_w, 0x11},
    {SDLK_x, 0x2D},
    {SDLK_y, 0x15},
    {SDLK_z, 0x2C},
    
    /* Function keys */
    {SDLK_F1, 0x3B},
    {SDLK_F2, 0x3C},
    {SDLK_F3, 0x3D},
    {SDLK_F4, 0x3E},
    {SDLK_F5, 0x3F},
    {SDLK_F6, 0x40},
    {SDLK_F7, 0x41},
    {SDLK_F8, 0x42},
    {SDLK_F9, 0x43},
    {SDLK_F10, 0x44},
    
    /* Modifiers */
    {SDLK_LSHIFT, 0x2A},
    {SDLK_RSHIFT, 0x36},
    {SDLK_LCTRL, 0x1D},
    {SDLK_RCTRL, 0x1D},
    {SDLK_LALT, 0x38},
    {SDLK_RALT, 0x38},
    {SDLK_CAPSLOCK, 0x3A},
    
    /* Arrow keys */
    {SDLK_UP, 0x48},
    {SDLK_DOWN, 0x50},
    {SDLK_LEFT, 0x4B},
    {SDLK_RIGHT, 0x4D},
    
    /* Special keys */
    {SDLK_INSERT, 0x52},
    {SDLK_HOME, 0x47},
    {SDLK_END, 0x4F},
    {SDLK_PAGEUP, 0x49},
    {SDLK_PAGEDOWN, 0x51},
    {SDLK_DELETE, 0x53},
    {SDLK_PRINTSCREEN, 0x54},
    {SDLK_SCROLLLOCK, 0x46},
    {SDLK_PAUSE, 0x45},
    
    /* Keypad (NumLock on) */
    {SDLK_KP_0, 0x70},
    {SDLK_KP_1, 0x6D},
    {SDLK_KP_2, 0x6E},
    {SDLK_KP_3, 0x6F},
    {SDLK_KP_4, 0x6A},
    {SDLK_KP_5, 0x6B},
    {SDLK_KP_6, 0x6C},
    {SDLK_KP_7, 0x67},
    {SDLK_KP_8, 0x68},
    {SDLK_KP_9, 0x69},
    {SDLK_KP_PERIOD, 0x71},
    {SDLK_KP_DIVIDE, 0x65},
    {SDLK_KP_MULTIPLY, 0x66},
    {SDLK_KP_MINUS, 0x4A},
    {SDLK_KP_PLUS, 0x4E},
    {SDLK_KP_ENTER, 0x72},
    
    /* Terminator */
    {SDLK_UNKNOWN, 0xFF}
};

/* List of ST scan codes to NOT de-bounce */
static const uint8_t DebounceExtendedKeys[] = {
    0x1D,  /* CTRL */
    0x2A,  /* Left SHIFT */
    0x01,  /* ESC */
    0x38,  /* ALT */
    0x36,  /* Right SHIFT */
    0      /* terminator */
};

/*-----------------------------------------------------------------------*/
/*
  Initialization
*/
void Keymap_Init(void)
{
    /* No initialization needed for this implementation */
}

/*-----------------------------------------------------------------------*/
/*
  Get ST scancode from SDL key event
*/
static uint8_t Keymap_GetSTScanCode(SDL_Keysym *keysym)
{
    /* Handle NumLock state for keypad */
    // if (keysym->sym >= SDLK_KP_0 && keysym->sym <= SDLK_KP_9) {
    if ((keysym->sym - SDLK_KP_0) <= (SDLK_KP_9 - SDLK_KP_0)) {
        if (!(SDL_GetModState() & KMOD_NUM)) {
            /* NumLock off - use alternate mappings */
            switch (keysym->sym) {
                case SDLK_KP_1: return 0x6D;  /* End */
                case SDLK_KP_2: return 0x50;  /* Down */
                case SDLK_KP_3: return 0x6F;  /* Page Down */
                case SDLK_KP_4: return 0x4B;  /* Left */
                case SDLK_KP_6: return 0x4D;  /* Right */
                case SDLK_KP_7: return 0x47;  /* Home */
                case SDLK_KP_8: return 0x48;  /* Up */
                case SDLK_KP_9: return 0x49;  /* Page Up */
                default: break;
            }
        }
    }

    /* Lookup in table */
    for (int i = 0; KeyMappingTable[i].sdl_key != SDLK_UNKNOWN; i++) {
        if (KeyMappingTable[i].sdl_key == keysym->sym) {
            return KeyMappingTable[i].st_scancode;
        }
    }

    return 0xFF;  /* Not found */
}

/*-----------------------------------------------------------------------*/
/*
  Check if key should be de-bounced
*/
BOOL Keymap_DebounceSTKey(uint8_t STScanCode)
{
    for (int i = 0; DebounceExtendedKeys[i] != 0; i++) {
        if (STScanCode == DebounceExtendedKeys[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/*-----------------------------------------------------------------------*/
/*
  Key down event
*/
void Keymap_KeyDown(SDL_Keysym *sdlkey)
{
    uint8_t st_code;
    int symkey = sdlkey->sym;
    int modkey = sdlkey->mod;

    /* Handle shortcut keys */
    if ((modkey & KMOD_MODE) || (modkey & KMOD_RGUI) || (modkey & KMOD_CTRL)) {
        ShortCutKey.Key = symkey;
        if (modkey & (KMOD_LCTRL | KMOD_RCTRL))
            ShortCutKey.bCtrlPressed = TRUE;
        if (modkey & (KMOD_LSHIFT | KMOD_RSHIFT))
            ShortCutKey.bShiftPressed = TRUE;
        return;
    }

    /* Ignore keys we don't want to process */
    if (symkey == SDLK_MODE || symkey == SDLK_LGUI || symkey == SDLK_NUMLOCKCLEAR) {
        return;
    }

    /* Get ST scancode */
    st_code = Keymap_GetSTScanCode(sdlkey);
    if (st_code != 0xFF) {
        Input_PressSTKey(st_code, TRUE);
    }
}

/*-----------------------------------------------------------------------*/
/*
  Key up event
*/
void Keymap_KeyUp(SDL_Keysym *sdlkey)
{
    uint8_t st_code;
    int symkey = sdlkey->sym;

    /* Handle special cases */
    if (symkey == SDLK_CAPSLOCK) {
        /* Simulate capslock toggle */
        Input_PressSTKey(0x3A, TRUE);
        return;
    }

    /* Ignore keys we don't want to process */
    if (symkey == SDLK_MODE || symkey == KMOD_LGUI || symkey == SDLK_NUMLOCKCLEAR) {
        return;
    }

    /* Get ST scancode */
    st_code = Keymap_GetSTScanCode(sdlkey);
    if (st_code != 0xFF) {
        Input_PressSTKey(st_code, FALSE);
    }
}

/*-----------------------------------------------------------------------*/
/*
  Debounce all keys (called each frame)
*/
void Keymap_DebounceAllKeys(void)
{
    /* Not needed in this implementation */
}