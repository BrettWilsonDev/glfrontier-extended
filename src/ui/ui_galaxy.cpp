/*
 * ui_galaxy.cpp - the galaxy atlas inside the game. The atlas (map, search,
 * starport overlay, the galaxy map's own System / Trade / Planets screens,
 * the orbit view) is src/galaxy/atlas.h, shared with the standalone
 * tools/fe2GalaxyViewer; this is its window here, laid out for the game's
 * screen:
 *
 *   map mode      [CLOSE] [search.............] [VIEW] [GO] [INFO]
 *                 the map, with search results over it while searching
 *                 the selected system's name and star, [DETAILS]
 *   details mode  [MAP] and the system's tabs, a game screen wide
 *
 * Every frame's use of the atlas runs game code inside the running game,
 * between fe2vm_begin() and fe2vm_end(), which put the machine back as it
 * was (src/galaxy/fe2vm.h). The game is paused while the atlas is open.
 */
#include "ui_galaxy.h"

#include <SDL.h>
#include <stdio.h>
#include <string>

#include "../galaxy/atlas.h"
#include "../galaxy/fe2vm.h"
#include "../gl/gl_api.h"
#include "imgui.h"
#include "ui_style.h"

extern "C"
{
#include "host.h"
#include "main.h"
#include "renderer.h"
}

using namespace galaxy;

namespace
{

/* The game's globals this uses (offsets from A6, fe2/fe2_modded.s) */
constexpr uint32_t A6_TIME_ACCEL = 14482; /* A6_time_accel: game time per frame, 0 = stopped */
constexpr uint32_t A6_CURRENT_SYSTEM = 726; /* the system the player is in (l83e70) */

bool open, started, details, opening;
bool drawing, close_requested; /* closing waits until the frame's calls are done */
Atlas *atlas;
int32_t saved_time_accel;

unsigned int gl_texture(const uint32_t *rgba, int w, int h, unsigned int old, bool smooth)
{
	GLuint tex = old;
	if (!tex)
		glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	GLint filter = smooth ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	return tex;
}

/* where the starport survey is kept, next to the saves */
std::string cache_file()
{
#if defined(__EMSCRIPTEN__)
	return "/saves/FE2Galaxy.cache";
#elif defined(ANDROID)
	const char *dir = SDL_AndroidGetInternalStoragePath();
	return dir ? std::string(dir) + "/FE2Galaxy.cache" : "";
#else
	return "FE2Galaxy.cache";
#endif
}

/* the system the player is in, if the game has one */
uint32_t current_system()
{
	uint32_t id = fe2vm_rd32(fe2vm_a6() + A6_CURRENT_SYSTEM);
	const Star *s = atlas->g.star(id);
	return s && s->id == id ? id : 0;
}

/* first open: the atlas starts on where the player is */
void start()
{
	atlas = new Atlas;
	atlas->texture = gl_texture;
	atlas->button = [](const char *label, float width) { return ui_button(label, width); };
	atlas->in_game = true;
	atlas->survey_ms = 4; /* the game gets the rest of the frame */
	atlas->px = ui_style_px();
	atlas->init(cache_file());
	started = true;
}

void on_open()
{
	atlas->here = current_system();
	if (atlas->here)
	{
		atlas->view.from = atlas->here;
		atlas->view.has_from = true;
		atlas->go_to_star(atlas->here, 60);
	}
}

ImVec2 game_pos(float x, float y)
{
	return ImVec2(Screen_GetGameOffsetX() + x * Screen_GetGameWidth() / 320.0f,
				  Screen_GetGameOffsetY() + y * Screen_GetGameHeight() / 240.0f);
}

void top_bar()
{
	float p = ui_style_px();
	if (ui_button("CLOSE", 34 * p))
		ui_galaxy_show(false);
	ImGui::SameLine();
	if (details)
	{
		if (ui_button("MAP", 34 * p))
			details = false;
		return;
	}
	/* the search box; its results show over the map (below) */
	float right = 3 * (32 * p) + 3 * ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - right);
	if (ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl)
		ImGui::SetKeyboardFocusHere();
	bool enter = ImGui::InputTextWithHint("##find", "Find a system", atlas->query, sizeof(atlas->query),
										  ImGuiInputTextFlags_EnterReturnsTrue);
	if (ImGui::IsItemEdited())
		atlas->start_search();
	if (enter && !atlas->hits.empty())
	{
		atlas->go_to_star(atlas->hits.front().id, 120);
		atlas->query[0] = 0;
		atlas->start_search();
	}
	ImGui::SameLine();
	if (ui_button("VIEW", 32 * p))
		ImGui::OpenPopup("##view");
	ImGui::SameLine();
	if (ui_button("GO", 32 * p))
		ImGui::OpenPopup("##go");
	ImGui::SameLine();
	if (ui_button("INFO", 32 * p))
		ImGui::OpenPopup("##info");
	if (ImGui::BeginPopup("##view"))
	{
		atlas->view_options();
		ImGui::Separator();
		atlas->range_options();
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup("##go"))
	{
		atlas->go_options();
		if (ImGui::BeginMenu("Systems near Sol"))
		{
			atlas->core_list();
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup("##info"))
	{
		ImGui::PushTextWrapPos(200 * p);
		atlas->about_text();
		ImGui::TextDisabled("%s", atlas->status_text().c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndPopup();
	}
}

/* the selected system in one line, under the map */
void bottom_bar()
{
	float p = ui_style_px();
	Atlas &a = *atlas;
	const Star *s = a.view.has_selected ? a.g.star(a.view.selected) : nullptr;
	if (!s)
	{
		ImGui::TextDisabled("Click a star. Drag to move, wheel, pinch or +/- to zoom, H: where people live.");
		return;
	}
	Star st = *s;
	float bw = 48 * p;
	ImGui::BeginGroup();
	ImGui::TextColored(ImVec4(1, 0.53f, 0, 1), "%s", st.name.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("%s", st.description.c_str());
	std::string where = "Sector " + std::to_string(id_x(st.id) - SOL_X) + "," + std::to_string(id_y(st.id) - SOL_Y);
	if (a.view.has_from && a.view.from != st.id)
		if (const Star *f = a.g.star(a.view.from))
			where += "   " + a.g.distance_text(a.g.distance(*f, st)) + " from " + f->name;
	ImGui::TextDisabled("%s", where.c_str());
	ImGui::EndGroup();
	ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bw);
	if (ui_button("DETAILS", bw))
		details = true;
}

} // namespace

extern "C" bool ui_galaxy_open(void)
{
	return open;
}

extern "C" void ui_galaxy_show(bool show)
{
	if (show == open)
		return;
	/* inside the frame's calls the RAM is put back after them, so the
	   game's time can only be restarted once they are over */
	if (!show && drawing)
	{
		close_requested = true;
		return;
	}
	open = show;
	/* pause the game while it's open, as the free camera does */
	if (open)
	{
		saved_time_accel = rdlong(fe2vm_a6() + A6_TIME_ACCEL);
		wrlong(fe2vm_a6() + A6_TIME_ACCEL, 0);
		details = false;
		opening = true;
	}
	else
	{
		wrlong(fe2vm_a6() + A6_TIME_ACCEL, saved_time_accel);
		if (atlas)
		{
			fe2vm_begin();
			atlas->save_cache(true);
			fe2vm_end();
		}
	}
}

void ui_galaxy_draw(void)
{
	if (!open || Screen_GetGameWidth() <= 0 || Screen_GetGameHeight() <= 0)
		return;
	drawing = true;
	fe2vm_begin();
	if (!started)
		start();
	Atlas &a = *atlas;
	a.px = ui_style_px();
	if (opening)
	{
		opening = false;
		on_open();
	}

	ImGui::SetNextWindowPos(game_pos(0, 0));
	ImGui::SetNextWindowSize(ImVec2(Screen_GetGameWidth(), Screen_GetGameHeight()));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2 * a.px, 2 * a.px));
	bool visible = ImGui::Begin("GALAXY ATLAS", nullptr,
								ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse |
									ImGuiWindowFlags_NoScrollbar);
	ImGui::PopStyleVar();
	if (visible)
	{
		if (!a.init_error.empty())
		{
			ImGui::TextWrapped("%s", a.init_error.c_str());
			if (ui_button("CLOSE"))
				ui_galaxy_show(false);
		}
		else
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::GetIO().WantTextInput)
			{
				if (details)
					details = false;
				else
					ui_galaxy_show(false);
			}
			a.update();
			top_bar();
			if (details)
			{
				ImGui::BeginChild("##details", ImVec2(0, 0));
				a.selected_panel();
				ImGui::EndChild();
			}
			else
			{
				ImVec2 pos = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
				float bottom = 2 * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
				ImVec2 size(avail.x, avail.y - bottom);
				a.map(pos, size);
				/* search results over the top of the map */
				if (!a.running_query.empty())
				{
					ImGui::SetCursorScreenPos(ImVec2(pos.x + 4 * a.px, pos.y + 2 * a.px));
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0.13f, 0.92f));
					ImGui::BeginChild("##results", ImVec2(size.x * 0.6f, 0),
									  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
					uint32_t picked = a.view.selected;
					a.search_results(std::min(size.y * 0.6f, 90 * a.px));
					if (a.view.selected != picked)
					{
						a.query[0] = 0; /* went there: close the list */
						a.start_search();
					}
					ImGui::EndChild();
					ImGui::PopStyleColor();
				}
				ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
				bottom_bar();
			}
		}
	}
	ImGui::End();
	fe2vm_end();
	drawing = false;
	if (close_requested)
	{
		close_requested = false;
		ui_galaxy_show(false);
	}
}
