/*
 * imconfig_galaxy.h - Dear ImGui config for the galaxy tool: its asserts are
 * checked in release builds too and reported by app.cpp (a count, and the
 * first few printed) instead of vanishing, so --smoke can catch UI misuse.
 */
#pragma once

void galaxy_imgui_assert(const char *expr, const char *file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : galaxy_imgui_assert(#_EXPR, __FILE__, __LINE__))
