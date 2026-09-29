/*
 * style.h - the game's look for the galaxy tool's ImGui (see style.cpp).
 */
#pragma once

#include "imgui.h"

namespace ui
{

constexpr int PX = 2; /* screen pixels per game pixel */
constexpr int COL_SPACE = 0x002, COL_PANEL_DARK = 0x444, COL_PANEL = 0x666, COL_PANEL_LIGHT = 0x999,
			  COL_HIGHLIGHT = 0xddd, COL_TEXT = 0xddd, COL_ORANGE = 0xf80, COL_RED = 0xe00, COL_YELLOW = 0xfe0;

ImVec4 rgb444(int c, float a = 1.0f);
void build_font(int px);
void build_style(int px);
void heading(const char *text);		   /* orange, wrapped */
void note(const char *text);		   /* grey, wrapped */
bool button(const char *label, float width = 0); /* bevelled game button, 0 = full width */

} // namespace ui
