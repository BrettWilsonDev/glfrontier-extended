/*
 * map_view.h - the zoomable galaxy map: from the whole galaxy (the game's
 * density map) down to the stars of one sector, with names, a sector grid,
 * height stalks when tilted, and a range circle.
 *
 * World coordinates are sectors as doubles (x right, y up, like the game's
 * galaxy map); a star at 16.16 position p is at p / 65536.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "galaxy.h"
#include "imgui.h"

struct MapOptions
{
	enum Colour
	{
		BY_STAR,
		BY_ALLEGIANCE,
		BY_EXPLORED,
	};
	int colour = BY_STAR;
	bool grid = true;
	bool names = true;
	bool density = true;	/* galaxy density background */
	bool counts = false;	/* per-sector star counts as a heat map */
	float tilt = 0;			/* degrees, 0 = straight down */
	bool stalks = true;		/* lines from tilted stars to the plane */
	float range_ly = 0;		/* circle round the "from" star, 0 = none */
	float star_size = 1.0f; /* multiplier */
	bool hue = false;		/* tint where people live: systems with starports (the survey) */
};

class MapView
{
  public:
	double cx = galaxy::SOL_X + 0.5, cy = galaxy::SOL_Y + 0.5; /* centre, in sectors */
	double scale = 40;										   /* pixels per sector */

	uint32_t hovered = 0, selected = 0, from = 0; /* star ids, 0 = none */
	bool has_hover = false, has_selected = false, has_from = false;
	bool selection_changed = false;
	double mouse_x = 0, mouse_y = 0; /* sector coordinates under the mouse */
	bool mouse_inside = false;

	float ly_per_sector = 8; /* from the game's distance routine, see App::init */
	int vx0 = 0, vy0 = 0, vx1 = 0, vy1 = 0; /* the sectors in view at the last draw */
	int loading = 0;		 /* sectors still to generate for this view */
	unsigned int density_tex = 0; /* GL texture of the whole galaxy, SECTORS/density_size per texel */
	int density_size = 0;

	void draw(galaxy::Galaxy &g, const MapOptions &opt, ImVec2 pos, ImVec2 size);
	void go_to(double x, double y, double new_scale = 0); /* animated */
	void select(uint32_t id);
	void update(float dt);
	/* where a star was drawn at the last draw() */
	bool star_screen(galaxy::Galaxy &g, uint32_t id, ImVec2 &out) const;

  private:
	double target_cx = 0, target_cy = 0, target_scale = 0;
	bool animating = false;
	double tilt_ = 0; /* radians, from the last draw */
	ImVec2 last_pos_, last_size_;
	MapOptions last_opt_;
	bool dragging = false;
	ImVec2 drag_start;
	double drag_cx = 0, drag_cy = 0;

	ImVec2 to_screen(double x, double y, double z, ImVec2 origin, ImVec2 size, const MapOptions &opt) const;
	void from_screen(ImVec2 p, ImVec2 origin, ImVec2 size, double &x, double &y) const;
};

uint32_t allegiance_colour(int allegiance);

/* Two finger pinch on touch screens: zoom by spreading the fingers, move by
 * dragging both. The host passes its finger events on in ImGui coordinates
 * (touch_finger); a view takes the frame's change when the fingers are over
 * it (touch_pinch_take), and the host clears what is left after the frame
 * (touch_pinch_end_frame). One finger is left to ImGui's mouse (the host's
 * touch to mouse emulation), so single finger drags and taps work as before. */
enum TouchFingerEvent
{
	TOUCH_DOWN,
	TOUCH_MOVE,
	TOUCH_UP
};
void touch_finger(long long id, TouchFingerEvent what, float x, float y);
bool touch_pinching(); /* two fingers are down */
/* The zoom factor, pan and centre since the last take, if the fingers are
 * inside [rmin, rmax) */
bool touch_pinch_take(ImVec2 rmin, ImVec2 rmax, ImVec2 &centre, float &zoom, ImVec2 &pan);
void touch_pinch_end_frame();
/* Set for the frame by a widget whose own drag must not scroll the panel
 * under it (the map, the orbit view); the host's touch drag scrolling
 * (ui.cpp) reads and clears it */
extern bool touch_scroll_block;
