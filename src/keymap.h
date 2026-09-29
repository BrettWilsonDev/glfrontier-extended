/*
 * keymap.h - SDL key events to Atari ST scancodes.
 */
#ifndef KEYMAP_H
#define KEYMAP_H

#include <SDL_keyboard.h>

#ifdef __cplusplus
extern "C"
{
#endif

void Keymap_Init(void);
void Keymap_KeyDown(SDL_Keysym *sdlkey);
void Keymap_KeyUp(SDL_Keysym *sdlkey);
/* Called by the game every frame; nothing to do with SDL key events */
void Keymap_DebounceAllKeys(void);

#ifdef __cplusplus
}
#endif

#endif /* KEYMAP_H */
