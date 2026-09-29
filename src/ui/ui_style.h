/*
 * ui_style.h - makes Dear ImGui look like Frontier's own interface: the
 * game's 8x8 pixel font, its palette and bevelled control panel buttons.
 *
 * Everything is sized in "game pixels" (one pixel of the 320x240 game
 * screen), so the menu scales with the window like the game does.
 */
#ifndef UI_STYLE_H
#define UI_STYLE_H

/* Builds the font and style for `px` screen pixels per game pixel.
 * Call between frames; returns true if the font atlas was rebuilt (the
 * renderer backend must then recreate its font texture). */
bool ui_style_update(int px);

/* Game pixel size in screen pixels (for custom drawing) */
float ui_style_px(void);

/* Bevelled button, like the game's control panel icons. Fills the rest of
 * the line unless given a width. */
bool ui_button(const char *label, float width = 0.0f);
/* Width for each of `count` buttons sharing one line */
float ui_row_width(int count);
/* Orange section heading */
void ui_heading(const char *text);
/* Begin a menu window with an orange title (ImGui::Begin + style) */
bool ui_begin_window(const char *title);

#endif /* UI_STYLE_H */
