/*
 * ui_galaxy.h - the galaxy atlas inside the game (src/galaxy/atlas.h),
 * opened from the emulator menu. It covers the game screen and pauses the
 * game while it is open.
 */
#ifndef UI_GALAXY_H
#define UI_GALAXY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool ui_galaxy_open(void);
void ui_galaxy_show(bool show);

#ifdef __cplusplus
}

/* draws it, inside an ImGui frame (ui.cpp) */
void ui_galaxy_draw(void);
#endif

#endif /* UI_GALAXY_H */
