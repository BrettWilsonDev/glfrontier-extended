/*
 * mesh.h - the studio's editable mesh. Coordinates are Blender/OBJ style:
 * metres, x right, y up, the nose towards -z. Faces are vertex index lists,
 * counter-clockwise from outside, with a material name fe2_XXX (the game's
 * colour value) plus "_2s" for faces drawn from both sides. Collision hulls
 * are separate little meshes the game never draws.
 */
#pragma once

#include <set>
#include <string>
#include <vector>

#include "fe2build.h"
#include "fe2core.h"

namespace studio
{

using fe2::Vec3;

struct Face
{
	std::vector<int> v;
	std::string m;
};

struct Hull
{
	std::string name;
	std::vector<Vec3> verts;
	std::vector<std::vector<int>> faces;
};

struct GamePart
{
	std::string key, label;
	int faces;
	bool on;
};

std::string mat_name(int colour, bool two_sided);
void mat_colour(const std::string &name, int &colour, bool &two_sided);
Vec3 mat_rgb(const std::string &name, int paint);
int make_colour(int r, int g, int b, bool glow, bool paint);

class Mesh
{
  public:
	std::vector<Vec3> verts;
	std::vector<Face> faces;
	std::vector<Hull> hulls;

	void checkpoint();
	bool undo();
	bool redo();

	void bounds(Vec3 &lo, Vec3 &hi) const;
	Vec3 size() const;
	Vec3 face_normal(const Face &f) const;
	Vec3 face_centre(const Face &f) const;
	std::set<int> used_verts() const;
	std::vector<std::string> materials() const;

	/* loading and saving */
	static Mesh from_game(const fe2::Image &image, int index, bool gear = false, const std::set<std::string> *parts = nullptr);
	static bool from_obj(const std::string &path, Mesh &out, std::string &err);
	bool save_obj(const std::string &path, std::string &err) const;
	fe2::BuildMesh to_build() const;

	/* editing (call checkpoint() first) */
	std::vector<int> add_part(const std::vector<Vec3> &v, const std::vector<std::vector<int>> &f, const std::string &mat);
	void move(const std::set<int> &vs, const Vec3 &d, bool symmetric);
	void scale(const std::set<int> &vs, const Vec3 &k);
	void rotate(const std::set<int> &vs, int axis, double degrees);
	Vec3 centre_of(const std::set<int> &vs) const;
	void delete_faces(const std::set<int> &fs);
	void delete_verts(const std::set<int> &vs);
	void flip_faces(const std::set<int> &fs);
	void paint(const std::set<int> &fs, const std::string &mat);
	bool make_face(const std::vector<int> &vlist, const std::string &mat);
	void extrude(const std::set<int> &fs, double dist);
	void mirror_x();
	int recalc_outside();
	void weld(double dist);
	void transform_all(double scale, const Vec3 &offset);
	void add_hull_box(const Vec3 &lo, const Vec3 &hi);

  private:
	struct State
	{
		std::vector<Vec3> verts;
		std::vector<Face> faces;
		std::vector<Hull> hulls;
	};
	std::vector<State> undo_stack, redo_stack;
	std::set<int> with_mirrors(const std::set<int> &vs) const;
};

/* the parts of a game model: its own hull and each sub-model it calls */
std::vector<GamePart> game_parts(const fe2::Image &image, int index);
std::string part_key(const std::string &group);

/* primitives: counter-clockwise from outside */
void prim_box(const Vec3 &lo, const Vec3 &hi, std::vector<Vec3> &v, std::vector<std::vector<int>> &f);
void prim_wedge(const Vec3 &size, std::vector<Vec3> &v, std::vector<std::vector<int>> &f);
void prim_cylinder(double r, double length, int sides, double r2, std::vector<Vec3> &v, std::vector<std::vector<int>> &f);
void prim_pyramid(const Vec3 &size, std::vector<Vec3> &v, std::vector<std::vector<int>> &f);
void prim_sphere(double r, int rings, int sides, std::vector<Vec3> &v, std::vector<std::vector<int>> &f);

} // namespace studio
