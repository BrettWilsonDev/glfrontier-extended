/*
 * atlas.cpp - see atlas.h. Moved here from tools/fe2GalaxyViewer/app.cpp so
 * the game can use it too.
 */
#include "atlas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fe2vm.h"

using namespace galaxy;

/* the game's colours ($0RGB) and a few plain stand-ins for the hosts' styled
   widgets, so this file needs neither the tool's style.h nor the game's */
static const int COL_ORANGE = 0xf80, COL_RED = 0xe00, COL_YELLOW = 0xfe0, COL_PANEL_LIGHT = 0x999;

static ImVec4 rgb444(int c, float a = 1.0f)
{
	return ImVec4(((c >> 8) & 15) / 15.0f, ((c >> 4) & 15) / 15.0f, (c & 15) / 15.0f, a);
}
static void note(const char *text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_PANEL_LIGHT));
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}
static void heading(const char *text)
{
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_ORANGE));
	ImGui::TextWrapped("%s", text);
	ImGui::PopStyleColor();
}
static bool plain_button(const char *label, float width)
{
	return ImGui::Button(label, ImVec2(width, 0));
}

static ImU32 col(uint32_t rgb, int alpha = 255)
{
	return IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, alpha);
}

static std::string lower(std::string s)
{
	for (auto &c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

/* -- a captured game screen -------------------------------------------------- */

/* Draws text runs at their places on the game's 320 pixel wide screen when
 * there is room (2 pixels per game pixel, like the font), else as lines. */
void Atlas::draw_screen(const Screen &s, const char *id)
{
	if (s.empty())
	{
		note("(nothing)");
		return;
	}
	int top = 1000, bottom = 0, left = 1000, right = 0;
	for (auto &r : s)
	{
		top = std::min(top, r.y);
		bottom = std::max(bottom, r.y + 10);
		left = std::min(left, r.x);
		right = std::max(right, r.x + (int)(ImGui::CalcTextSize(r.text.c_str()).x / px));
	}
	float avail = ImGui::GetContentRegionAvail().x;
	if (avail >= (right - left + 4) * px)
	{
		ImVec2 size((float)(right - left + 4) * px, (float)(bottom - top + 2) * px);
		ImVec2 o = ImGui::GetCursorScreenPos();
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(o, ImVec2(o.x + size.x, o.y + size.y), IM_COL32(0, 0, 34, 255));
		for (auto &r : s)
		{
			ImVec2 p(o.x + (r.x - left + 2) * px, o.y + (r.y - top + 1) * px);
			dl->AddText(ImVec2(p.x + px, p.y + px), IM_COL32(0, 0, 0, 255), r.text.c_str());
			dl->AddText(p, col(r.colour), r.text.c_str());
		}
		ImGui::Dummy(size);
		return;
	}
	/* narrow: one line per y, runs in x order */
	std::vector<const TextRun *> runs;
	for (auto &r : s)
		runs.push_back(&r);
	std::stable_sort(runs.begin(), runs.end(), [](const TextRun *a, const TextRun *b) {
		return a->y != b->y ? a->y < b->y : a->x < b->x;
	});
	ImGui::PushID(id);
	int y = -1;
	for (auto *r : runs)
	{
		if (r->y != y)
			y = r->y;
		else
			ImGui::SameLine(0, 6.0f * px);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col(r->colour)));
		ImGui::TextUnformatted(r->text.c_str());
		ImGui::PopStyleColor();
	}
	ImGui::PopID();
}

bool Atlas::init(const std::string &cache_file)
{
	std::string err;
	if (!g.init(err))
	{
		init_error = err;
		return false;
	}
	view.ly_per_sector = (float)g.ly_per_sector();
	for (int a = 0; a < 4; a++)
		allegiance_names.push_back(g.game_string(0x84b7 + a)); /* l84c04's allegiance strings */
	for (auto &cs : g.core_sectors())
	{
		const Sector &s = g.sector(cs.first, cs.second);
		for (auto &st : s.stars)
			core_ids.push_back(st.id);
	}
	/* the starport survey done so far, next to the exe */
	cache_path = cache_file;
	g.load_cache(cache_path);
	saved_sectors = g.sector_summaries().size();
	/* start on Sol, like the game */
	const Star *sol = g.star(make_id(SOL_X, SOL_Y, 0));
	if (sol)
	{
		view.cx = sol->x / (double)UNITS_PER_SECTOR;
		view.cy = sol->y / (double)UNITS_PER_SECTOR;
		view.select(sol->id);
		view.from = sol->id;
		view.has_from = true;
	}
	view.scale = 60;
	build_density_texture();
	return true;
}

void Atlas::build_density_texture()
{
	/* the smooth galaxy density the game's map background comes from
	   (L8ad0e), one sample per texel */
	const int N = 512, step = SECTORS / N;
	std::vector<int> v(N * N);
	int maxv = 1;
	for (int ty = 0; ty < N; ty++)
		for (int tx = 0; tx < N; tx++)
		{
			int d = g.density(tx * step + step / 2, ty * step + step / 2, false);
			v[ty * N + tx] = d;
			maxv = std::max(maxv, d);
		}
	std::vector<uint32_t> px(N * N);
	for (int i = 0; i < N * N; i++)
	{
		float f = sqrtf(v[i] / (float)maxv);
		/* deep blue -> violet -> warm white, like a galaxy photo */
		float r = std::min(1.0f, f * 1.3f), gch = std::min(1.0f, f * f * 1.1f), b = std::min(1.0f, 0.15f + f * 0.9f);
		int a = (int)(std::min(1.0f, f * 1.6f) * 255);
		px[i] = (uint32_t)(r * 255) | (uint32_t)(gch * 230) << 8 | (uint32_t)(b * 255) << 16 | (uint32_t)a << 24;
	}
	view.density_tex = texture ? texture(px.data(), N, N, 0, true) : 0;
	view.density_size = N;
}

void Atlas::go_to_star(uint32_t id, double min_scale)
{
	if (const Star *s = g.star(id))
	{
		view.select(id);
		view.go_to(s->x / (double)UNITS_PER_SECTOR, s->y / (double)UNITS_PER_SECTOR, std::max(view.scale, min_scale));
	}
}

/* -- search -------------------------------------------------------------------- */

static double now_s()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Atlas::~Atlas()
{
	if (scan_thread.joinable())
		scan_thread.join();
}

void Atlas::start_search()
{
	hits.clear();
	scanned = false;
	running_query = lower(query);
	query_time = running_query.empty() ? -1 : now_s();
	if (running_query.empty())
		return;
	scan_cx = view.cx;
	scan_cy = view.cy;
	/* the hand placed systems at once: their names come from a table */
	for (uint32_t id : core_ids)
	{
		const Star *s = g.star(id);
		if (s && lower(s->name).find(running_query) != std::string::npos)
		{
			double dx = s->x / (double)UNITS_PER_SECTOR - scan_cx, dy = s->y / (double)UNITS_PER_SECTOR - scan_cy;
			hits.push_back({id, s->name, dx * dx + dy * dy});
		}
	}
}

void Atlas::search_step()
{
	/* start the galaxy scan when the typing stops */
	if (!scanning && query_time >= 0 && now_s() - query_time > 0.35 && running_query.size() >= 2)
	{
		query_time = -1;
		scan_query = running_query;
		scanning = true;
		const Galaxy *gp = &g;
		std::string q = scan_query;
		double cx = scan_cx, cy = scan_cy;
		long long *checked = &scan_checked;
#ifdef __EMSCRIPTEN__
		/* no threads there: scan now */
		scan_result = gp->name_candidates(q, cx, cy, 1500, checked);
		scan_done = true;
#else
		scan_done = false;
		scan_thread = std::thread([this, gp, q, cx, cy, checked]() {
			scan_result = gp->name_candidates(q, cx, cy, 1500, checked);
			scan_done = true;
		});
#endif
	}
	if (scanning && scan_done)
	{
		if (scan_thread.joinable())
			scan_thread.join();
		scan_done = false;
		std::vector<uint32_t> cands = std::move(scan_result);
		scanning = false;
		if (scan_query != running_query)
			return; /* typed on since: the next scan starts when the typing stops */
		for (uint32_t id : g.check_names(cands, scan_query, 300))
		{
			const Star *s = g.star(id);
			if (!s || std::any_of(hits.begin(), hits.end(), [&](const SearchHit &h) { return h.id == id; }))
				continue;
			double dx = s->x / (double)UNITS_PER_SECTOR - scan_cx, dy = s->y / (double)UNITS_PER_SECTOR - scan_cy;
			hits.push_back({id, s->name, dx * dx + dy * dy});
		}
		std::sort(hits.begin(), hits.end(), [](const SearchHit &a, const SearchHit &b) { return a.dist2 < b.dist2; });
		scanned = true;
	}
}

void Atlas::search_panel(float list_height)
{
	search_box();
	search_results(list_height);
}

void Atlas::search_box()
{
	ImGui::SetNextItemWidth(-1);
	if (ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl)
		ImGui::SetKeyboardFocusHere();
	bool enter = ImGui::InputTextWithHint("##find", "Find any system by name (Ctrl+F)", query, sizeof(query),
										  ImGuiInputTextFlags_EnterReturnsTrue);
	if (ImGui::IsItemEdited())
		start_search();
	if (enter && !hits.empty())
		go_to_star(hits.front().id, 120);
}

void Atlas::search_results(float list_height)
{
	if (running_query.empty())
		return;
	char buf[128];
	if (scanning || query_time >= 0)
		snprintf(buf, sizeof(buf), "%d found, searching the whole galaxy...", (int)hits.size());
	else if (scanned)
		snprintf(buf, sizeof(buf), "%d found in the whole galaxy, nearest first (Enter goes to the first)",
				 (int)hits.size());
	else
		snprintf(buf, sizeof(buf), "%d found (2 letters or more search the whole galaxy)", (int)hits.size());
	note(buf);
	ImGui::BeginChild("##hits", ImVec2(0, list_height), ImGuiChildFlags_Borders);
	for (auto &h : hits)
	{
		snprintf(buf, sizeof(buf), "%s##%u", h.name.c_str(), h.id);
		if (ImGui::Selectable(buf, view.has_selected && view.selected == h.id))
			go_to_star(h.id, 120);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.45f);
		ImGui::TextDisabled("sector %d,%d   %.0f ly", id_x(h.id) - SOL_X, id_y(h.id) - SOL_Y,
							sqrt(h.dist2) * view.ly_per_sector);
	}
	ImGui::EndChild();
}

/* -- the starport survey ------------------------------------------------------- */

void Atlas::survey_step()
{
	survey_left = 0;
	if (!opt.hue)
		return;
	/* the sectors in view (at most 257 x 257 round the middle), nearest first */
	int cx = (int)view.cx, cy = (int)view.cy;
	int x0 = std::max(view.vx0, cx - 128), x1 = std::min(view.vx1, cx + 128);
	int y0 = std::max(view.vy0, cy - 128), y1 = std::min(view.vy1, cy + 128);
	std::vector<std::pair<int, uint32_t>> todo;
	for (int y = y0; y <= y1; y++)
		for (int x = x0; x <= x1; x++)
			if (!g.has_sector_summary(x, y))
				todo.push_back({(x - cx) * (x - cx) + (y - cy) * (y - cy), (uint32_t)y << 16 | (uint32_t)x});
	survey_left = (int)todo.size();
	if (todo.empty())
		return;
	std::sort(todo.begin(), todo.end());
	auto t0 = std::chrono::steady_clock::now();
	for (auto &t : todo)
	{
		if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > 8)
			break;
		g.sector_summary((int)(t.second & 0xffff), (int)(t.second >> 16));
		survey_left--;
	}
	save_cache(false);
}

void Atlas::save_cache(bool force)
{
	size_t n = g.sector_summaries().size();
	if (cache_path.empty() || n == saved_sectors || (!force && now_s() - last_save < 20))
		return;
	g.save_cache(cache_path);
	saved_sectors = n;
	last_save = now_s();
}

void Atlas::sector_table(int x, int y)
{
	const Sector &s = g.sector(x, y);
	char buf[64];
	snprintf(buf, sizeof(buf), "Sector %d,%d: %d systems", x - SOL_X, y - SOL_Y, (int)s.stars.size());
	heading(buf);
	if (ImGui::BeginTable("##sector", 4,
						  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
							  ImGuiTableFlags_SizingStretchProp,
						  ImVec2(0, 260)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("No.", ImGuiTableColumnFlags_WidthFixed, 30.0f * px);
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Star");
		ImGui::TableSetupColumn("Allegiance");
		ImGui::TableHeadersRow();
		for (auto &st : s.stars)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			snprintf(buf, sizeof(buf), "%d##row%u", id_n(st.id), st.id);
			if (ImGui::Selectable(buf, view.has_selected && view.selected == st.id, ImGuiSelectableFlags_SpanAllColumns))
				view.select(st.id);
			ImGui::TableNextColumn();
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col(st.colour)));
			ImGui::TextUnformatted(st.name.c_str());
			ImGui::PopStyleColor();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(st.description.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(
				st.explored && st.allegiance < (int)allegiance_names.size() ? allegiance_names[st.allegiance].c_str()
																			 : "-");
		}
		ImGui::EndTable();
	}
}

void Atlas::system_panel(const Star &s)
{
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_ORANGE));
	ImGui::TextUnformatted(s.name.c_str());
	ImGui::PopStyleColor();
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col(s.colour)));
	ImGui::TextWrapped("%s", s.description.c_str());
	ImGui::PopStyleColor();
	ImGui::Text("Sector %d,%d   System %d", id_x(s.id) - SOL_X, id_y(s.id) - SOL_Y, id_n(s.id));
	if (view.has_from && view.from != s.id)
	{
		const Star *f = g.star(view.from);
		if (f)
		{
			std::string d = g.distance_text(g.distance(*f, s));
			ImGui::TextWrapped("%s from %s", d.c_str(), f->name.c_str());
		}
	}
	float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
	if ((button ? button : plain_button)("Centre", bw))
		view.go_to(s.x / (double)UNITS_PER_SECTOR, s.y / (double)UNITS_PER_SECTOR);
	ImGui::SameLine();
	if ((button ? button : plain_button)("Measure from", bw))
	{
		view.from = s.id;
		view.has_from = true;
	}
	ImGui::SameLine();
	if ((button ? button : plain_button)("Copy name", bw))
		ImGui::SetClipboardText(s.name.c_str());

	const Details &d = g.details(s.id);
	if (d.hangs || d.crashes)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_YELLOW));
		ImGui::TextWrapped(d.hangs ? "The game's system generator never finishes for this system: it loops forever. "
									 "The game runs the same code for its info icon, the system map and on arrival, "
									 "so it would most likely hang there too."
								   : "The game's system generator crashes on this system (it fills its 60 body "
									 "slots and returns with its stack out of step, to a bad address). The game "
									 "runs the same code for its info icon, the system map and on arrival, so it "
									 "would most likely crash there too.");
		ImGui::PopStyleColor();
		note(d.error.c_str());
	}
	else if (!d.error.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_RED));
		ImGui::TextWrapped("%s", d.error.c_str());
		ImGui::PopStyleColor();
	}
	if (const Summary *sum = g.known_summary(s.id))
		if (sum->ports())
			ImGui::Text("Starports: %d on the surface, %d in orbit", sum->surface_ports, sum->orbital_ports);
	if (!ImGui::BeginTabBar("##systabs"))
		return;
	auto tab = [&](const char *name, int n) {
		bool open = ImGui::BeginTabItem(name, nullptr, system_tab == n ? ImGuiTabItemFlags_SetSelected : 0);
		return open;
	};
	if (tab("System", 0))
	{
		draw_screen(d.system, "sys");
		ImGui::EndTabItem();
	}
	if (tab("Trade", 1))
	{
		draw_screen(d.trade, "trade");
		ImGui::EndTabItem();
	}
	if (tab("Planets", 2))
	{
		planets_panel(d);
		ImGui::EndTabItem();
	}
	if (tab("Sector", 3))
	{
		sector_table(id_x(s.id), id_y(s.id));
		ImGui::EndTabItem();
	}
	if (tab("Game data", 4))
	{
		ImGui::Text("System code   $%08x", s.id);
		ImGui::Text("Position      $%08x $%08x $%08x", (uint32_t)s.x, (uint32_t)s.y, (uint32_t)s.z);
		ImGui::Text("Star type     %d (type byte $%02x)", s.type, s.type_byte);
		ImGui::Text("Companions    %d", s.multiple);
		ImGui::Text("Info byte     $%02x (allegiance %d)", s.info, s.allegiance);
		ImGui::Text("Planet data   %s", s.explored ? "yes" : "no (unexplored)");
		ImGui::Text("Bodies        %d", (int)d.bodies.size());
		ImGui::Spacing();
		note("The system code is what the game stores for a system: (sector y << 19) | (sector x << 6) | "
			 "number. Positions are 16.16 sectors; the game adds $1718,$1524 to the sector numbers it shows.");
		ImGui::EndTabItem();
	}
	system_tab = -1;
	ImGui::EndTabBar();
}

void Atlas::planets_panel(const Details &d)
{
	if (d.bodies.empty())
	{
		draw_screen(d.bodies_screen, "bodies");
		return;
	}
	if (body_sel >= (int)d.bodies.size())
		body_sel = -1;
	if (!ImGui::BeginTabBar("##planettabs"))
		return;
	if (ImGui::BeginTabItem("Game view", nullptr, planets_tab == 0 ? ImGuiTabItemFlags_SetSelected : 0))
	{
		game_view(d);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Orbits", nullptr, planets_tab == 1 ? ImGuiTabItemFlags_SetSelected : 0))
	{
		orbit_view(d);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("List", nullptr, planets_tab == 2 ? ImGuiTabItemFlags_SetSelected : 0))
	{
		body_list(d);
		ImGui::EndTabItem();
	}
	planets_tab = -1;
	ImGui::EndTabBar();
}

/* the details of the selected body, or a note for a surface starport */
void Atlas::body_details(const Details &d, int sel)
{
	if (sel < 0 || sel >= (int)d.bodies.size())
		return;
	const Body &b = d.bodies[sel];
	if (b.surface)
	{
		const char *on = b.parent >= 0 ? d.bodies[b.parent].name.c_str() : "?";
		char buf[200];
		snprintf(buf, sizeof(buf),
				 "%s: a starport on the surface of %s. The game's map has no panel for these; it lists them under "
				 "Major Starports of %s.",
				 b.name.c_str(), on, on);
		note(buf);
	}
	else
		draw_screen(b.details, "body");
}

/* The galaxy map's own system view (l84f02): the icons as the game drew
 * them, clickable where the game registered them (L4231a), with the text
 * of the system or of the clicked body underneath, as in the game. */
void Atlas::game_view(const Details &d)
{
	const Picture &p = d.picture;
	if (p.pixels.size() != 320 * 200)
	{
		draw_screen(d.bodies_screen, "bodies");
		return;
	}
	if (pic_id != d.id || !pic_tex)
	{
		std::vector<uint32_t> rgba(320 * 200);
		for (int i = 0; i < 320 * 200; i++)
		{
			uint32_t c = p.palette[p.pixels[i] & 15];
			rgba[i] = (c >> 16 & 255) | (c & 0xff00) | (c & 255) << 16 | 0xff000000u;
		}
		pic_tex = texture ? texture(rgba.data(), 320, 200, pic_tex, false) : 0;
		pic_id = d.id;
	}
	/* the screen above the control panel, 2 pixels a pixel when it fits */
	const int rows = 168;
	float s = std::min((float)px, ImGui::GetContentRegionAvail().x / 320.0f);
	ImVec2 o = ImGui::GetCursorScreenPos(), size(320 * s, rows * s);
	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->AddImage((ImTextureID)(intptr_t)pic_tex, o, ImVec2(o.x + size.x, o.y + size.y), ImVec2(0, 0),
				 ImVec2(1, rows / 200.0f));
	ImGui::InvisibleButton("##gameview", size);
	bool hovered = ImGui::IsItemHovered(), clicked = ImGui::IsItemClicked();
	ImVec2 m = ImGui::GetIO().MousePos;
	int gx = (int)((m.x - o.x) / s), gy = (int)((m.y - o.y) / s);
	const HitArea *over = nullptr;
	for (auto &h : p.areas)
		if (hovered && gx >= h.x0 && gx <= h.x1 && gy >= h.y0 && gy <= h.y1)
			over = &h;
	auto rect = [&](const HitArea &h, ImU32 c) {
		dl->AddRect(ImVec2(o.x + h.x0 * s, o.y + h.y0 * s), ImVec2(o.x + (h.x1 + 1) * s, o.y + (h.y1 + 1) * s), c);
	};
	for (auto &h : p.areas)
		if (h.key - 0x80 == body_sel)
			rect(h, IM_COL32(255, 136, 0, 255));
	if (over)
	{
		rect(*over, IM_COL32(255, 255, 255, 180));
		int n = over->key - 0x80;
		if (n >= 0 && n < (int)d.bodies.size())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(d.bodies[n].name.c_str());
			ImGui::TextDisabled("%s", d.bodies[n].kind_name.c_str());
			ImGui::EndTooltip();
		}
	}
	if (clicked)
		body_sel = over ? over->key - 0x80 : -1; /* like the game: an icon, or back to the system */
	/* the text the game draws over it */
	const Screen *text = &d.bodies_screen;
	if (body_sel >= 0 && body_sel < (int)d.bodies.size() && !d.bodies[body_sel].surface)
		text = &d.bodies[body_sel].details;
	ImGui::PushClipRect(o, ImVec2(o.x + size.x, o.y + size.y), true);
	for (auto &r : *text)
	{
		ImVec2 q(o.x + r.x * s, o.y + r.y * s);
		dl->AddText(ImVec2(q.x + s, q.y + s), IM_COL32(0, 0, 0, 255), r.text.c_str());
		dl->AddText(q, col(r.colour), r.text.c_str());
	}
	ImGui::PopClipRect();
	note("The game's own system view. Click an icon for that body, or the background for the system.");
}

/* Orbits drawn from the game's numbers for each body (radius, eccentricity
 * and period, as its body screens print them). Where a body is along its
 * orbit is not in those numbers: the game works that out from its clock when
 * you are there, so here everything starts at its nearest point (day 0).
 * Stations orbit too close for the game's 3 decimals ("0.000 A.U."): they go
 * on a small ring round their planet, turning with their real period.
 * Surface starports sit on their planet's rim. */
void Atlas::orbit_view(const Details &d)
{
	const double TWO_PI = 2 * 3.14159265358979;
	if (orbit_id != d.id)
	{
		orbit_id = d.id;
		orbit_focus = 0;
		orbit_days = 0;
		orbit_pan = ImVec2(0, 0);
		orbit_zoom = 1;
	}
	if (orbit_focus >= (int)d.bodies.size())
		orbit_focus = 0;
	auto orbits = [&](const Body &b) { return b.au > 0; }; /* big enough to draw an orbit */
	auto station = [&](const Body &b) { return !b.surface && b.au == 0 && b.period_days > 0; }; /* stations */
	auto children = [&](int i) {
		std::vector<int> v;
		for (auto &b : d.bodies)
			if (b.parent == i)
				v.push_back(b.index);
		return v;
	};
	auto has_orbiters = [&](int i) {
		for (int k : children(i))
			if (orbits(d.bodies[k]))
				return true;
		return false;
	};
	auto set_focus = [&](int i) {
		orbit_focus = i;
		orbit_days = 0;
		orbit_pan = ImVec2(0, 0);
		orbit_zoom = 1;
	};

	/* where we are: Sol > Earth, with a way back up */
	std::string path = d.bodies[orbit_focus].name;
	for (int p = d.bodies[orbit_focus].parent, guard = 0; p >= 0 && guard < 8; p = d.bodies[p].parent, guard++)
		path = d.bodies[p].name + " > " + path;
	ImGui::TextUnformatted(path.c_str());
	if (d.bodies[orbit_focus].parent >= 0)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Up"))
			set_focus(d.bodies[orbit_focus].parent);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Recentre"))
		orbit_pan = ImVec2(0, 0), orbit_zoom = 1;
	std::vector<int> kids = children(orbit_focus);
	double max_period = 1;
	for (int k : kids)
		if (orbits(d.bodies[k]))
			max_period = std::max(max_period, d.bodies[k].period_days);
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
	ImGui::SliderFloat("##days", &orbit_days, 0, (float)max_period, "day %.1f");
	ImGui::SameLine();
	ImGui::Checkbox("Run", &orbit_play);
	ImGui::SameLine();
	ImGui::Checkbox("Log scale", &orbit_log);
	ImGui::Checkbox("Starports", &orbit_ports);
	ImGui::SameLine();
	ImGui::Checkbox("Names", &orbit_names);
	if (orbit_play)
		orbit_days = fmodf(orbit_days + ImGui::GetIO().DeltaTime * (float)max_period / 20.0f, (float)max_period);

	float w = ImGui::GetContentRegionAvail().x, h = std::clamp(w * 0.6f, 280.0f, 420.0f);
	ImVec2 o = ImGui::GetCursorScreenPos();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(0, 0, 20, 255));
	ImGui::InvisibleButton("##orbits", ImVec2(w, h));
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY); /* the wheel zooms here, it does not scroll the panel */
	bool hovered = ImGui::IsItemHovered();
	ImGuiIO &io = ImGui::GetIO();
	if (ImGui::IsItemActive())
		touch_scroll_block = true;
	/* drag to move, wheel to zoom round the mouse */
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2) && !touch_pinching())
		orbit_pan.x += io.MouseDelta.x, orbit_pan.y += io.MouseDelta.y;
	ImVec2 mid(o.x + w / 2, o.y + h / 2);
	/* two fingers: zoom round their midpoint and move with it */
	ImVec2 pc, pp;
	float pz;
	if (touch_pinch_take(o, ImVec2(o.x + w, o.y + h), pc, pz, pp))
	{
		float z = std::clamp(orbit_zoom * pz, 0.02f, 2000.0f), k = z / orbit_zoom;
		ImVec2 rel(pc.x - mid.x - orbit_pan.x, pc.y - mid.y - orbit_pan.y);
		orbit_pan.x += pp.x - rel.x * (k - 1);
		orbit_pan.y += pp.y - rel.y * (k - 1);
		orbit_zoom = z;
	}
	if (hovered && io.MouseWheel != 0)
	{
		float z = std::clamp(orbit_zoom * powf(1.25f, io.MouseWheel), 0.02f, 2000.0f), k = z / orbit_zoom;
		ImVec2 rel(io.MousePos.x - mid.x - orbit_pan.x, io.MousePos.y - mid.y - orbit_pan.y);
		orbit_pan.x -= rel.x * (k - 1);
		orbit_pan.y -= rel.y * (k - 1);
		orbit_zoom = z;
	}
	ImVec2 c(mid.x + orbit_pan.x, mid.y + orbit_pan.y);
	dl->PushClipRect(o, ImVec2(o.x + w, o.y + h), true);

	/* screen radius for a distance in AU */
	double rmax = 0, rmin = 1e9;
	for (int k : kids)
		if (orbits(d.bodies[k]))
		{
			rmax = std::max(rmax, d.bodies[k].au * (1 + d.bodies[k].eccentricity));
			rmin = std::min(rmin, d.bodies[k].au * (1 - d.bodies[k].eccentricity));
		}
	double pix = (std::min(w, h) / 2 - 24) * orbit_zoom, r0 = std::max(rmin * 0.5, 1e-6);
	auto radius = [&](double au) {
		if (rmax <= 0)
			return 0.0;
		return orbit_log ? pix * log(1 + au / r0) / log(1 + rmax / r0) : pix * au / rmax;
	};
	auto place = [&](double x, double y) {
		double r = sqrt(x * x + y * y), k = r > 0 ? radius(r) / r : 0;
		return ImVec2((float)(c.x + x * k), (float)(c.y - y * k));
	};
	auto body_colour = [&](const Body &b) -> uint32_t {
		if (b.surface)
			return 0x40ff80;
		if (b.port)
			return 0x28dcff;
		if (b.kind_name.find("star") != std::string::npos)
			return star_colour(b.kind_name);
		if (b.kind_name.find("gas giant") != std::string::npos)
			return 0xe0a060;
		if (b.kind_name.find("life") != std::string::npos || b.kind_name.find("terraformed") != std::string::npos)
			return 0x60a0ff;
		return 0xa0a0b0;
	};
	auto body_size = [&](const Body &b) -> float {
		if (b.kind_name.find("star") != std::string::npos)
			return 7;
		if (b.kind_name.find("gas giant") != std::string::npos)
			return 5.5f;
		return b.port ? 2.5f : 4;
	};

	int over = -1;
	float over_d2 = 1e9f;
	ImVec2 m = io.MousePos;
	auto hit = [&](int i, ImVec2 q, float r) {
		float dx = m.x - q.x, dy = m.y - q.y, d2 = dx * dx + dy * dy;
		if (hovered && d2 < (r + 5) * (r + 5) && d2 < over_d2)
			over = i, over_d2 = d2;
	};
	/* names go where they don't cover another: the first one there wins */
	std::vector<ImVec4> taken;
	auto label = [&](const std::string &text, ImVec2 at, ImU32 colour) {
		if (!orbit_names)
			return;
		ImVec2 ts = ImGui::CalcTextSize(text.c_str());
		ImVec4 r(at.x - 1, at.y - 1, at.x + ts.x + 1, at.y + ts.y + 1);
		for (auto &t : taken)
			if (r.x < t.z && t.x < r.z && r.y < t.w && t.y < r.w)
				return;
		taken.push_back(r);
		dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 255), text.c_str());
		dl->AddText(at, colour, text.c_str());
	};
	auto draw_body = [&](const Body &b, ImVec2 q, bool named) {
		float r = body_size(b);
		if (b.port)
			dl->AddRectFilled(ImVec2(q.x - r, q.y - r), ImVec2(q.x + r, q.y + r), col(body_colour(b)));
		else
			dl->AddCircleFilled(q, r, col(body_colour(b)), 12);
		if (b.index == body_sel)
			dl->AddCircle(q, r + 4, IM_COL32(255, 136, 0, 255), 16, 2);
		hit(b.index, q, r);
		if (named)
			label(b.name, ImVec2(q.x + r + 3, q.y - 6),
				  b.port ? IM_COL32(150, 220, 190, 255) : IM_COL32(200, 200, 220, 255));
	};
	auto port_count = [&](int i, int &surface, int &orbital) {
		surface = orbital = 0;
		for (int k : children(i))
			if (d.bodies[k].surface)
				surface++;
			else if (d.bodies[k].port)
				orbital++;
	};

	/* The focus in the middle. Its starports: stations on one ring, placed by
	   their period on the day, surface ports round its rim. */
	const Body &f = d.bodies[orbit_focus];
	int fs, fo;
	port_count(orbit_focus, fs, fo);
	bool focus_ports = orbit_ports && orbit_focus != 0 && fs + fo > 0;
	float fr = focus_ports ? 26 : body_size(f) + (orbit_focus ? 8 : 1);
	dl->AddCircleFilled(c, fr, col(body_colour(f)), 32);
	hit(orbit_focus, c, fr);
	if (focus_ports)
	{
		std::vector<int> ks = children(orbit_focus);
		int i_surface = 0, i_station = 0;
		float ring = fr + 30;
		if (fo)
			dl->AddCircle(c, ring, IM_COL32(40, 120, 150, 110), 48);
		for (int k : ks)
		{
			const Body &b = d.bodies[k];
			if (b.surface)
			{
				/* evenly round the rim, names straight out from it */
				double a = TWO_PI * i_surface++ / fs + 0.3;
				ImVec2 p(c.x + (float)cos(a) * fr, c.y - (float)sin(a) * fr);
				draw_body(b, p, false);
				ImVec2 ts = ImGui::CalcTextSize(b.name.c_str());
				float lx = c.x + (float)cos(a) * (fr + 8), ly = c.y - (float)sin(a) * (fr + 8) - ts.y / 2;
				if (cos(a) < 0)
					lx -= ts.x;
				label(b.name, ImVec2(lx, ly), IM_COL32(150, 240, 170, 255));
			}
			else if (station(b))
			{
				/* spread over the ring, turning with its own period */
				double a = TWO_PI * (orbit_days / b.period_days + (double)i_station++ / std::max(1, fo));
				ImVec2 p(c.x + (float)cos(a) * ring, c.y - (float)sin(a) * ring);
				draw_body(b, p, true);
			}
		}
	}
	for (int k : kids)
	{
		const Body &b = d.bodies[k];
		if (!orbits(b))
			continue;
		double a = b.au, e = std::min(b.eccentricity, 0.99), bb = a * sqrt(1 - e * e);
		ImVec2 pts[96];
		for (int i = 0; i < 96; i++)
		{
			double E = i * TWO_PI / 96;
			pts[i] = place(a * (cos(E) - e), bb * sin(E)); /* the focus is at the middle */
		}
		dl->AddPolyline(pts, 96, b.port ? IM_COL32(40, 150, 180, 160) : IM_COL32(70, 90, 140, 200), ImDrawFlags_Closed,
						1.0f);
		/* where it is on the day: Kepler's equation for the mean anomaly */
		double M = b.period_days > 0 ? TWO_PI * orbit_days / b.period_days : 0, E = M;
		for (int it = 0; it < 8; it++)
			E -= (E - e * sin(E) - M) / (1 - e * cos(E));
		ImVec2 q = place(a * (cos(E) - e), bb * sin(E));
		draw_body(b, q, true);
		if (has_orbiters(k))
			dl->AddCircle(q, body_size(b) + 2, IM_COL32(200, 200, 255, 120), 16);
		/* its starports as one small mark and a count: double-click shows them */
		int s_n, o_n;
		port_count(k, s_n, o_n);
		if (orbit_ports && s_n + o_n > 0)
		{
			float r = body_size(b);
			ImVec2 p(q.x + r + 1, q.y + r + 1);
			dl->AddRectFilled(p, ImVec2(p.x + 5, p.y + 5), o_n ? IM_COL32(40, 220, 255, 255) : IM_COL32(60, 255, 110, 255));
			char n[16];
			snprintf(n, sizeof(n), "%d", s_n + o_n);
			label(n, ImVec2(p.x + 7, p.y - 2), o_n ? IM_COL32(40, 220, 255, 255) : IM_COL32(60, 255, 110, 255));
		}
	}
	dl->PopClipRect();

	if (over >= 0)
	{
		const Body &b = d.bodies[over];
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(b.name.c_str());
		ImGui::TextDisabled("%s", b.surface ? "Surface starport" : b.kind_name.c_str());
		if (b.au >= 0 && !b.surface)
			ImGui::Text("%.3f A.U., eccentricity %.3f, %.2f days", b.au, b.eccentricity, b.period_days);
		int ps = 0, po = 0;
		for (int k : children(over))
			ps += d.bodies[k].surface, po += d.bodies[k].port && !d.bodies[k].surface;
		if (ps + po)
			ImGui::Text("Starports: %d on the surface, %d in orbit", ps, po);
		if (has_orbiters(over) && over != orbit_focus)
			ImGui::TextDisabled("double-click for what orbits it");
		ImGui::EndTooltip();
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16)
			body_sel = over;
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && over != orbit_focus && !children(over).empty())
			set_focus(over);
	}
	note("Drag to move, wheel or pinch to zoom, Recentre to fit. Double-click a planet for its moons and starports (squares: "
		 "green on the surface, blue in orbit; a planet's small square and number count them). Names that would "
		 "overlap are left out: hover for them. Sizes, shapes and periods are the game's; the starting places are "
		 "not: the game works them out from its clock when you are in the system, so here everything starts on "
		 "day 0 at its nearest point.");
	body_details(d, body_sel);
}

void Atlas::body_list(const Details &d)
{
	ImGui::BeginChild("##bodylist", ImVec2(0, 220), ImGuiChildFlags_Borders);
	for (auto &b : d.bodies)
	{
		/* indent by how deep it orbits: parents come first in the list */
		int depth = 0;
		for (int p = b.parent, guard = 0; p >= 0 && p < (int)d.bodies.size() && guard < 8;
			 p = d.bodies[p].parent, guard++)
			depth++;
		char buf[128];
		snprintf(buf, sizeof(buf), "%*s%s##body%d", depth * 2, "", b.name.c_str(), b.index);
		if (b.port)
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col(0x60ff90)));
		if (ImGui::Selectable(buf, body_sel == b.index))
			body_sel = b.index;
		if (b.port)
			ImGui::PopStyleColor();
		ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
		ImGui::TextDisabled("%s", b.surface ? "Surface starport" : b.kind_name.c_str());
	}
	ImGui::EndChild();
	body_details(d, body_sel);
}

/* what the star colours mean, in the map's top left corner */
void Atlas::legend(ImVec2 at)
{
	std::vector<std::pair<uint32_t, std::string>> rows;
	if (opt.colour == MapOptions::BY_ALLEGIANCE)
	{
		for (int a = 0; a < 3 && a < (int)allegiance_names.size(); a++)
			rows.push_back({allegiance_colour(a), allegiance_names[a]});
	}
	else if (opt.colour == MapOptions::BY_EXPLORED)
	{
		rows.push_back({0x70e070, "Planet data"});
		rows.push_back({0x806060, "Unexplored"});
	}
	else
		for (const char *w : {"red", "orange", "yellow", "white", "blue", "white dwarf", "brown"})
			rows.push_back({star_colour(w), w}); /* the colour words of the game's star lines */
	if (opt.hue)
	{
		rows.push_back({0x28dcff, "Orbital stations"});
		rows.push_back({0x3cff6e, "Surface starports only"});
		if (survey_left)
			rows.push_back({0x808080, "(surveying " + std::to_string(survey_left) + " sectors)"});
	}
	ImDrawList *dl = ImGui::GetWindowDrawList();
	float lh = ImGui::GetTextLineHeight() + 2, w = 0;
	for (auto &r : rows)
		w = std::max(w, ImGui::CalcTextSize(r.second.c_str()).x);
	ImVec2 a(at.x + 8, at.y + 8), b(a.x + w + 34, a.y + lh * rows.size() + 8);
	dl->AddRectFilled(a, b, IM_COL32(0, 0, 20, 190));
	dl->AddRect(a, b, IM_COL32(80, 80, 110, 200));
	for (size_t i = 0; i < rows.size(); i++)
	{
		ImVec2 p(a.x + 8, a.y + 4 + lh * i);
		dl->AddCircleFilled(ImVec2(p.x + 5, p.y + lh * 0.5f - 1), 4, col(rows[i].first));
		dl->AddText(ImVec2(p.x + 16, p.y), IM_COL32(200, 200, 215, 255), rows[i].second.c_str());
	}
}

void Atlas::tooltip()
{
	if (!view.has_hover || !view.mouse_inside)
		return;
	const Star *s = g.star(view.hovered);
	if (!s)
		return;
	ImGui::BeginTooltip();
	ImGui::PushStyleColor(ImGuiCol_Text, rgb444(COL_ORANGE));
	ImGui::TextUnformatted(s->name.c_str());
	ImGui::PopStyleColor();
	ImGui::TextUnformatted(s->description.c_str());
	ImGui::Text("Sector %d,%d  System %d", id_x(s->id) - SOL_X, id_y(s->id) - SOL_Y, id_n(s->id));
	if (s->explored && s->allegiance < (int)allegiance_names.size())
		ImGui::TextUnformatted(allegiance_names[s->allegiance].c_str());
	else if (!s->explored)
		ImGui::TextUnformatted("Unexplored");
	if (const Summary *sum = g.known_summary(s->id))
	{
		if (sum->ports())
			ImGui::Text("Starports: %d on the surface, %d in orbit", sum->surface_ports, sum->orbital_ports);
		else if (s->explored)
			ImGui::TextDisabled("No starports");
	}
	if (view.has_from && view.from != s->id)
		if (const Star *f = g.star(view.from))
			ImGui::Text("%s from %s", g.distance_text(g.distance(*f, *s)).c_str(), f->name.c_str());
	ImGui::EndTooltip();
}

/* -- per frame, and the map ----------------------------------------------------- */

void Atlas::update()
{
	if (!init_error.empty())
		return;
	ImGuiIO &io = ImGui::GetIO();
	if (!io.WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Home))
			go_to_star(here ? here : make_id(SOL_X, SOL_Y, 0), 60);
		if (ImGui::IsKeyPressed(ImGuiKey_H))
			opt.hue = !opt.hue;
	}
	if (view.selection_changed)
	{
		view.selection_changed = false;
		body_sel = -1;
	}
	search_step();
	survey_step();
	view.update(io.DeltaTime);
}

void Atlas::map(ImVec2 pos, ImVec2 size)
{
	view.draw(g, opt, pos, size);
	/* where the player is */
	ImVec2 p;
	if (here && view.star_screen(g, here, p))
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->PushClipRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), true);
		float r = 6 * std::max(1.0f, px / 2);
		dl->AddTriangleFilled(ImVec2(p.x, p.y - r - 2), ImVec2(p.x - r * 0.6f, p.y - 2 * r - 4),
							  ImVec2(p.x + r * 0.6f, p.y - 2 * r - 4), IM_COL32(255, 230, 60, 255));
		dl->PopClipRect();
	}
	legend(pos);
	tooltip();
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && view.mouse_inside && view.has_hover)
	{
		context_id = view.hovered;
		ImGui::OpenPopup("##starmenu");
	}
	if (ImGui::BeginPopup("##starmenu"))
	{
		if (const Star *s = g.star(context_id))
		{
			heading(s->name.c_str());
			if (ImGui::MenuItem("Select"))
				view.select(context_id);
			if (ImGui::MenuItem("Measure distances from here"))
			{
				view.from = context_id;
				view.has_from = true;
			}
			if (ImGui::MenuItem("Zoom in"))
				view.go_to(s->x / (double)UNITS_PER_SECTOR, s->y / (double)UNITS_PER_SECTOR,
						   std::max(view.scale * 4, 200.0));
		}
		ImGui::EndPopup();
	}
	g.trim_cache(20000);
}

void Atlas::selected_panel()
{
	if (view.has_selected)
	{
		if (const Star *s = g.star(view.selected))
		{
			Star copy = *s; /* details() can reload the cache */
			system_panel(copy);
			return;
		}
	}
	note("Click a star to see what the game knows about it.");
}

void Atlas::core_list()
{
	for (uint32_t id : core_ids)
	{
		const Star *s = g.star(id);
		if (!s)
			continue;
		char buf[80];
		snprintf(buf, sizeof(buf), "%s##core%u", s->name.c_str(), id);
		if (ImGui::Selectable(buf, view.has_selected && view.selected == id))
			go_to_star(id, 120);
	}
}

std::string Atlas::status_text()
{
	char buf[200];
	snprintf(buf, sizeof(buf), "Sector %d,%d   %d sectors in memory, %d surveyed%s",
			 (int)floor(view.mouse_x) - SOL_X, (int)floor(view.mouse_y) - SOL_Y, (int)g.cache_size(),
			 (int)g.sector_summaries().size(), view.loading ? "   (loading...)" : "");
	return buf;
}

/* -- menu contents ------------------------------------------------------------------ */

void Atlas::view_options()
{
	float w = 80 * px;
	ImGui::Checkbox("Where people live (starports)", &opt.hue);
	ImGui::SameLine();
	ImGui::TextDisabled("H");
	ImGui::Checkbox("Galaxy density", &opt.density);
	ImGui::Checkbox("Star counts per sector", &opt.counts);
	ImGui::Checkbox("Sector grid", &opt.grid);
	ImGui::Checkbox("Names", &opt.names);
	ImGui::Separator();
	ImGui::TextUnformatted("Colour stars by");
	ImGui::RadioButton("Star colour", &opt.colour, MapOptions::BY_STAR);
	ImGui::RadioButton("Allegiance", &opt.colour, MapOptions::BY_ALLEGIANCE);
	ImGui::RadioButton("Explored", &opt.colour, MapOptions::BY_EXPLORED);
	ImGui::Separator();
	ImGui::SetNextItemWidth(w);
	ImGui::SliderFloat("Tilt", &opt.tilt, 0, 75, "%.0f deg");
	ImGui::Checkbox("Height stalks", &opt.stalks);
	ImGui::SetNextItemWidth(w);
	ImGui::SliderFloat("Star size", &opt.star_size, 0.5f, 3.0f, "%.1f");
}

void Atlas::go_options()
{
	if (here && ImGui::MenuItem("Where you are", "Home"))
		go_to_star(here, 60);
	if (ImGui::MenuItem("Sol", here ? nullptr : "Home"))
		go_to_star(make_id(SOL_X, SOL_Y, 0), 60);
	if (ImGui::MenuItem("Whole galaxy"))
		view.go_to(SECTORS / 2.0, SECTORS / 2.0, 0.09);
	if (ImGui::MenuItem("Selected system", nullptr, false, view.has_selected))
		go_to_star(view.selected, 120);
	ImGui::Separator();
	ImGui::TextUnformatted("Sector (Sol = 0,0)");
	ImGui::SetNextItemWidth(45 * px);
	ImGui::InputInt("x##goto", &goto_x, 1, 10);
	ImGui::SetNextItemWidth(45 * px);
	ImGui::InputInt("y##goto", &goto_y, 1, 10);
	if (ImGui::MenuItem("Go to sector"))
		view.go_to(SOL_X + goto_x + 0.5, SOL_Y + goto_y + 0.5, std::max(view.scale, 80.0));
}

void Atlas::range_options()
{
	note("A circle round the \"from\" star (green). Right-click a star to make it the from star.");
	ImGui::SetNextItemWidth(80 * px);
	ImGui::SliderFloat("Light years", &opt.range_ly, 0, 60, opt.range_ly > 0 ? "%.1f" : "off");
}

void Atlas::about_text()
{
	note("Drag to move, mouse wheel or pinch to zoom, click a star to select it, double-click to zoom in on it, "
		 "right-click for more. Arrow keys move, +/- zoom, H shows where people live.");
	ImGui::Separator();
	note("Everything shown comes from running Frontier's own code: the sectors from L8444a, positions from "
		 "L83d3c, names and star lines from L84700, allegiance from L4beee, distances from L84ae6, the density "
		 "from L8ad0e, and the info screens are the galaxy map's own (l84c04, l84b34, l84f02, l84cde) with "
		 "their text captured as the game draws it.");
}
