/*
 * view.h - the studio's 3D view: an orbit camera, an OpenGL renderer that
 * draws into a texture (shown with ImGui::Image), and picking.
 */
#pragma once

#include <set>
#include <string>
#include <vector>

#include "mesh.h"

namespace studio
{

struct Camera
{
	double yaw = 0.7, pitch = 0.45, dist = 40;
	Vec3 target;
	double fov = 50; /* degrees */
	void axes(Vec3 &right, Vec3 &up, Vec3 &fwd) const;
	Vec3 eye() const;
	/* world -> pixel (and depth); false if behind the camera */
	bool project(const Vec3 &p, float w, float h, float &sx, float &sy) const;
	void ray(float sx, float sy, float w, float h, Vec3 &origin, Vec3 &dir) const;
};

/* what the renderer draws */
struct DrawFace
{
	std::vector<Vec3> pts;
	int colour;
	bool two_sided;
	bool selected;
	int id; /* face index, -1 for the game result */
};

struct ViewOptions
{
	bool cull = true, wire = false, hulls = true, verts = false;
	int paint = 0x666;
};

class Renderer
{
  public:
	bool init();
	/* draw into an offscreen texture of size w x h, return the GL texture */
	unsigned int render(int w, int h, const Camera &cam, const std::vector<DrawFace> &faces,
						const std::vector<std::vector<Vec3>> &hull_lines, const std::vector<Vec3> &points,
						const std::vector<Vec3> &selected_points, const ViewOptions &opt);

  private:
	unsigned int prog = 0, line_prog = 0, vao = 0, vbo = 0, fbo = 0, tex = 0, depth = 0;
	int tw = 0, th = 0;
};

/* nearest face under the pixel (respecting culling), -1 if none */
int pick_face(const Camera &cam, float sx, float sy, float w, float h, const std::vector<DrawFace> &faces, bool cull);
/* nearest vertex within `radius` pixels, -1 if none */
int pick_vertex(const Camera &cam, float sx, float sy, float w, float h, const std::vector<Vec3> &verts,
				const std::set<int> &candidates, float radius = 10);

} // namespace studio
