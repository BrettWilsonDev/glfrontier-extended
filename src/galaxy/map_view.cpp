/*
 * map_view.cpp - see map_view.h.
 */
#include "map_view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <unordered_map>

using namespace galaxy;

/* Stars are drawn when at most this many sectors are in view; further out
   only the density map shows */
static const int MAX_VIEW_SECTORS = 4096;
/* time per frame spent generating sectors (the game code is fast, but a
   zoomed out view can need thousands) */
static const double LOAD_BUDGET_MS = 12.0;
static const double MIN_SCALE = 0.04, MAX_SCALE = 6000;
static const double PI = 3.14159265358979323846;

static ImU32 col(uint32_t rgb, int alpha = 255)
{
	return IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, alpha);
}

uint32_t allegiance_colour(int allegiance)
{
	switch (allegiance)
	{
	case 0: return 0xd0d0d0; /* Independent */
	case 1: return 0x60a0ff; /* Imperial (the game's strings, $84b7 +) */
	case 2: return 0xff7050; /* Federation */
	default: return 0x808080;
	}
}

/* -- two finger pinch -------------------------------------------------------- */
namespace
{
struct Finger
{
	long long id;
	ImVec2 pos;
};
Finger fingers[2];
int n_fingers = 0;
float pinch_zoom = 1;
ImVec2 pinch_pan(0, 0), pinch_centre(0, 0);

ImVec2 midpoint()
{
	return ImVec2((fingers[0].pos.x + fingers[1].pos.x) / 2, (fingers[0].pos.y + fingers[1].pos.y) / 2);
}
float spread()
{
	float dx = fingers[0].pos.x - fingers[1].pos.x, dy = fingers[0].pos.y - fingers[1].pos.y;
	return sqrtf(dx * dx + dy * dy);
}
} // namespace

void touch_finger(long long id, TouchFingerEvent what, float x, float y)
{
	int i = 0;
	while (i < n_fingers && fingers[i].id != id)
		i++;
	switch (what)
	{
	case TOUCH_DOWN:
		if (i == n_fingers && n_fingers < 2)
			fingers[n_fingers++] = {id, ImVec2(x, y)};
		if (n_fingers == 2)
		{
			pinch_centre = midpoint();
			pinch_zoom = 1;
			pinch_pan = ImVec2(0, 0);
		}
		break;
	case TOUCH_MOVE:
	{
		if (i == n_fingers)
			break;
		if (n_fingers < 2)
		{
			fingers[i].pos = ImVec2(x, y);
			break;
		}
		ImVec2 m0 = midpoint();
		float d0 = spread();
		fingers[i].pos = ImVec2(x, y);
		ImVec2 m1 = midpoint();
		float d1 = spread();
		if (d0 > 1 && d1 > 1)
			pinch_zoom *= d1 / d0;
		pinch_pan.x += m1.x - m0.x;
		pinch_pan.y += m1.y - m0.y;
		pinch_centre = m1;
		break;
	}
	case TOUCH_UP:
		if (i == n_fingers)
			break;
		fingers[i] = fingers[--n_fingers];
		break;
	}
}

bool touch_scroll_block;

bool touch_pinching()
{
	return n_fingers == 2;
}

bool touch_pinch_take(ImVec2 rmin, ImVec2 rmax, ImVec2 &centre, float &zoom, ImVec2 &pan)
{
	if (n_fingers < 2 || pinch_centre.x < rmin.x || pinch_centre.y < rmin.y || pinch_centre.x >= rmax.x ||
		pinch_centre.y >= rmax.y)
		return false;
	centre = pinch_centre;
	zoom = pinch_zoom;
	pan = pinch_pan;
	pinch_zoom = 1;
	pinch_pan = ImVec2(0, 0);
	return true;
}

void touch_pinch_end_frame()
{
	pinch_zoom = 1;
	pinch_pan = ImVec2(0, 0);
}

ImVec2 MapView::to_screen(double x, double y, double z, ImVec2 o, ImVec2 size, const MapOptions &opt) const
{
	double t = opt.tilt * PI / 180.0;
	double sx = (x - cx) * scale;
	double sy = -((y - cy) * cos(t) + z * sin(t)) * scale;
	return ImVec2((float)(o.x + size.x * 0.5 + sx), (float)(o.y + size.y * 0.5 + sy));
}

void MapView::from_screen(ImVec2 p, ImVec2 o, ImVec2 size, double &x, double &y) const
{
	/* on the galactic plane, z = 0 */
	x = cx + (p.x - o.x - size.x * 0.5) / scale;
	y = cy - (p.y - o.y - size.y * 0.5) / scale / std::max(0.2, cos(tilt_));
}

void MapView::go_to(double x, double y, double new_scale)
{
	target_cx = x;
	target_cy = y;
	target_scale = new_scale > 0 ? new_scale : scale;
	animating = true;
}

void MapView::select(uint32_t id)
{
	selected = id;
	has_selected = true;
	selection_changed = true;
}

void MapView::update(float dt)
{
	if (!animating)
		return;
	/* ease in log space for zoom, so big zooms don't crawl */
	double k = 1.0 - exp(-dt * 8.0);
	double ls = log(scale), lt = log(target_scale);
	ls += (lt - ls) * k;
	scale = exp(ls);
	cx += (target_cx - cx) * k;
	cy += (target_cy - cy) * k;
	if (fabs(lt - ls) < 0.002 && fabs(target_cx - cx) * scale < 0.5 && fabs(target_cy - cy) * scale < 0.5)
	{
		cx = target_cx;
		cy = target_cy;
		scale = target_scale;
		animating = false;
	}
}

static double star_x(const Star &s);
static double star_y(const Star &s);
static double star_z(const Star &s);

bool MapView::star_screen(Galaxy &g, uint32_t id, ImVec2 &out) const
{
	const Star *s = g.star(id);
	if (!s)
		return false;
	out = to_screen(star_x(*s), star_y(*s), star_z(*s), last_pos_, last_size_, last_opt_);
	return true;
}

static double star_x(const Star &s)
{
	return s.x / (double)UNITS_PER_SECTOR;
}
static double star_y(const Star &s)
{
	return s.y / (double)UNITS_PER_SECTOR;
}
static double star_z(const Star &s)
{
	return s.z / (double)UNITS_PER_SECTOR;
}

void MapView::draw(Galaxy &g, const MapOptions &opt, ImVec2 o, ImVec2 size)
{
	ImDrawList *dl = ImGui::GetWindowDrawList();
	ImGuiIO &io = ImGui::GetIO();
	tilt_ = opt.tilt * PI / 180.0;
	last_pos_ = o, last_size_ = size, last_opt_ = opt;
	dl->PushClipRect(o, ImVec2(o.x + size.x, o.y + size.y), true);
	dl->AddRectFilled(o, ImVec2(o.x + size.x, o.y + size.y), IM_COL32(0, 0, 10, 255));

	/* -- input ------------------------------------------------------------ */
	ImGui::SetCursorScreenPos(o);
	ImGui::InvisibleButton("##map", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	bool hover = ImGui::IsItemHovered();
	mouse_inside = hover;
	from_screen(io.MousePos, o, size, mouse_x, mouse_y);
	if (hover && io.MouseWheel != 0)
	{
		double f = pow(1.25, io.MouseWheel);
		double ns = std::clamp(scale * f, MIN_SCALE, MAX_SCALE);
		/* keep the point under the mouse still */
		double mx = mouse_x, my = mouse_y;
		scale = ns;
		double nx, ny;
		from_screen(io.MousePos, o, size, nx, ny);
		cx += mx - nx;
		cy += my - ny;
		animating = false;
	}
	/* two fingers: zoom round their midpoint and move with it */
	ImVec2 pc, pp;
	float pz;
	if (touch_pinch_take(o, ImVec2(o.x + size.x, o.y + size.y), pc, pz, pp))
	{
		double ct = std::max(0.2, cos(tilt_));
		cx -= pp.x / scale;
		cy += pp.y / scale / ct;
		double bx, by, ax, ay;
		from_screen(pc, o, size, bx, by);
		scale = std::clamp(scale * pz, MIN_SCALE, MAX_SCALE);
		from_screen(pc, o, size, ax, ay);
		cx += bx - ax;
		cy += by - ay;
		animating = false;
	}
	if (touch_pinching())
	{
		/* the finger that is also the mouse carries on from here after the
		   pinch, and lifting it is not a click */
		dragging = true;
		drag_start = io.MousePos;
		drag_cx = cx;
		drag_cy = cy;
	}
	else if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		dragging = false;
		drag_start = io.MousePos;
		drag_cx = cx;
		drag_cy = cy;
	}
	if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		ImVec2 d(io.MousePos.x - drag_start.x, io.MousePos.y - drag_start.y);
		if (dragging || d.x * d.x + d.y * d.y > 16)
		{
			dragging = true;
			animating = false;
			cx = drag_cx - d.x / scale;
			cy = drag_cy + d.y / scale / std::max(0.2, cos(opt.tilt * PI / 180.0));
		}
	}
	if (ImGui::IsItemActive())
		touch_scroll_block = true;
	bool clicked = ImGui::IsItemDeactivated() && !dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
	bool dbl = hover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (hover && !io.WantTextInput)
	{
		double pan = 300.0 / scale * io.DeltaTime;
		if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
			cx -= pan, animating = false;
		if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
			cx += pan, animating = false;
		if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
			cy += pan, animating = false;
		if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
			cy -= pan, animating = false;
		if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
			go_to(cx, cy, std::min(scale * 2, MAX_SCALE));
		if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
			go_to(cx, cy, std::max(scale / 2, MIN_SCALE));
	}
	cx = std::clamp(cx, 0.0, (double)SECTORS);
	cy = std::clamp(cy, 0.0, (double)SECTORS);

	double tilt = opt.tilt * PI / 180.0;
	double ct = std::max(0.2, cos(tilt));

	/* -- density background -------------------------------------------------- */
	if (opt.density && density_tex)
	{
		ImVec2 a = to_screen(0, SECTORS, 0, o, size, opt), b = to_screen(SECTORS, SECTORS, 0, o, size, opt);
		ImVec2 c = to_screen(SECTORS, 0, 0, o, size, opt), d = to_screen(0, 0, 0, o, size, opt);
		/* texture row 0 is sector y 0 */
		dl->AddImageQuad((ImTextureID)(intptr_t)density_tex, a, b, c, d, ImVec2(0, 1), ImVec2(1, 1), ImVec2(1, 0),
						 ImVec2(0, 0), IM_COL32(255, 255, 255, scale > 60 ? 90 : 255));
	}

	/* visible sectors (on the plane; stars sit within a sector above/below) */
	double half_w = size.x * 0.5 / scale + 1, half_h = size.y * 0.5 / scale / ct + 1;
	int x0 = std::max(0, (int)floor(cx - half_w)), x1 = std::min(SECTORS - 1, (int)floor(cx + half_w));
	int y0 = std::max(0, (int)floor(cy - half_h)), y1 = std::min(SECTORS - 1, (int)floor(cy + half_h));
	long view_sectors = (long)(x1 - x0 + 1) * (y1 - y0 + 1);
	vx0 = x0, vy0 = y0, vx1 = x1, vy1 = y1;

	/* -- where people live: sectors with starports, from the survey ---------------- */
	if (opt.hue)
	{
		/* sectors, or blocks of them when a sector is under 4 pixels */
		int bin = scale >= 4 ? 1 : (int)ceil(4 / scale);
		std::unordered_map<uint64_t, std::pair<int, int>> bins; /* inhabited systems, with orbital stations */
		for (auto &kv : g.sector_summaries())
		{
			if (!kv.second.inhabited)
				continue;
			int sx = (int)(kv.first & 0xffff), sy = (int)(kv.first >> 16);
			if (sx < x0 - bin || sx > x1 + bin || sy < y0 - bin || sy > y1 + bin)
				continue;
			uint64_t k = (uint64_t)(sy / bin) << 32 | (uint32_t)(sx / bin);
			bins[k].first += kv.second.inhabited;
			bins[k].second += kv.second.orbital;
		}
		for (auto &kv : bins)
		{
			int bx = (int)(kv.first & 0xffffffff) * bin, by = (int)(kv.first >> 32) * bin;
			/* per sector: 1 inhabited system is faint, 6 or more is full */
			float f = std::min(1.0f, kv.second.first / (6.0f * bin * bin) + 0.25f);
			bool stations = kv.second.second > 0;
			ImU32 c = stations ? IM_COL32(40, 220, 255, (int)(40 + 120 * f)) : IM_COL32(60, 255, 110, (int)(40 + 120 * f));
			ImVec2 p0 = to_screen(bx, by + bin, 0, o, size, opt), p1 = to_screen(bx + bin, by, 0, o, size, opt);
			dl->AddRectFilled(p0, p1, c);
		}
	}

	/* -- per-sector star counts ----------------------------------------------- */
	if (opt.counts && view_sectors <= 40000 && scale >= 3)
	{
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
			{
				int n = g.star_count(x, y);
				if (!n)
					continue;
				float f = std::min(1.0f, n / 40.0f);
				ImVec2 p0 = to_screen(x, y + 1, 0, o, size, opt), p1 = to_screen(x + 1, y, 0, o, size, opt);
				dl->AddRectFilled(p0, p1, IM_COL32((int)(40 + 160 * f), (int)(30 + 60 * f), (int)(80 * (1 - f)), 110));
			}
	}

	/* -- sector grid ------------------------------------------------------------ */
	if (opt.grid && scale >= 6)
	{
		int step = 1;
		ImU32 gc = IM_COL32(40, 60, 110, scale >= 20 ? 160 : 90);
		for (int x = x0; x <= x1 + 1; x += step)
			dl->AddLine(to_screen(x, y0, 0, o, size, opt), to_screen(x, y1 + 1, 0, o, size, opt),
						x == SOL_X ? IM_COL32(90, 120, 200, 200) : gc);
		for (int y = y0; y <= y1 + 1; y += step)
			dl->AddLine(to_screen(x0, y, 0, o, size, opt), to_screen(x1 + 1, y, 0, o, size, opt),
						y == SOL_Y ? IM_COL32(90, 120, 200, 200) : gc);
		if (scale >= 36)
		{
			char buf[32];
			for (int x = x0; x <= x1; x++)
				for (int y = y0; y <= y1; y++)
				{
					snprintf(buf, sizeof(buf), "%d,%d", x - SOL_X, y - SOL_Y);
					ImVec2 p = to_screen(x, y + 1, 0, o, size, opt);
					dl->AddText(ImVec2(p.x + 3, p.y + 2), IM_COL32(70, 95, 160, 255), buf);
				}
		}
	}
	else if (opt.grid && scale < 6)
	{
		/* coarse grid every 64 sectors, with Sol's axes */
		ImU32 gc = IM_COL32(30, 45, 90, 110);
		for (int x = 0; x <= SECTORS; x += 64)
			dl->AddLine(to_screen(x, 0, 0, o, size, opt), to_screen(x, SECTORS, 0, o, size, opt), gc);
		for (int y = 0; y <= SECTORS; y += 64)
			dl->AddLine(to_screen(0, y, 0, o, size, opt), to_screen(SECTORS, y, 0, o, size, opt), gc);
		ImU32 sc = IM_COL32(90, 120, 200, 140);
		dl->AddLine(to_screen(SOL_X + 0.5, 0, 0, o, size, opt), to_screen(SOL_X + 0.5, SECTORS, 0, o, size, opt), sc);
		dl->AddLine(to_screen(0, SOL_Y + 0.5, 0, o, size, opt), to_screen(SECTORS, SOL_Y + 0.5, 0, o, size, opt), sc);
	}

	/* -- stars ------------------------------------------------------------------- */
	loading = 0;
	has_hover = false;
	double best_d2 = 12.0 * 12.0;
	const Star *hover_star = nullptr;
	if (view_sectors <= MAX_VIEW_SECTORS)
	{
		/* generate missing sectors nearest the centre first */
		auto t0 = std::chrono::steady_clock::now();
		std::vector<std::pair<double, int>> order;
		order.reserve(view_sectors);
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				if (!g.cached(x, y))
				{
					double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
					order.push_back({dx * dx + dy * dy, y * SECTORS + x});
				}
		std::sort(order.begin(), order.end());
		for (auto &e : order)
		{
			double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			if (ms > LOAD_BUDGET_MS)
			{
				loading++;
				continue;
			}
			g.sector(e.second % SECTORS, e.second / SECTORS);
		}

		float base = (float)std::clamp(scale / 60.0, 1.2, 4.5) * opt.star_size;
		bool names = opt.names && scale >= 70;
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
			{
				if (!g.cached(x, y))
					continue;
				const Sector &s = g.sector(x, y);
				for (auto &st : s.stars)
				{
					double sx = star_x(st), sy = star_y(st), sz = star_z(st);
					ImVec2 p = to_screen(sx, sy, sz, o, size, opt);
					if (p.x < o.x - 50 || p.y < o.y - 20 || p.x > o.x + size.x + 10 || p.y > o.y + size.y + 20)
						continue;
					uint32_t rgb = st.colour;
					if (opt.colour == MapOptions::BY_ALLEGIANCE)
						rgb = allegiance_colour(st.allegiance);
					else if (opt.colour == MapOptions::BY_EXPLORED)
						rgb = st.explored ? 0x70e070 : 0x806060;
					if (opt.tilt > 0.5f && opt.stalks && scale >= 12)
					{
						ImVec2 q = to_screen(sx, sy, 0, o, size, opt);
						dl->AddLine(p, q, col(rgb, 70));
						dl->AddLine(ImVec2(q.x - 2, q.y), ImVec2(q.x + 3, q.y), col(rgb, 70));
					}
					float r = base * (st.multiple ? 1.25f : 1.0f);
					if (r < 2.2f)
						dl->AddRectFilled(ImVec2(p.x - r * 0.5f, p.y - r * 0.5f), ImVec2(p.x + r * 0.5f, p.y + r * 0.5f),
										  col(rgb));
					else
						dl->AddCircleFilled(p, r, col(rgb), 10);
					if (!st.explored && scale >= 40 && opt.colour != MapOptions::BY_EXPLORED)
						dl->AddCircle(p, r + 2, IM_COL32(120, 80, 80, 160), 10);
					if (opt.hue)
						if (const Summary *sum = g.known_summary(st.id))
							if (sum->ports())
							{
								/* a halo: cyan with orbital stations, green with surface ports only */
								float h = r + 3 + std::min(6, sum->ports()) * 0.8f;
								ImU32 c = sum->orbital_ports ? IM_COL32(40, 220, 255, 220) : IM_COL32(60, 255, 110, 220);
								dl->AddCircleFilled(p, h + 2, sum->orbital_ports ? IM_COL32(40, 220, 255, 50)
																				 : IM_COL32(60, 255, 110, 50),
													16);
								dl->AddCircle(p, h, c, 16, 1.5f);
							}
					if (names && scale >= 22 * sqrt((double)s.stars.size())) /* room for them */
						dl->AddText(ImVec2(p.x + r + 3, p.y - 5), IM_COL32(170, 170, 190, 255), st.name.c_str());
					if (hover)
					{
						double dx = p.x - io.MousePos.x, dy = p.y - io.MousePos.y;
						if (dx * dx + dy * dy < best_d2)
						{
							best_d2 = dx * dx + dy * dy;
							hover_star = &st;
						}
					}
				}
			}
	}
	else
	{
		const char *msg = "Zoom in to see stars";
		ImVec2 ts = ImGui::CalcTextSize(msg);
		dl->AddText(ImVec2(o.x + (size.x - ts.x) * 0.5f, o.y + size.y - ts.y - 8), IM_COL32(150, 150, 170, 200), msg);
	}
	if (hover_star)
	{
		has_hover = true;
		hovered = hover_star->id;
	}

	/* -- markers ------------------------------------------------------------------- */
	auto mark = [&](uint32_t id, ImU32 c, float r) {
		const Star *s = g.star(id);
		if (!s)
			return;
		ImVec2 p = to_screen(star_x(*s), star_y(*s), star_z(*s), o, size, opt);
		float a = r, b = r * 0.45f;
		dl->AddLine(ImVec2(p.x - a, p.y - a), ImVec2(p.x - a + b, p.y - a), c, 2);
		dl->AddLine(ImVec2(p.x - a, p.y - a), ImVec2(p.x - a, p.y - a + b), c, 2);
		dl->AddLine(ImVec2(p.x + a, p.y - a), ImVec2(p.x + a - b, p.y - a), c, 2);
		dl->AddLine(ImVec2(p.x + a, p.y - a), ImVec2(p.x + a, p.y - a + b), c, 2);
		dl->AddLine(ImVec2(p.x - a, p.y + a), ImVec2(p.x - a + b, p.y + a), c, 2);
		dl->AddLine(ImVec2(p.x - a, p.y + a), ImVec2(p.x - a, p.y + a - b), c, 2);
		dl->AddLine(ImVec2(p.x + a, p.y + a), ImVec2(p.x + a - b, p.y + a), c, 2);
		dl->AddLine(ImVec2(p.x + a, p.y + a), ImVec2(p.x + a, p.y + a - b), c, 2);
		if (!(opt.names && scale >= 70))
			dl->AddText(ImVec2(p.x + a + 4, p.y - 6), c, s->name.c_str());
	};
	if (has_from)
	{
		const Star *f = g.star(from);
		if (f && opt.range_ly > 0 && ly_per_sector > 0)
		{
			/* the range on the plane through the star */
			double rad = opt.range_ly / ly_per_sector;
			const int N = 96;
			ImVec2 pts[N];
			for (int i = 0; i < N; i++)
			{
				double a = i * 2 * PI / N;
				pts[i] = to_screen(star_x(*f) + cos(a) * rad, star_y(*f) + sin(a) * rad, star_z(*f), o, size, opt);
			}
			dl->AddPolyline(pts, N, IM_COL32(80, 220, 120, 200), ImDrawFlags_Closed, 1.5f);
		}
		mark(from, IM_COL32(80, 220, 120, 255), 8);
	}
	if (has_selected)
		mark(selected, col(0xff8800), 10); /* the game's orange */
	if (has_hover && (!has_selected || hovered != selected))
		mark(hovered, IM_COL32(220, 220, 255, 200), 7);

	if (clicked)
	{
		if (has_hover)
			select(hovered);
	}
	if (dbl && has_hover)
	{
		const Star *s = g.star(hovered);
		if (s)
			go_to(star_x(*s), star_y(*s), std::max(scale, 400.0));
	}
	dl->PopClipRect();
}
