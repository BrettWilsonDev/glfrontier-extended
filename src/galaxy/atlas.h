/*
 * atlas.h - the galaxy atlas's ImGui parts, shared by the standalone viewer
 * (tools/fe2GalaxyViewer) and the game (src/ui/ui_galaxy.cpp): the map with
 * its legend, tooltip and right-click menu, the search, the starport survey,
 * and the system panels (the game's own System, Trade and Planets screens,
 * the orbit view, the sector's systems). The host lays them out and gives
 * the atlas a way to make GL textures.
 *
 * Everything here runs game code (galaxy.h): inside the running game the
 * host wraps each frame's use of the atlas in fe2vm_begin() / fe2vm_end().
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "galaxy.h"
#include "imgui.h"
#include "map_view.h"

struct SearchHit
{
	uint32_t id;
	std::string name;
	double dist2; /* sectors^2 from where the search started */
};

class Atlas
{
  public:
	/* -- set by the host --------------------------------------------------- */
	/* makes or updates (old != 0) an RGBA texture; returns its GL name */
	std::function<unsigned int(const uint32_t *rgba, int w, int h, unsigned int old, bool smooth)> texture;
	float px = 2;			   /* screen pixels per game pixel: the game's screens are drawn at this size */
	float survey_ms = 8;	   /* time per frame for the starport survey */
	uint32_t here = 0;		   /* the system the player is in (in the game), 0 = none */
	bool in_game = false;
	bool (*button)(const char *label, float width) = nullptr; /* the host's styled button, else ImGui's */

	galaxy::Galaxy g;
	MapView view;
	MapOptions opt;
	std::string init_error;
	std::vector<std::string> allegiance_names;
	std::vector<uint32_t> core_ids;

	/* search: the hand placed systems at once, then the whole galaxy (the
	   name hash scan runs on a thread, the game checks what it finds) */
	char query[64] = "";
	std::string running_query, scan_query;
	std::vector<SearchHit> hits;
	/* a plain thread and a flag, not std::async: std::future needs call_once
	   and TLS from the runtime, which clang with MinGW's libstdc++ gets wrong */
	std::thread scan_thread;
	std::atomic<bool> scan_done{false};
	std::vector<uint32_t> scan_result;
	bool scanning = false, scanned = false;
	double scan_cx = 0, scan_cy = 0, query_time = -1;
	long long scan_checked = 0;

	/* the starport survey for the overlay, and where it is saved */
	std::string cache_path;
	size_t saved_sectors = 0;
	double last_save = 0;
	int survey_left = 0;

	int goto_x = 0, goto_y = 0;
	int body_sel = -1;					   /* -1: the system's text on the game view */
	int system_tab = -1, planets_tab = -1; /* tabs to open next frame */
	uint32_t context_id = 0;

	/* the Planets tab */
	unsigned int pic_tex = 0;
	uint32_t pic_id = 0;
	int orbit_focus = 0;
	float orbit_days = 0, orbit_zoom = 1;
	ImVec2 orbit_pan = ImVec2(0, 0); /* pixels, dragged */
	bool orbit_log = true, orbit_play = false, orbit_ports = true, orbit_names = true;
	uint32_t orbit_id = 0;

	Atlas() = default;
	~Atlas();
	Atlas(const Atlas &) = delete;
	Atlas &operator=(const Atlas &) = delete;

	bool init(const std::string &cache_file);
	/* once a frame, before drawing: keys, search, survey, the map's easing */
	void update();
	/* the map, with its legend, tooltip and right-click menu */
	void map(ImVec2 pos, ImVec2 size);

	void search_panel(float list_height); /* search_box and search_results */
	void search_box();
	void search_results(float list_height);
	void selected_panel(); /* the selected system, or a hint */
	void system_panel(const galaxy::Star &s);
	void core_list();
	void sector_table(int x, int y);
	std::string status_text();

	/* the contents of the host's menus */
	void view_options();
	void go_options();
	void range_options();
	void about_text();

	void go_to_star(uint32_t id, double min_scale);
	void start_search();
	void save_cache(bool force);
	void draw_screen(const galaxy::Screen &s, const char *id);

  private:
	void build_density_texture();
	void search_step();
	void survey_step();
	void planets_panel(const galaxy::Details &d);
	void game_view(const galaxy::Details &d);
	void orbit_view(const galaxy::Details &d);
	void body_list(const galaxy::Details &d);
	void body_details(const galaxy::Details &d, int sel);
	void tooltip();
	void legend(ImVec2 at);
};
