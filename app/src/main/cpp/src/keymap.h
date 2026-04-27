#ifndef HATARI_KEYMAP_H
#define HATARI_KEYMAP_H

#include <SDL_keyboard.h>

extern void Keymap_Init(void);
extern char Keymap_RemapKeyToSTScanCode(SDL_Keysym *pKeySym);
extern void Keymap_LoadRemapFile(char *pszFileName);
extern void Keymap_DebounceAllKeys(void);
extern void Keymap_KeyDown(SDL_Keysym *sdlkey);
extern void Keymap_KeyUp(SDL_Keysym *sdlkey);

#endif
