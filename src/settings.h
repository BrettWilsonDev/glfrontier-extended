/*
 * settings.h - the player's settings, kept across restarts in a small text
 * file (settings.c): aspect ratio, renderer, fullscreen, free camera
 * options, saves for the unmodded game, emulator speed and hidden touch
 * pads.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Reads the file and applies it; call once the window exists */
void settings_load(void);
/* Writes the file when anything changed since the last write; cheap, call
 * every frame (a phone can kill the app at any moment) */
void settings_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
