/*
 * app.cpp - FE2 Ship Builder: make a Frontier: Elite 2 ship from start to
 * finish in one window, and put it in the game. Everything the studio needs
 * (the game's models, its font) is compiled in, so FE2ShipBuilder.exe runs on
 * its own.
 *
 * View: right drag = turn, middle drag (or shift + right drag) = pan, wheel =
 * zoom. Left click selects (shift adds), dragging the selection moves it,
 * shift + drag on empty space box-selects. Keys: 1/2 vertex/face mode, A all,
 * F frame, X/Y/Z lock moves to an axis, Delete, Ctrl+Z / Ctrl+Y.
 */
#include <SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "fe2build.h"
#include "fe2core.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "mesh.h"
#include "style.h"
#include "util.h"
#include "view.h"

using namespace studio;
using fe2::Vec3;

namespace
{

std::string fmt(const char *f, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, f);
	vsnprintf(buf, sizeof(buf), f, ap);
	va_end(ap);
	return buf;
}

const char *PRIMS[] = {"box", "wedge", "cylinder", "cone", "pyramid", "sphere"};

struct ModEntry
{
	std::string path, file;
	bool enabled;
	fe2::ModelBlobs models;
	fe2::Fe2mMeta meta;
	int index = -1;
	std::string label;
};

struct App
{
	const fe2::Image &image = fe2::Image::game();
	std::vector<std::pair<int, std::string>> ship_list;
	std::vector<std::vector<int>> lists;
	Mesh mesh;

	/* the ship being made */
	bool new_ship = true;
	int base = 26;
	char name[41] = "My Ship";
	int slot = 240;		 /* model number of a new ship: handed out by the studio, see pick_slot() */
	std::string editing; /* the ships folder file this ship was loaded from or saved to ("" = not saved yet) */
	std::map<std::string, int> stats;
	int picks[32] = {};
	int collision = 0;
	bool keep_extras = true, fix_normals = false;
	int paint = 0x666;
	char paint_hex[4] = "666";

	/* parts of the game model we started from */
	int parts_model = -1;
	std::vector<GamePart> parts;
	std::vector<char> parts_on;

	/* view */
	Camera cam;
	Renderer ren;
	ViewOptions vopt;
	bool show_game = false;
	std::vector<DrawFace> game_faces;

	/* selection */
	bool vert_mode = false;
	std::set<int> sel;
	std::vector<int> pick_order;
	bool symmetric = true;
	int axis_lock = -1;

	/* tools */
	int cr = 4, cg = 4, cb = 4;
	bool cglow = false, cpaint = false, c2s = false;
	int prim = 0, prim_sides = 8;
	float prim_size[3] = {4, 2, 8}, prim_pos[3] = {0, 0, 0};
	bool prim_mirror = true;
	float t_move[3] = {0, 0, 0}, t_scale[3] = {1, 1, 1}, t_deg = 90, extrude_dist = 1, want_len = 30, weld_dist = 0.01f;
	int t_axis = 1;

	/* drag */
	enum
	{
		D_NONE,
		D_ORBIT,
		D_PAN,
		D_MOVE,
		D_BOX,
		D_MOVE_WAIT
	} drag = D_NONE;
	ImVec2 drag_start, drag_last;

	/* windows */
	bool show_lib = false, show_mods = false, show_code = false, show_help = false;
	char lib_filter[64] = "";
	int lib_sel = -1;
	std::string code_text;
	std::vector<ModEntry> mods;
	int mods_sel = -1;
	int goto_tab = -1;

	std::vector<std::string> log_lines;
	bool log_scroll = false;
	std::string ships_dir, game_exe, blender_path;
	std::map<int, Vec3> base_sizes;

	void log(const std::string &s)
	{
		size_t a = 0;
		while (a <= s.size())
		{
			size_t b = s.find('\n', a);
			if (b == std::string::npos)
				b = s.size();
			log_lines.push_back(s.substr(a, b - a));
			a = b + 1;
		}
		if (log_lines.size() > 400)
			log_lines.erase(log_lines.begin(), log_lines.begin() + (log_lines.size() - 400));
		log_scroll = true;
	}

	std::string ship_label(int idx) const
	{
		auto m = image.model(idx);
		std::string n = m ? m->ship_name() : "";
		return fmt("%03d %s", idx, n.empty() ? "(model)" : n.c_str());
	}

	std::map<std::string, int> base_stats() const
	{
		auto m = image.model(base);
		return m && m->ship_off ? fe2::read_fields(m->ship_data(32)) : std::map<std::string, int>();
	}

	Vec3 base_size()
	{
		auto it = base_sizes.find(base);
		if (it != base_sizes.end())
			return it->second;
		std::set<std::string> none;
		Mesh m = Mesh::from_game(image, base, false);
		Vec3 s = m.verts.empty() ? Vec3() : m.size();
		base_sizes[base] = s;
		return s;
	}

	void init()
	{
		ship_list = fe2::ships(image);
		lists = fe2::picker_lists(image);
		std::string dir = exe_dir();
		/* run from tools/fe2ShipBuilder/build: use tools/fe2ShipBuilder/custom_ships and the repo's build/GLFrontier.exe,
		   else both sit beside the studio */
		std::string repo = normal_path(join(dir, "../../../"));
		if (file_exists(join(repo, "fe2/fe2_modded.s")))
		{
			ships_dir = normal_path(join(repo, "tools/fe2ShipBuilder/custom_ships"));
			game_exe = normal_path(join(repo, "build/GLFrontier.exe"));
		}
		else
		{
			ships_dir = normal_path(join(dir, "custom_ships"));
			game_exe = join(dir, "GLFrontier.exe");
		}
		set_base(26);
		fresh_ship();
		start_from_example();
		log("FE2 Ship Builder. Work down the steps on the left; the view is on the right.");
		log("Your ships are saved in " + ships_dir);
		if (free_slot() < 0)
			log("Every new ship number (240-511) is taken: delete a ship in File > Your ships to make a new one.");
		else
			log(fmt("A new ship gets model number %d (the studio picks free numbers, see step 6).", slot));
	}

	/* -- model numbers ---------------------------------------------------------------------------
	 * New ship types need their own model number, from 240 up to 511. Each ship file is named
	 * after its number, and big ships put extra parts at numbers from 511 down. The studio hands
	 * these out itself: a new ship gets the first free number, a ship loaded from the folder keeps
	 * its own, and saving never overwrites a different ship. */

	/* model numbers the ship files use, and who has each; skip1 / skip2 are left out (the file
	   being edited, the file being saved over). Turned off files count, so turning them back on
	   can't clash. */
	std::map<int, std::string> used_slot_owners(const std::string &skip1 = "", const std::string &skip2 = "")
	{
		std::map<int, std::string> used;
		for (auto &f : list_dir(ships_dir))
		{
			if ((!skip1.empty() && f == skip1) || (!skip2.empty() && f == skip2) ||
				f.find(".fe2m") == std::string::npos)
				continue;
			fe2::ModelBlobs ms;
			fe2::Fe2mMeta meta;
			if (!fe2::read_fe2m(join(ships_dir, f), ms, meta) || ms.empty())
				continue;
			int first = ms[0].first;
			std::string who = first >= fe2::SHIP_SLOT_FIRST ? "'" + meta.name + "'" : "the new look for " + ship_label(first);
			who += " (" + f + ")";
			for (auto &m : ms)
				used[m.first] = m.first == first ? who : "a part of " + who;
			/* the file name's number is taken too: saving there would overwrite this file */
			int named = atoi(f.c_str());
			if (f.size() > 3 && isdigit((unsigned char)f[0]) && !used.count(named))
				used[named] = who;
		}
		return used;
	}

	std::set<int> used_slots(const std::string &skip1 = "", const std::string &skip2 = "")
	{
		std::set<int> used;
		for (auto &kv : used_slot_owners(skip1, skip2))
			used.insert(kv.first);
		return used;
	}

	/* used_slot_owners(editing), read again at most once a second (for the UI) */
	std::map<int, std::string> slot_cache;
	std::string slot_cache_for = "?";
	double slot_cache_time = -10;
	const std::map<int, std::string> &cached_owners()
	{
		if (slot_cache_for != editing || ImGui::GetTime() - slot_cache_time > 1.0)
		{
			slot_cache = used_slot_owners(editing);
			slot_cache_for = editing;
			slot_cache_time = ImGui::GetTime();
		}
		return slot_cache;
	}

	/* first free new ship number, -1 when all are taken */
	int free_slot()
	{
		std::set<int> used = used_slots(editing);
		for (int n = fe2::SHIP_SLOT_FIRST; n <= fe2::SHIP_SLOT_LAST; n++)
			if (!used.count(n))
				return n;
		return -1;
	}

	/* a fresh ship, not saved yet: give it a free number */
	void fresh_ship()
	{
		editing.clear();
		int n = free_slot();
		slot = n >= 0 ? n : fe2::SHIP_SLOT_LAST;
	}

	/* make sure slot is still free before a build; false when every number is taken */
	bool pick_slot()
	{
		auto used = used_slot_owners(editing);
		if (!used.count(slot) && slot >= fe2::SHIP_SLOT_FIRST && slot <= fe2::SHIP_SLOT_LAST)
			return true;
		int n = free_slot();
		if (n < 0)
		{
			log("Every new ship number (240-511) is used. Delete a ship in File > Your ships (turned off "
				"ones count too), or load one there and save over it.");
			return false;
		}
		if (used.count(slot))
			log(fmt("model number %d is %s, so this ship gets %d", slot, used[slot].c_str(), n));
		slot = n;
		return true;
	}

	/* the file a model number saves to: the one being edited when it has that number (a turned off
	   file stays off), else NNN.fe2m */
	std::string file_for(int index)
	{
		std::string file = fmt("%03d.fe2m", index);
		if (!editing.empty() && editing.compare(0, file.size(), file) == 0)
			return editing;
		return file;
	}

	void set_base(int b)
	{
		base = b;
		stats = base_stats();
		for (int c = 0; c < 32; c++)
			picks[c] = 0;
		for (int c = 0; c < (int)lists.size(); c++)
			if (std::find(lists[c].begin(), lists[c].end(), base) != lists[c].end())
				picks[c] = 1;
		auto m = image.model(base);
		if (m)
		{
			paint = m->colour;
			snprintf(paint_hex, sizeof(paint_hex), "%03x", paint);
		}
	}

	/* -- loading ------------------------------------------------------------------ */
	void changed()
	{
		show_game = false;
		game_faces.clear();
	}

	void after_load()
	{
		sel.clear();
		pick_order.clear();
		changed();
		frame_all();
		if (!mesh.hulls.empty())
			collision = 4; /* custom */
	}

	void start_from_example()
	{
		/* the example dart, built in */
		fresh_ship();
		mesh = Mesh();
		std::vector<Vec3> v = {{0, 0.8, -11}, {0, 2.2, 2},	   {8, 0, 6},	 {-8, 0, 6},	 {0, -1.2, 3}, {0, 0.9, 7},
							   {2.2, 0.1, 7}, {-2.2, 0.1, 7}, {0, -0.8, 7}, {0, 1.7, -4}, {0.9, 1.2, -2}, {-0.9, 1.2, -2}};
		mesh.verts = v;
		auto F = [&](std::vector<int> f, int c) {
			for (int &i : f)
				i--;
			std::reverse(f.begin(), f.end());
			mesh.faces.push_back({f, mat_name(c, false)});
		};
		int hull = make_colour(6, 1, 1, false, false), belly = make_colour(1, 1, 1, false, false),
			glow = make_colour(7, 4, 1, true, false), canopy = make_colour(1, 6, 7, true, false);
		F({1, 3, 2}, hull);
		F({1, 2, 4}, hull);
		F({2, 3, 6}, hull);
		F({4, 2, 6}, hull);
		F({3, 7, 6}, hull);
		F({4, 6, 8}, hull);
		F({1, 5, 3}, belly);
		F({1, 4, 5}, belly);
		F({5, 9, 3}, belly);
		F({3, 9, 7}, belly);
		F({4, 5, 9}, belly);
		F({4, 9, 8}, belly);
		F({6, 7, 9, 8}, glow);
		F({10, 11, 12}, canopy);
		mesh.recalc_outside();
		after_load();
	}

	void start_from_game(int idx, bool gear)
	{
		fresh_ship();
		mesh = Mesh::from_game(image, idx, gear);
		log(fmt("started from %s (%d faces)", ship_label(idx).c_str(), (int)mesh.faces.size()));
		show_parts(idx, gear);
		after_load();
	}

	void show_parts(int idx, bool gear)
	{
		parts_model = idx;
		parts = game_parts(image, idx);
		parts_on.clear();
		for (auto &p : parts)
			parts_on.push_back(p.on || gear);
	}

	void reload_parts()
	{
		std::set<std::string> keys;
		for (size_t i = 0; i < parts.size(); i++)
			if (parts_on[i])
				keys.insert(parts[i].key);
		mesh = Mesh::from_game(image, parts_model, false, &keys);
		log(fmt("reloaded %s with %d part(s): %d faces", ship_label(parts_model).c_str(), (int)keys.size(),
				(int)mesh.faces.size()));
		after_load();
	}

	void open_obj()
	{
		std::string p = open_dialog("Open an OBJ (from Blender)", "OBJ files|*.obj|All files|*.*");
		if (!p.empty())
		{
			fresh_ship();
			load_obj(p);
		}
	}

	void load_obj(const std::string &p)
	{
		std::string err;
		Mesh m;
		if (!Mesh::from_obj(p, m, err))
		{
			log("couldn't open: " + err);
			return;
		}
		mesh = m;
		parts_model = -1;
		log(fmt("opened %s: %d faces, %d collision hulls", p.c_str(), (int)mesh.faces.size(), (int)mesh.hulls.size()));
		after_load();
	}

	void save_obj()
	{
		std::string p = save_dialog("Save an OBJ for Blender", "OBJ files|*.obj", "my_ship.obj", "obj");
		if (p.empty())
			return;
		std::string err;
		if (mesh.save_obj(p, err))
		{
			blender_path = p;
			log("saved " + p + " - open it in Blender (File > Import > Wavefront), edit, export over the same file, "
							   "then press Reload from Blender");
		}
		else
			log(err);
	}

	/* -- camera ---------------------------------------------------------------------- */
	void frame_all()
	{
		std::vector<Vec3> pts;
		for (int i : mesh.used_verts())
			pts.push_back(mesh.verts[i]);
		for (auto &h : mesh.hulls)
			pts.insert(pts.end(), h.verts.begin(), h.verts.end());
		if (pts.empty())
		{
			cam.target = Vec3();
			cam.dist = 40;
			return;
		}
		Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
		for (auto &p : pts)
			for (int a = 0; a < 3; a++)
			{
				lo[a] = std::min(lo[a], p[a]);
				hi[a] = std::max(hi[a], p[a]);
			}
		cam.target = (lo + hi) * 0.5;
		cam.dist = std::max(5.0, std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z}) * 1.6);
	}

	/* -- selection ---------------------------------------------------------------------- */
	std::set<int> sel_verts() const
	{
		if (vert_mode)
			return sel;
		std::set<int> out;
		for (int k : sel)
			if (k < (int)mesh.faces.size())
				out.insert(mesh.faces[k].v.begin(), mesh.faces[k].v.end());
		return out;
	}

	void set_mode(bool verts)
	{
		vert_mode = verts;
		sel.clear();
		pick_order.clear();
	}

	void select_all()
	{
		sel.clear();
		if (vert_mode)
			sel = mesh.used_verts();
		else
			for (int k = 0; k < (int)mesh.faces.size(); k++)
				sel.insert(k);
	}

	void delete_sel()
	{
		if (sel.empty())
			return;
		mesh.checkpoint();
		if (vert_mode)
			mesh.delete_verts(sel);
		else
			mesh.delete_faces(sel);
		sel.clear();
		changed();
	}

	std::string cur_mat() const { return mat_name(make_colour(cr, cg, cb, cglow, cpaint), c2s); }

	/* -- build and play ---------------------------------------------------------------------- */
	bool build(bool install)
	{
		if (mesh.faces.empty())
		{
			log("nothing to build - start from something on step 1");
			return false;
		}
		auto tmpl = image.model(base);
		if (!tmpl)
			return false;
		if (new_ship && !tmpl->ship_off)
		{
			log("a new ship has to be based on a ship");
			return false;
		}
		if (new_ship && !pick_slot())
			return false;
		int index = new_ship ? slot : base;
		std::string file = file_for(index);
		fe2::BuildOptions opt;
		opt.index = index;
		opt.keep_extras = keep_extras;
		opt.fix_normals = fix_normals;
		opt.collision = fe2::COLLISION_MODES[collision];
		opt.colour = paint;
		/* parts can reuse numbers from the file being saved over, never another ship's */
		opt.used_slots = used_slots(editing, file);
		auto bstats = base_stats();
		for (auto &kv : stats)
			if (kv.first != "name_id" && bstats[kv.first] != kv.second)
				opt.ship_values[kv.first] = kv.second;
		fe2::BuildResult res;
		try
		{
			res = fe2::build_model(mesh.to_build(), &*tmpl, image, opt);
		}
		catch (std::exception &e)
		{
			log(std::string("BUILD FAILED: ") + e.what());
			return false;
		}
		log(fmt("%s: %d faces, %d vertices, %d normals, scale %d (1 unit = %d mm), %d bytes",
				new_ship ? fmt("new ship %d '%s' based on %s", index, name, ship_label(base).c_str()).c_str()
						 : fmt("new look for %s", ship_label(base).c_str()).c_str(),
				res.faces, res.verts, res.norms, res.scale, 1 << res.scale, res.bytes));
		if (res.parts > 1)
			log(fmt("  split into %d parts (models %d-%d)", res.parts, res.slots.front(), res.slots.back()));
		log("  collision: " + res.collision + (res.extras ? ", kept the base ship's engine glow and lights" : ""));
		for (auto &w : res.warnings)
			log("  warning: " + w);
		if (install)
		{
			make_dirs(ships_dir);
			fe2::Fe2mMeta meta;
			if (new_ship)
			{
				meta.name = name;
				meta.like = base;
				for (int c = 0; c < (int)lists.size(); c++)
					if (picks[c])
						meta.picks.push_back({c, picks[c]});
			}
			if (!new_ship && editing != file && file_exists(join(ships_dir, file)))
				log("  this replaces your earlier new look for " + ship_label(base) + " (" + file + ")");
			try
			{
				fe2::write_fe2m(join(ships_dir, file), res.models, new_ship ? &meta : nullptr);
				log(">> saved " + join(ships_dir, file) + ". Press SAVE THE GAME to put it into GLFrontier.exe.");
				/* a new ship moved to another number: its old file goes */
				if (new_ship && !editing.empty() && editing != file && atoi(editing.c_str()) >= fe2::SHIP_SLOT_FIRST &&
					file_exists(join(ships_dir, editing)))
				{
					std::remove(join(ships_dir, editing).c_str());
					log("  removed the old " + editing);
				}
				editing = file;
				slot_cache_time = -10;
				refresh_mods();
			}
			catch (std::exception &e)
			{
				log(e.what());
			}
		}
		/* what the game will draw */
		fe2::Image sub = image.with_models(res.models);
		auto c = fe2::collect(sub, index);
		game_faces.clear();
		for (auto &f : c.faces)
		{
			if (f.group.find("~far") != std::string::npos)
				continue;
			DrawFace d;
			for (auto &p : f.pts)
				d.pts.push_back(Vec3(p.x / 1000, p.y / 1000, -p.z / 1000));
			std::optional<Vec3> nn;
			if (f.normal)
				nn = Vec3(f.normal->x, f.normal->y, -f.normal->z);
			d.pts = fe2::orient(d.pts, nn);
			d.colour = f.colour;
			d.two_sided = !f.normal;
			d.selected = false;
			d.id = -1;
			game_faces.push_back(d);
		}
		show_game = true;
		return true;
	}

	std::vector<std::pair<std::string, std::vector<uint8_t>>> enabled_mod_files()
	{
		std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
		for (auto &f : list_dir(ships_dir))
		{
			if (f.size() < 5 || f.compare(f.size() - 5, 5, ".fe2m") != 0)
				continue;
			std::vector<uint8_t> d;
			if (read_file(join(ships_dir, f), d))
				files.push_back({f, d});
		}
		return files;
	}

	void save_game()
	{
		if (!file_exists(game_exe))
		{
			std::string p = open_dialog("Where is GLFrontier.exe?", "GLFrontier|GLFrontier.exe|Programs|*.exe");
			if (p.empty())
				return;
			game_exe = p;
		}
		auto files = enabled_mod_files();
		std::string err;
		if (write_game_with_ships(game_exe, game_exe, files, err))
			log(fmt(">> %s now has %d ship file(s) built in - it's still a single standalone exe.", game_exe.c_str(),
					(int)files.size()));
		else
			log("couldn't save the game: " + err);
	}

	void launch_game()
	{
		if (!file_exists(game_exe))
		{
			log("can't find " + game_exe + " (Tools > Save the game... asks where it is)");
			return;
		}
		std::string err;
		if (launch(game_exe, err))
			log("started " + game_exe);
		else
			log(err);
	}

	/* -- mods ---------------------------------------------------------------------------------- */
	void refresh_mods()
	{
		mods.clear();
		for (auto &f : list_dir(ships_dir))
		{
			bool on = f.size() > 5 && f.compare(f.size() - 5, 5, ".fe2m") == 0;
			bool off = f.size() > 9 && f.compare(f.size() - 9, 9, ".fe2m.off") == 0;
			if (!on && !off)
				continue;
			ModEntry e;
			e.path = join(ships_dir, f);
			e.file = f;
			e.enabled = on;
			if (!fe2::read_fe2m(e.path, e.models, e.meta) || e.models.empty())
			{
				e.label = f + " (broken)";
				mods.push_back(e);
				continue;
			}
			e.index = e.models[0].first;
			bool is_new = e.index >= fe2::SHIP_SLOT_FIRST;
			e.label = fmt("%s %03d %-9s %s", on ? "on " : "off", e.index, is_new ? "new ship" : "new look",
						  is_new ? e.meta.name.c_str() : ship_label(e.index).c_str() + 4);
			mods.push_back(e);
		}
	}

	void mod_load(ModEntry &e)
	{
		fe2::Image sub = image.with_models(e.models);
		mesh = Mesh::from_game(sub, e.index, true);
		bool is_new = e.index >= fe2::SHIP_SLOT_FIRST;
		new_ship = is_new;
		editing = e.file;
		if (is_new)
		{
			slot = e.index;
			snprintf(name, sizeof(name), "%s", e.meta.name.c_str());
			if (e.meta.like >= 0)
				set_base(e.meta.like);
			for (int c = 0; c < 32; c++)
				picks[c] = 0;
			for (auto &cw : e.meta.picks)
				if (cw.first < 32)
					picks[cw.first] = cw.second;
		}
		else
			set_base(e.index);
		/* stats from the file */
		auto &b = e.models[0].second;
		int so = (b[0x1c] << 8) | b[0x1d];
		if (so && so + 32 <= (int)b.size())
			stats = fe2::read_fields(std::vector<uint8_t>(b.begin() + so, b.begin() + so + 32));
		parts_model = -1;
		log("loaded " + e.file + " into the editor (as the game draws it; collision is rebuilt on the next build)");
		after_load();
	}

	/* -- UI --------------------------------------------------------------------------------------- */
	void ui();
	void ui_menu();
	void ui_steps(float w);
	void ui_view(ImVec2 size);
	void ui_windows();
	void tab_start();
	void tab_shape();
	void tab_paint();
	void tab_coll();
	void tab_stats();
	void tab_where();
	void tab_build();
	bool next_button(int tab);
	void view_input(ImVec2 origin, ImVec2 size);
	void keys();
};

bool App::next_button(int tab)
{
	ImGui::Spacing();
	if (button(fmt("Next step  >##next%d", tab).c_str()))
	{
		goto_tab = tab;
		return true;
	}
	return false;
}

void App::tab_start()
{
	heading("What are you making?");
	if (ImGui::RadioButton("New ship type", new_ship))
		new_ship = true;
	if (ImGui::RadioButton("Replace an existing model", !new_ship))
		new_ship = false;
	ImGui::Spacing();
	heading("Base ship");
	note("New ship: starts with its stats and engine glow, and the game treats it like this ship. Replace: the "
		 "model that gets the new look.");
	ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginCombo("##base", ship_label(base).c_str()))
	{
		for (auto &s : ship_list)
			if (ImGui::Selectable(ship_label(s.first).c_str(), s.first == base))
			{
				set_base(s.first);
				log("base ship " + ship_label(base) + ": stats and 'where' reset to match it");
			}
		ImGui::EndCombo();
	}
	ImGui::Spacing();
	heading("Start the model from");
	if (button("the base ship (without landing gear)"))
		start_from_game(base, false);
	if (button("the base ship with landing gear"))
		start_from_game(base, true);
	if (button("an OBJ file (from Blender)..."))
		open_obj();
	if (button("the example dart"))
	{
		start_from_example();
		parts_model = -1;
		log("started from the example dart");
	}
	if (button("nothing (build from shapes)"))
	{
		mesh = Mesh();
		parts_model = -1;
		after_load();
		log("blank model - add shapes on the Shape step");
	}
	if (button("any game model... (library)"))
		show_lib = true;
	if (parts_model >= 0)
	{
		ImGui::Spacing();
		heading(fmt("Parts of %s (tick and reload)", ship_label(parts_model).c_str()).c_str());
		for (size_t i = 0; i < parts.size(); i++)
		{
			bool on = parts_on[i];
			if (ImGui::Checkbox(fmt("%s  %d faces##p%d", parts[i].label.c_str(), parts[i].faces, (int)i).c_str(), &on))
				parts_on[i] = on;
		}
		if (button("Reload with these parts"))
			reload_parts();
	}
	next_button(1);
}

void App::tab_shape()
{
	heading("Add a shape");
	ImGui::SetNextItemWidth(-1);
	ImGui::Combo("##prim", &prim, PRIMS, 6);
	ImGui::InputFloat3("size w,h,l (m)", prim_size);
	ImGui::InputFloat3("position x,y,z", prim_pos);
	ImGui::SetNextItemWidth(120);
	ImGui::InputInt("sides", &prim_sides);
	prim_sides = std::max(3, std::min(16, prim_sides));
	ImGui::SameLine();
	ImGui::Checkbox("mirrored pair", &prim_mirror);
	if (button("Add shape"))
	{
		std::vector<Vec3> v;
		std::vector<std::vector<int>> f;
		double w = prim_size[0], h = prim_size[1], l = prim_size[2];
		switch (prim)
		{
		case 0:
			prim_box(Vec3(-w / 2, -h / 2, -l / 2), Vec3(w / 2, h / 2, l / 2), v, f);
			break;
		case 1:
			prim_wedge(Vec3(w, h, l), v, f);
			break;
		case 2:
			prim_cylinder(w / 2, l, prim_sides, w / 2, v, f);
			break;
		case 3:
			prim_cylinder(w / 2, l, prim_sides, w / 8, v, f);
			break;
		case 4:
			prim_pyramid(Vec3(w, h, l), v, f);
			break;
		default:
			prim_sphere(w / 2, 4, prim_sides, v, f);
		}
		Vec3 pos(prim_pos[0], prim_pos[1], prim_pos[2]);
		for (auto &p : v)
			p = p + pos;
		mesh.checkpoint();
		auto idx = mesh.add_part(v, f, cur_mat());
		if (prim_mirror && std::fabs(pos.x) > 1e-6)
		{
			std::vector<Vec3> mv;
			for (auto &p : v)
				mv.push_back(Vec3(-p.x, p.y, p.z));
			auto mf = f;
			for (auto &fa : mf)
				std::reverse(fa.begin(), fa.end());
			mesh.add_part(mv, mf, cur_mat());
		}
		vert_mode = true;
		sel = std::set<int>(idx.begin(), idx.end());
		log(fmt("added a %s (%d faces)", PRIMS[prim], (int)f.size()));
		changed();
	}
	ImGui::Separator();
	heading("Move / scale / turn (selection, or all)");
	auto target = [&]() { return sel.empty() ? mesh.used_verts() : sel_verts(); };
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
	ImGui::InputFloat3("##mv", t_move);
	ImGui::SameLine();
	if (button("Move", -1))
	{
		auto vs = target();
		mesh.checkpoint();
		mesh.move(vs, Vec3(t_move[0], t_move[1], t_move[2]), symmetric && !sel.empty());
		changed();
	}
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
	ImGui::InputFloat3("##sc", t_scale);
	ImGui::SameLine();
	if (button("Scale", -1))
	{
		auto vs = target();
		mesh.checkpoint();
		mesh.scale(vs, Vec3(t_scale[0], t_scale[1], t_scale[2]));
		changed();
	}
	const char *axes[] = {"X", "Y", "Z"};
	ImGui::SetNextItemWidth(60);
	ImGui::Combo("##ax", &t_axis, axes, 3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
	ImGui::InputFloat("##deg", &t_deg);
	ImGui::SameLine();
	if (button("Rotate", -1))
	{
		auto vs = target();
		mesh.checkpoint();
		mesh.rotate(vs, t_axis, t_deg);
		changed();
	}
	ImGui::Separator();
	heading("Faces");
	ImGui::SetNextItemWidth(100);
	ImGui::InputFloat("##ex", &extrude_dist);
	ImGui::SameLine();
	if (button("Extrude", 110) && !vert_mode && !sel.empty())
	{
		mesh.checkpoint();
		mesh.extrude(sel, extrude_dist);
		changed();
	}
	ImGui::SameLine();
	if (button("Flip", 80) && !vert_mode && !sel.empty())
	{
		mesh.checkpoint();
		mesh.flip_faces(sel);
		changed();
	}
	ImGui::SameLine();
	if (button("Delete", -1))
		delete_sel();
	if (button("Make face from picked vertices"))
	{
		if (!vert_mode || pick_order.size() < 3)
			log("make face: switch to vertex mode and shift+click 3 or 4 vertices in order");
		else
		{
			mesh.checkpoint();
			mesh.make_face(pick_order, cur_mat());
			pick_order.clear();
			changed();
		}
	}
	ImGui::Separator();
	heading("Whole model");
	float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (button("Mirror right half to left", half))
	{
		mesh.checkpoint();
		mesh.mirror_x();
		changed();
	}
	ImGui::SameLine();
	if (button("Recalc outside", half))
	{
		mesh.checkpoint();
		log(fmt("recalculate outside: %d faces flipped", mesh.recalc_outside()));
		changed();
	}
	if (button("Centre", half))
	{
		mesh.checkpoint();
		Vec3 lo, hi;
		mesh.bounds(lo, hi);
		mesh.transform_all(1, (lo + hi) * -0.5);
		changed();
		frame_all();
	}
	ImGui::SameLine();
	if (button("Match base ship length", half))
	{
		Vec3 lo, hi;
		mesh.bounds(lo, hi);
		double cur = hi.z - lo.z, want = base_size().z;
		if (cur > 1e-6 && want > 0)
		{
			mesh.checkpoint();
			mesh.transform_all(want / cur, Vec3());
			log(fmt("scaled to the base ship's length (%.1f m)", want));
			changed();
			frame_all();
		}
	}
	ImGui::SetNextItemWidth(100);
	ImGui::InputFloat("##len", &want_len);
	ImGui::SameLine();
	if (button("Scale to this length (m)"))
	{
		Vec3 lo, hi;
		mesh.bounds(lo, hi);
		if (hi.z - lo.z > 1e-6)
		{
			mesh.checkpoint();
			mesh.transform_all(want_len / (hi.z - lo.z), Vec3());
			changed();
			frame_all();
		}
	}
	ImGui::SetNextItemWidth(100);
	ImGui::InputFloat("##weld", &weld_dist);
	ImGui::SameLine();
	if (button("Weld close vertices"))
	{
		mesh.checkpoint();
		mesh.weld(std::max(1e-5f, weld_dist));
		changed();
	}
	note("The game: 64 vertices / 63 normals per model - bigger meshes are split into parts automatically. "
		 "Precision is 1/254 of the ship's size.");
	next_button(2);
}

void App::tab_paint()
{
	heading("Colour (the game has 8 levels per channel)");
	ImGui::SliderInt("red", &cr, 0, 7);
	ImGui::SliderInt("green", &cg, 0, 7);
	ImGui::SliderInt("blue", &cb, 0, 7);
	ImGui::Checkbox("glow (not lit: windows, engines, lights)", &cglow);
	ImGui::Checkbox("add the ship's paint colour", &cpaint);
	ImGui::Checkbox("two-sided (never hidden: fins, decals)", &c2s);
	int colour = make_colour(cr, cg, cb, cglow, cpaint);
	Vec3 rgb = fe2::display_rgb(colour, paint);
	ImGui::ColorButton("##sw", ImVec4((float)rgb.x, (float)rgb.y, (float)rgb.z, 1), 0, ImVec2(60, 40));
	ImGui::SameLine();
	ImGui::Text("game colour #%03x%s", colour, c2s ? "  two-sided" : "");
	float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (button("Paint selected faces", half))
	{
		if (vert_mode || sel.empty())
			log("paint: select faces first (face mode; A selects all)");
		else
		{
			mesh.checkpoint();
			mesh.paint(sel, cur_mat());
			changed();
		}
	}
	ImGui::SameLine();
	if (button("Pick from selection", half) && !vert_mode && !sel.empty())
	{
		int c;
		bool two;
		mat_colour(mesh.faces[*sel.begin()].m, c, two);
		cr = (c >> 9) & 7;
		cg = (c >> 5) & 7;
		cb = (c >> 1) & 7;
		cglow = c & 0x100;
		cpaint = c & 0x10;
		c2s = two;
	}
	ImGui::Separator();
	heading("Colours on the model (click: select those faces)");
	int n = 0;
	for (auto &m : mesh.materials())
	{
		Vec3 c = mat_rgb(m, paint);
		if (n % 7)
			ImGui::SameLine();
		if (ImGui::ColorButton(("##" + m).c_str(), ImVec4((float)c.x, (float)c.y, (float)c.z, 1), 0, ImVec2(44, 30)))
		{
			set_mode(false);
			for (int k = 0; k < (int)mesh.faces.size(); k++)
				if (mesh.faces[k].m == m)
					sel.insert(k);
			int col;
			bool two;
			mat_colour(m, col, two);
			cr = (col >> 9) & 7;
			cg = (col >> 5) & 7;
			cb = (col >> 1) & 7;
			cglow = col & 0x100;
			cpaint = col & 0x10;
			c2s = two;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", m.c_str());
		n++;
	}
	ImGui::Separator();
	ImGui::SetNextItemWidth(80);
	if (ImGui::InputText("paint colour (3 hex)", paint_hex, sizeof(paint_hex), ImGuiInputTextFlags_CharsHexadecimal))
		paint = (int)strtol(paint_hex, nullptr, 16) & 0xFFF;
	note("Paint = the ship's own colour; ships of a type get different ones in the game. This one is for the view "
		 "and the default.");
	next_button(3);
}

void App::tab_coll()
{
	heading("How the game knows you hit something");
	ImGui::SetNextItemWidth(-1);
	ImGui::Combo("##coll", &collision, fe2::COLLISION_MODES, fe2::COLLISION_MODE_COUNT);
	note("auto: your hulls below if there are any, else kdop14. box / kdop14 / kdop26: a 6, 14 or 26 sided hull "
		 "around the whole mesh. custom: the hulls below (each one convex).");
	ImGui::Separator();
	heading("Collision hulls (pink in the view)");
	static int hull_sel = -1;
	if (ImGui::BeginListBox("##hulls", ImVec2(-1, 6 * ImGui::GetTextLineHeightWithSpacing())))
	{
		for (int i = 0; i < (int)mesh.hulls.size(); i++)
			if (ImGui::Selectable(fmt("%d: %s (%d faces)", i + 1, mesh.hulls[i].name.c_str(),
									  (int)mesh.hulls[i].faces.size())
									  .c_str(),
								  hull_sel == i))
				hull_sel = i;
		ImGui::EndListBox();
	}
	auto box_around = [&](const std::set<int> &vs) {
		if (vs.empty())
		{
			log("nothing to put a box around");
			return;
		}
		Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
		for (int i : vs)
			for (int a = 0; a < 3; a++)
			{
				lo[a] = std::min(lo[a], mesh.verts[i][a]);
				hi[a] = std::max(hi[a], mesh.verts[i][a]);
			}
		mesh.checkpoint();
		mesh.add_hull_box(lo, hi);
		collision = 4;
		changed();
	};
	float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (button("Box around ship", half))
		box_around(mesh.used_verts());
	ImGui::SameLine();
	if (button("Box around selection", half))
		box_around(sel_verts());
	float third = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
	auto grow = [&](double k) {
		if (hull_sel < 0 || hull_sel >= (int)mesh.hulls.size())
			return;
		auto &h = mesh.hulls[hull_sel];
		Vec3 c;
		for (auto &v : h.verts)
			c = c + v;
		c = c * (1.0 / h.verts.size());
		mesh.checkpoint();
		for (auto &v : h.verts)
			v = c + (v - c) * k;
		changed();
	};
	if (button("Grow 10%", third))
		grow(1.1);
	ImGui::SameLine();
	if (button("Shrink 10%", third))
		grow(0.9);
	ImGui::SameLine();
	if (button("Delete##hull", third) && hull_sel >= 0 && hull_sel < (int)mesh.hulls.size())
	{
		mesh.checkpoint();
		mesh.hulls.erase(mesh.hulls.begin() + hull_sel);
		hull_sel = -1;
		changed();
	}
	note("Tip: a box for the body and one for the wings fits most ships.");
	next_button(4);
}

void App::tab_stats()
{
	heading("Ship data (starts as the base ship's)");
	for (int i = 0; i < fe2::SHIP_FIELD_COUNT; i++)
	{
		auto &f = fe2::SHIP_FIELDS[i];
		std::string k = f.key;
		if (k == "name_id" || k == "x12")
			continue;
		int v = stats[k];
		ImGui::SetNextItemWidth(160);
		if (ImGui::InputInt(f.label, &v))
			stats[k] = f.is_signed ? std::max(-32768, std::min(32767, v)) : std::max(0, std::min(65535, v));
		if (f.help[0] && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", f.help);
	}
	if (button("Reset to the base ship"))
		stats = base_stats();
	note("Drive: 1 interplanetary, 2-8 = class 1-7 hyperdrive, 10-12 military. Add 32768 to make it built in.");
	next_button(5);
}

void App::tab_where()
{
	ImGui::SetNextItemWidth(280);
	ImGui::InputText("name", name, sizeof(name));
	if (new_ship)
	{
		auto used = cached_owners();
		if (used.count(slot) && pick_slot())
			slot_cache_time = -10;
		ImGui::SetNextItemWidth(160);
		if (ImGui::BeginCombo("model number", fmt("%d", slot).c_str()))
		{
			for (int n = fe2::SHIP_SLOT_FIRST; n <= fe2::SHIP_SLOT_LAST; n++)
			{
				bool taken = used.count(n) > 0;
				std::string label = taken ? fmt("%d  taken: %s", n, used[n].c_str()) : fmt("%d  free", n);
				if (ImGui::Selectable(label.c_str(), n == slot, taken ? ImGuiSelectableFlags_Disabled : 0))
					slot = n;
			}
			ImGui::EndCombo();
		}
		int free_count = 0;
		for (int n = fe2::SHIP_SLOT_FIRST; n <= fe2::SHIP_SLOT_LAST; n++)
			free_count += !used.count(n);
		note(fmt("Picked for you: each new ship type gets its own model number from 240 up (big ships' extra "
				 "parts take numbers from 511 down). Taken numbers can't be chosen, so ships never overwrite "
				 "each other. %s. %d of %d free.",
				 editing.empty() ? "Not saved yet" : ("Saves to " + file_for(slot)).c_str(), free_count,
				 fe2::SHIP_SLOT_COUNT)
				 .c_str());
	}
	else
		note(fmt("A new look keeps the model number of the ship it replaces (%03d).", base).c_str());
	ImGui::Separator();
	heading("Where it appears (new ships only)");
	note("Weight = entries in the list; the game picks one entry at random, so % is the chance per pick. A "
		 "shipyard only rolls a few picks when you arrive in a system (and half of them come up empty), so a "
		 "low % can take many visits. The shipyard lists depend on who runs the system: independent, Imperial "
		 "or Federal.");
	note("To test: in the game press Ctrl-F, CHEATS, \"Sell My Ships In This System\", then open the shipyard.");
	for (int c = 0; c < (int)lists.size(); c++)
	{
		ImGui::SetNextItemWidth(110);
		ImGui::SliderInt(fmt("##pk%d", c).c_str(), &picks[c], 0, 20);
		ImGui::SameLine();
		int total = (int)lists[c].size() + picks[c];
		std::string pct = picks[c] ? fmt("%3d%%", total ? picks[c] * 100 / total : 0) : "    ";
		ImGui::TextColored(rgb444(COL_PANEL_LIGHT), "%s %2d %s", pct.c_str(), c,
						   c < fe2::CATEGORY_COUNT ? fe2::CATEGORIES[c] : "?");
		if (ImGui::IsItemHovered())
		{
			std::set<std::string> names;
			for (int i : lists[c])
				if (auto m = image.model(i))
					names.insert(m->ship_name());
			std::string s;
			for (auto &n : names)
				s += (s.empty() ? "" : ", ") + n;
			ImGui::SetTooltip("now: %s", s.c_str());
		}
	}
	if (button("Same places as the base ship"))
		for (int c = 0; c < (int)lists.size(); c++)
			picks[c] = (int)std::count(lists[c].begin(), lists[c].end(), base); /* as common as the base ship */
	next_button(6);
}

void App::tab_build()
{
	ImGui::Checkbox("keep the base ship's engine glow and lights", &keep_extras);
	ImGui::Checkbox("fix faces pointing inwards", &fix_normals);
	ImGui::Spacing();
	if (button("Test build  (shows what the game will draw)"))
		build(false);
	if (button("SAVE (to your ships folder)"))
		build(true);
	if (button("SAVE THE GAME with my ships in it"))
		save_game();
	if (button("Launch the game (you must recompile to see changes)"))
		launch_game();
	ImGui::Separator();
	float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
	if (button("Save OBJ for Blender...", half))
		save_obj();
	ImGui::SameLine();
	if (button("Reload from Blender", half))
	{
		if (blender_path.empty())
			log("save an OBJ for Blender first");
		else
			load_obj(blender_path);
	}
	if (button("Your ships..."))
	{
		refresh_mods();
		show_mods = true;
	}
	ImGui::Spacing();
	note(("Your ships folder: " + ships_dir).c_str());
	note(("Game: " + game_exe).c_str());
	note("SAVE THE GAME copies every ship in the folder into GLFrontier.exe, so it stays one standalone file you "
		 "can take anywhere. In the game: F4 at a station, Shipyard, New and Reconditioned Ships (start a new game "
		 "or visit another station so the shipyard restocks).");
}

void App::ui_steps(float w)
{
	ImGui::BeginChild("steps", ImVec2(w, 0), ImGuiChildFlags_Borders);
	static const char *names[] = {"1 Start", "2 Shape", "3 Paint", "4 Collision", "5 Stats", "6 Name & where",
								  "7 Build & play"};
	if (ImGui::BeginTabBar("stepsbar", ImGuiTabBarFlags_FittingPolicyScroll))
	{
		for (int t = 0; t < 7; t++)
		{
			ImGuiTabItemFlags fl = goto_tab == t ? ImGuiTabItemFlags_SetSelected : 0;
			if (ImGui::BeginTabItem(names[t], nullptr, fl))
			{
				ImGui::BeginChild("steppage");
				switch (t)
				{
				case 0:
					tab_start();
					break;
				case 1:
					tab_shape();
					break;
				case 2:
					tab_paint();
					break;
				case 3:
					tab_coll();
					break;
				case 4:
					tab_stats();
					break;
				case 5:
					tab_where();
					break;
				default:
					tab_build();
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	if (goto_tab >= 0)
	{
		static int frames = 0;
		if (++frames > 1)
		{
			goto_tab = -1;
			frames = 0;
		}
	}
	ImGui::EndChild();
}

void App::view_input(ImVec2 o, ImVec2 size)
{
	ImGuiIO &io = ImGui::GetIO();
	ImVec2 m(io.MousePos.x - o.x, io.MousePos.y - o.y);
	bool hovered = ImGui::IsItemHovered();
	float w = size.x, h = size.y;
	if (hovered && io.MouseWheel != 0)
		cam.dist = std::max(1.0, std::min(5000.0, cam.dist * (io.MouseWheel > 0 ? 0.88 : 1.14)));
	bool shift = io.KeyShift;
	if (hovered && !show_game && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		std::vector<DrawFace> fs;
		for (int k = 0; k < (int)mesh.faces.size(); k++)
		{
			DrawFace d;
			for (int i : mesh.faces[k].v)
				d.pts.push_back(mesh.verts[i]);
			int c;
			mat_colour(mesh.faces[k].m, c, d.two_sided);
			d.id = k;
			fs.push_back(d);
		}
		int hit = vert_mode ? pick_vertex(cam, m.x, m.y, w, h, mesh.verts, mesh.used_verts())
							: pick_face(cam, m.x, m.y, w, h, fs, vopt.cull);
		if (hit >= 0 && sel.count(hit) && !shift)
			drag = D_MOVE_WAIT;
		else if (hit < 0)
		{
			if (shift)
				drag = D_BOX;
			else
			{
				sel.clear();
				pick_order.clear();
				drag = D_NONE;
			}
		}
		else
		{
			if (!shift)
			{
				sel = {hit};
				pick_order = {hit};
			}
			else if (sel.count(hit))
				sel.erase(hit);
			else
			{
				sel.insert(hit);
				pick_order.push_back(hit);
			}
			drag = D_MOVE_WAIT;
		}
		drag_start = drag_last = m;
	}
	if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
	{
		drag = (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || shift) ? D_PAN : D_ORBIT;
		drag_start = drag_last = m;
	}
	float dx = m.x - drag_last.x, dy = m.y - drag_last.y;
	Vec3 r, u, f;
	cam.axes(r, u, f);
	double k = cam.dist * 2 * std::tan(cam.fov * 3.14159265 / 360) / h;
	if (drag == D_ORBIT)
	{
		cam.yaw += dx * 0.01;
		cam.pitch = std::max(-1.55, std::min(1.55, cam.pitch + dy * 0.01));
	}
	else if (drag == D_PAN)
		cam.target = cam.target - r * (dx * k) + u * (dy * k);
	else if (drag == D_MOVE_WAIT && std::hypot(m.x - drag_start.x, m.y - drag_start.y) > 4)
	{
		mesh.checkpoint();
		drag = D_MOVE;
		changed();
	}
	else if (drag == D_MOVE && (dx || dy))
	{
		auto vs = sel_verts();
		if (!vs.empty())
		{
			Vec3 c = mesh.centre_of(vs);
			double depth = std::max(0.5, fe2::dot(cam.eye() - c, f));
			double kk = depth * 2 * std::tan(cam.fov * 3.14159265 / 360) / h;
			Vec3 d = r * (dx * kk) - u * (dy * kk);
			if (axis_lock >= 0)
			{
				double along = d[axis_lock];
				if (std::fabs(along) < 1e-9)
					along = -dy * kk;
				d = Vec3();
				d[axis_lock] = along;
			}
			mesh.move(vs, d, symmetric);
		}
	}
	drag_last = m;
	if (drag == D_BOX)
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();
		dl->AddRect(ImVec2(o.x + std::min(drag_start.x, m.x), o.y + std::min(drag_start.y, m.y)),
					ImVec2(o.x + std::max(drag_start.x, m.x), o.y + std::max(drag_start.y, m.y)),
					ImGui::GetColorU32(rgb444(COL_YELLOW)));
	}
	if (!io.MouseDown[0] && !io.MouseDown[1] && !io.MouseDown[2] && drag != D_NONE)
	{
		if (drag == D_BOX)
		{
			float x0 = std::min(drag_start.x, m.x), x1 = std::max(drag_start.x, m.x);
			float y0 = std::min(drag_start.y, m.y), y1 = std::max(drag_start.y, m.y);
			auto in = [&](const Vec3 &p) {
				float sx, sy;
				return cam.project(p, w, h, sx, sy) && sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1;
			};
			if (vert_mode)
			{
				for (int i : mesh.used_verts())
					if (in(mesh.verts[i]))
						sel.insert(i);
			}
			else
				for (int kf = 0; kf < (int)mesh.faces.size(); kf++)
				{
					bool all = true;
					for (int i : mesh.faces[kf].v)
						if (!in(mesh.verts[i]))
							all = false;
					if (all)
						sel.insert(kf);
				}
		}
		drag = D_NONE;
	}
}

void App::ui_view(ImVec2 size)
{
	/* toolbar */
	if (ImGui::RadioButton("vertices", vert_mode))
		set_mode(true);
	ImGui::SameLine();
	if (ImGui::RadioButton("faces", !vert_mode))
		set_mode(false);
	ImGui::SameLine();
	ImGui::Checkbox("mirror edits", &symmetric);
	ImGui::SameLine();
	const char *locks[] = {"free", "X", "Y", "Z"};
	int lock = axis_lock + 1;
	ImGui::SetNextItemWidth(70);
	if (ImGui::Combo("move", &lock, locks, 4))
		axis_lock = lock - 1;
	ImGui::SameLine();
	ImGui::Checkbox("hide back faces", &vopt.cull);
	ImGui::SameLine();
	ImGui::Checkbox("wire", &vopt.wire);
	ImGui::SameLine();
	ImGui::Checkbox("collision", &vopt.hulls);
	ImGui::SameLine();
	if (ImGui::Checkbox("game result", &show_game) && show_game && game_faces.empty())
	{
		show_game = false;
		log("build first (step 7: Test build) to see the game result");
	}

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 vsize(avail.x, std::max(100.0f, size.y - ImGui::GetFrameHeightWithSpacing() - 8));
	vopt.paint = paint;
	vopt.verts = vert_mode;
	std::vector<DrawFace> faces;
	if (show_game)
		faces = game_faces;
	else
		for (int k = 0; k < (int)mesh.faces.size(); k++)
		{
			DrawFace d;
			for (int i : mesh.faces[k].v)
				d.pts.push_back(mesh.verts[i]);
			mat_colour(mesh.faces[k].m, d.colour, d.two_sided);
			d.selected = !vert_mode && sel.count(k);
			d.id = k;
			faces.push_back(d);
		}
	std::vector<std::vector<Vec3>> hl;
	for (auto &hh : mesh.hulls)
		for (auto &hf : hh.faces)
		{
			std::vector<Vec3> l;
			for (int i : hf)
				l.push_back(hh.verts[i]);
			l.push_back(hh.verts[hf[0]]);
			hl.push_back(l);
		}
	std::vector<Vec3> pts, spts;
	if (vert_mode && !show_game)
		for (int i : mesh.used_verts())
			(sel.count(i) ? spts : pts).push_back(mesh.verts[i]);
	unsigned int tex = ren.render((int)vsize.x, (int)vsize.y, cam, faces, hl, pts, spts, vopt);
	ImVec2 o = ImGui::GetCursorScreenPos();
	ImGui::Image((ImTextureID)(intptr_t)tex, vsize, ImVec2(0, 1), ImVec2(1, 0));
	view_input(o, vsize);

	/* on-screen info */
	ImDrawList *dl = ImGui::GetWindowDrawList();
	Vec3 s = mesh.faces.empty() ? Vec3() : mesh.size(), bs = base_size();
	std::string l1 = fmt("%d faces  %d vertices   %.1f x %.1f x %.1f m (w x h x l)    base ship %.1f x %.1f x %.1f m",
						 (int)mesh.faces.size(), (int)mesh.used_verts().size(), s.x, s.y, s.z, bs.x, bs.y, bs.z);
	dl->AddText(ImVec2(o.x + 10, o.y + 8), ImGui::GetColorU32(rgb444(COL_TEXT)), l1.c_str());
	std::string l2 = show_game ? "GAME RESULT (as built)"
							   : fmt("%s mode  sel %d%s", vert_mode ? "vertex" : "face", (int)sel.size(),
									 axis_lock >= 0 ? fmt("   move locked to %c", "XYZ"[axis_lock]).c_str() : "");
	dl->AddText(ImVec2(o.x + 10, o.y + 8 + 11 * PX), ImGui::GetColorU32(rgb444(COL_ORANGE)), l2.c_str());
	float nx, ny;
	if (cam.project(Vec3(0, 0, -30), vsize.x, vsize.y, nx, ny))
		dl->AddText(ImVec2(o.x + nx, o.y + ny), ImGui::GetColorU32(rgb444(0x48f)), "nose");
	dl->AddText(ImVec2(o.x + 10, o.y + vsize.y - 12 * PX), ImGui::GetColorU32(rgb444(COL_PANEL_LIGHT)),
				"right drag: turn   middle drag: pan   wheel: zoom   click: select   drag selection: move   shift+drag: box");
}

void App::ui_menu()
{
	if (!ImGui::BeginMenuBar())
		return;
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Open OBJ (from Blender)..."))
			open_obj();
		if (ImGui::MenuItem("Save OBJ (for Blender)..."))
			save_obj();
		if (ImGui::MenuItem("Reload from Blender") && !blender_path.empty())
			load_obj(blender_path);
		ImGui::Separator();
		if (ImGui::MenuItem("Game model library..."))
			show_lib = true;
		if (ImGui::MenuItem("Your ships..."))
		{
			refresh_mods();
			show_mods = true;
		}
		if (ImGui::MenuItem("Open your ships folder"))
		{
			make_dirs(ships_dir);
			open_folder(ships_dir);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Quit"))
		{
			SDL_Event e;
			e.type = SDL_QUIT;
			SDL_PushEvent(&e);
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Edit"))
	{
		if (ImGui::MenuItem("Undo", "Ctrl+Z") && mesh.undo())
			changed();
		if (ImGui::MenuItem("Redo", "Ctrl+Y") && mesh.redo())
			changed();
		if (ImGui::MenuItem("Select all", "A"))
			select_all();
		if (ImGui::MenuItem("Delete selected", "Del"))
			delete_sel();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Frame all", "F"))
			frame_all();
		if (ImGui::MenuItem("Front"))
			cam.yaw = 0, cam.pitch = 0;
		if (ImGui::MenuItem("Side"))
			cam.yaw = 1.5708, cam.pitch = 0;
		if (ImGui::MenuItem("Top"))
			cam.yaw = 0, cam.pitch = 1.55;
		if (ImGui::MenuItem("Three quarter"))
			cam.yaw = 0.7, cam.pitch = 0.45;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools"))
	{
		if (ImGui::MenuItem("Save the game with my ships in it"))
			save_game();
		if (ImGui::MenuItem("Launch the game (you must recompile to see changes)"))
			launch_game();
		if (ImGui::MenuItem("Choose GLFrontier.exe..."))
		{
			std::string p = open_dialog("Where is GLFrontier.exe?", "GLFrontier|GLFrontier.exe|Programs|*.exe");
			if (!p.empty())
				game_exe = p;
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Help"))
		show_help = true;
	ImGui::EndMenuBar();
}

void App::ui_windows()
{
	ImGuiIO &io = ImGui::GetIO();
	if (show_lib)
	{
		ImGui::SetNextWindowSize(ImVec2(520, 640), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.35f, 80), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Game model library", &show_lib))
		{
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##lf", "filter", lib_filter, sizeof(lib_filter));
			if (ImGui::BeginListBox("##lib", ImVec2(-1, -3 * ImGui::GetFrameHeightWithSpacing())))
			{
				std::string q = lib_filter;
				for (auto &c : q)
					c = (char)tolower((unsigned char)c);
				for (int i : image.model_numbers())
				{
					std::string l = ship_label(i), ll = l;
					for (auto &c : ll)
						c = (char)tolower((unsigned char)c);
					if (!q.empty() && ll.find(q) == std::string::npos)
						continue;
					if (ImGui::Selectable(l.c_str(), lib_sel == i))
						lib_sel = i;
				}
				ImGui::EndListBox();
			}
			float third = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
			if (button("Open", third) && lib_sel >= 0)
				start_from_game(lib_sel, false);
			ImGui::SameLine();
			if (button("Open with gear", third) && lib_sel >= 0)
				start_from_game(lib_sel, true);
			ImGui::SameLine();
			if (button("Use as base ship", third) && lib_sel >= 0)
			{
				auto m = image.model(lib_sel);
				if (m && m->ship_off && !m->ship_name().empty())
					set_base(lib_sel);
				else
					log(ship_label(lib_sel) + " isn't a ship");
			}
			if (button("Show its data and code") && lib_sel >= 0)
			{
				auto m = image.model(lib_sel);
				if (m)
				{
					code_text = fmt("model %d %s  scale %d  radius %d  colour #%03x\n", m->index, ship_label(m->index).c_str(),
									m->scale, m->radius, m->colour);
					code_text += fmt("%d vertices, %d normals\n", m->num_verts, m->num_norms);
					if (m->ship_off)
					{
						auto st = fe2::read_fields(m->ship_data(32));
						for (int i = 0; i < fe2::SHIP_FIELD_COUNT; i++)
							code_text += fmt("  %-28s %d\n", fe2::SHIP_FIELDS[i].label, st[fe2::SHIP_FIELDS[i].key]);
					}
					code_text += "\n" + fe2::disasm(fe2::decode(image.data, m->code_addr()));
					show_code = true;
				}
			}
		}
		ImGui::End();
	}
	if (show_code)
	{
		ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Model data and bytecode", &show_code))
			ImGui::InputTextMultiline("##code", &code_text[0], code_text.size() + 1, ImVec2(-1, -1),
									  ImGuiInputTextFlags_ReadOnly);
		ImGui::End();
	}
	if (show_mods)
	{
		ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.35f, 120), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Your ships", &show_mods))
		{
			if (ImGui::BeginListBox("##mods", ImVec2(-1, -2 * ImGui::GetFrameHeightWithSpacing())))
			{
				for (int i = 0; i < (int)mods.size(); i++)
					if (ImGui::Selectable(mods[i].label.c_str(), mods_sel == i))
						mods_sel = i;
				if (mods.empty())
					ImGui::TextDisabled("nothing saved yet");
				ImGui::EndListBox();
			}
			float q = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4;
			bool ok = mods_sel >= 0 && mods_sel < (int)mods.size();
			if (button("Load into editor", q) && ok && mods[mods_sel].index >= 0)
				mod_load(mods[mods_sel]);
			ImGui::SameLine();
			if (button("Turn on/off", q) && ok)
			{
				auto &e = mods[mods_sel];
				std::string to = e.enabled ? e.path + ".off" : e.path.substr(0, e.path.size() - 4);
				if (file_exists(to))
					log("can't: " + base_name(to) + " is already there. Delete one of the two first.");
				else
				{
					std::rename(e.path.c_str(), to.c_str());
					if (editing == e.file)
						editing = base_name(to);
				}
				refresh_mods();
			}
			ImGui::SameLine();
			if (button("Delete", q) && ok)
			{
				if (editing == mods[mods_sel].file)
					editing.clear(); /* the next save makes a new file */
				std::remove(mods[mods_sel].path.c_str());
				log("deleted " + mods[mods_sel].path);
				mods_sel = -1;
				refresh_mods();
			}
			ImGui::SameLine();
			if (button("Refresh", q))
				refresh_mods();
			note("Turned off ships stay in the folder but aren't put into the game. Press SAVE THE GAME after "
				 "changes.");
		}
		ImGui::End();
	}
	if (show_help)
	{
		ImGui::SetNextWindowSize(ImVec2(760, 420), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Help", &show_help))
			ImGui::TextWrapped(
				"FE2 Ship Builder makes ships for Frontier: Elite 2 (GLFrontier) and puts them in the game.\n\n"
				"Work down the steps on the left: pick a base ship and start from it (or an OBJ from Blender, or "
				"shapes), shape it, paint it, give it collision hulls, set its stats, name it and choose where it "
				"appears, then Test build, SAVE and SAVE THE GAME.\n\n"
				"View: right drag turns, middle drag (or shift + right drag) pans, the wheel zooms. Left click "
				"selects (shift adds), dragging the selection moves it, shift + drag on empty space box-selects.\n"
				"Keys: 1 / 2 vertex or face mode, A select all, F frame, X / Y / Z lock moves to an axis, Delete, "
				"Ctrl+Z / Ctrl+Y undo and redo.\n\n"
				"The game's limits: 64 vertices and 63 normals per model and 1/254 of its size in precision; "
				"bigger meshes are split into parts automatically. The game has no depth buffer and hides faces "
				"that point away, so keep ships closed and outward facing (Recalc outside).");
		ImGui::End();
	}
}

void App::keys()
{
	ImGuiIO &io = ImGui::GetIO();
	if (io.WantTextInput)
		return;
	bool ctrl = io.KeyCtrl;
	if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z))
	{
		if (mesh.undo())
		{
			sel.clear();
			changed();
		}
	}
	else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y))
	{
		if (mesh.redo())
		{
			sel.clear();
			changed();
		}
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Delete))
		delete_sel();
	else if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_A))
		select_all();
	else if (ImGui::IsKeyPressed(ImGuiKey_F))
		frame_all();
	else if (ImGui::IsKeyPressed(ImGuiKey_1))
		set_mode(true);
	else if (ImGui::IsKeyPressed(ImGuiKey_2))
		set_mode(false);
	else if (!ctrl)
		for (int a = 0; a < 3; a++)
			if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_X + a)))
				axis_lock = axis_lock == a ? -1 : a;
}

void App::ui()
{
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::Begin("FE2 Ship Builder", nullptr,
				 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus |
					 ImGuiWindowFlags_NoScrollbar);
	ui_menu();
	float left = std::min(560.0f, io.DisplaySize.x * 0.4f);
	float log_h = 7 * ImGui::GetTextLineHeightWithSpacing() + 12;
	ImGui::BeginGroup();
	ui_steps(left);
	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("viewarea", ImVec2(avail.x, avail.y - log_h), ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ui_view(ImGui::GetContentRegionAvail());
	ImGui::EndChild();
	ImGui::BeginChild("log", ImVec2(avail.x, log_h - 4), ImGuiChildFlags_Borders);
	for (auto &l : log_lines)
		ImGui::TextUnformatted(l.c_str());
	if (log_scroll)
	{
		ImGui::SetScrollHereY(1.0f);
		log_scroll = false;
	}
	ImGui::EndChild();
	ImGui::EndGroup();
	ImGui::End();
	ui_windows();
	keys();
}

} // namespace

/* FE2ShipBuilder --build model.obj BASE INDEX out.fe2m [name]: build without the
 * window (for scripts and for checking the builder) */
static int cmd_build(int argc, char **argv)
{
	if (argc < 6)
		return 2;
	Mesh m;
	std::string err;
	if (!Mesh::from_obj(argv[2], m, err))
	{
		fprintf(stderr, "%s\n", err.c_str());
		return 1;
	}
	const fe2::Image &img = fe2::Image::game();
	int base = atoi(argv[3]), index = atoi(argv[4]);
	auto tmpl = img.model(base);
	if (!tmpl)
		return 1;
	fe2::BuildOptions opt;
	opt.index = index;
	try
	{
		auto res = fe2::build_model(m.to_build(), &*tmpl, img, opt);
		fe2::Fe2mMeta meta;
		meta.name = argc > 6 ? argv[6] : "";
		meta.like = index != base ? base : -1;
		fe2::write_fe2m(argv[5], res.models, index != base ? &meta : nullptr);
		printf("%d faces %d verts %d norms scale %d parts %d bytes %d collision %s\n", res.faces, res.verts, res.norms,
			   res.scale, res.parts, res.bytes, res.collision.c_str());
		for (auto &w : res.warnings)
			printf("warning: %s\n", w.c_str());
	}
	catch (std::exception &e)
	{
		fprintf(stderr, "build failed: %s\n", e.what());
		return 1;
	}
	return 0;
}

/* FE2ShipBuilder --save-game GLFrontier.exe ship.fe2m...: put ships into the game
 * exe without the window (no ship files = take them all out) */
static int cmd_save_game(int argc, char **argv)
{
	if (argc < 3)
		return 2;
	std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
	for (int i = 3; i < argc; i++)
	{
		std::vector<uint8_t> d;
		if (!read_file(argv[i], d))
		{
			fprintf(stderr, "can't read %s\n", argv[i]);
			return 1;
		}
		files.push_back({base_name(argv[i]), d});
	}
	std::string err;
	if (!write_game_with_ships(argv[2], argv[2], files, err))
	{
		fprintf(stderr, "%s\n", err.c_str());
		return 1;
	}
	printf("%s: %d ship file(s) in the exe\n", argv[2], (int)files.size());
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && std::string(argv[1]) == "--build")
		return cmd_build(argc, argv);
	if (argc > 1 && std::string(argv[1]) == "--save-game")
		return cmd_save_game(argc, argv);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
		return 1;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_Window *win = SDL_CreateWindow("FE2 Ship Builder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 950,
									   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (!win)
		return 1;
	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	SDL_GL_MakeCurrent(win, ctx);
	SDL_GL_SetSwapInterval(1);
	if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
		return 1;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui_ImplSDL2_InitForOpenGL(win, ctx);
	ImGui_ImplOpenGL3_Init("#version 330 core");
	build_font(PX);
	build_style(PX);

	App app;
	app.ren.init();
	app.init();

	bool quit = false;
	while (!quit)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			if (e.type == SDL_QUIT)
				quit = true;
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
				quit = true;
		}
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		app.ui();
		ImGui::Render();
		int w, h;
		SDL_GL_GetDrawableSize(win, &w, &h);
		glViewport(0, 0, w, h);
		glClearColor(0, 0, 34 / 255.0f, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(win);
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
