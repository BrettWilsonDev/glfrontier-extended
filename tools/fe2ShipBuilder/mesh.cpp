/*
 * mesh.cpp - see mesh.h.
 */
#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace studio
{

static const double PI = 3.14159265358979323846;

std::string mat_name(int colour, bool two)
{
	char b[32];
	snprintf(b, sizeof(b), "fe2_%03x%s", colour & 0xFFF, two ? "_2s" : "");
	return b;
}

void mat_colour(const std::string &name, int &colour, bool &two)
{
	std::string n = name;
	for (auto &c : n)
		c = (char)tolower((unsigned char)c);
	colour = 0x666;
	two = n.size() >= 3 && n.compare(n.size() - 3, 3, "_2s") == 0;
	if (n.rfind("fe2_", 0) == 0 && n.size() >= 7)
	{
		char *end = nullptr;
		std::string hex = n.substr(4, 3);
		long v = strtol(hex.c_str(), &end, 16);
		if (end && *end == 0)
			colour = (int)v;
	}
}

Vec3 mat_rgb(const std::string &name, int paint)
{
	int c;
	bool two;
	mat_colour(name, c, two);
	return fe2::display_rgb(c, paint);
}

int make_colour(int r, int g, int b, bool glow, bool paint)
{
	return (r << 9) | (g << 5) | (b << 1) | (glow ? 0x100 : 0) | (paint ? 0x010 : 0);
}

/* -- undo ---------------------------------------------------------------------- */

void Mesh::checkpoint()
{
	undo_stack.push_back({verts, faces, hulls});
	if (undo_stack.size() > 100)
		undo_stack.erase(undo_stack.begin());
	redo_stack.clear();
}

bool Mesh::undo()
{
	if (undo_stack.empty())
		return false;
	redo_stack.push_back({verts, faces, hulls});
	auto &s = undo_stack.back();
	verts = s.verts;
	faces = s.faces;
	hulls = s.hulls;
	undo_stack.pop_back();
	return true;
}

bool Mesh::redo()
{
	if (redo_stack.empty())
		return false;
	undo_stack.push_back({verts, faces, hulls});
	auto &s = redo_stack.back();
	verts = s.verts;
	faces = s.faces;
	hulls = s.hulls;
	redo_stack.pop_back();
	return true;
}

/* -- info ---------------------------------------------------------------------- */

void Mesh::bounds(Vec3 &lo, Vec3 &hi) const
{
	auto used = used_verts();
	if (used.empty())
	{
		lo = Vec3(-1, -1, -1);
		hi = Vec3(1, 1, 1);
		return;
	}
	lo = Vec3(1e30, 1e30, 1e30);
	hi = Vec3(-1e30, -1e30, -1e30);
	for (int i : used)
		for (int a = 0; a < 3; a++)
		{
			lo[a] = std::min(lo[a], verts[i][a]);
			hi[a] = std::max(hi[a], verts[i][a]);
		}
}

Vec3 Mesh::size() const
{
	Vec3 lo, hi;
	bounds(lo, hi);
	return hi - lo;
}

Vec3 Mesh::face_normal(const Face &f) const
{
	std::vector<Vec3> pts;
	for (int i : f.v)
		pts.push_back(verts[i]);
	return fe2::normalize(fe2::newell(pts));
}

Vec3 Mesh::face_centre(const Face &f) const
{
	Vec3 c;
	for (int i : f.v)
		c = c + verts[i];
	return c * (1.0 / f.v.size());
}

std::set<int> Mesh::used_verts() const
{
	std::set<int> s;
	for (auto &f : faces)
		s.insert(f.v.begin(), f.v.end());
	return s;
}

std::vector<std::string> Mesh::materials() const
{
	std::set<std::string> s;
	for (auto &f : faces)
		s.insert(f.m);
	return std::vector<std::string>(s.begin(), s.end());
}

/* -- game models ---------------------------------------------------------------- */

std::string part_key(const std::string &group)
{
	std::string g = group.substr(0, group.find('~'));
	g = std::regex_replace(g, std::regex("_if[0-9a-f]{4}"), "");
	size_t first = g.find('_');
	if (first == std::string::npos)
		return g;
	size_t second = g.find('_', first + 1);
	return second == std::string::npos ? g : g.substr(0, second);
}

static const std::map<int, const char *> KNOWN_PARTS = {
	{2, "landing gear"},  {3, "landing gear wheel"}, {111, "engines"},	  {120, "equipment"},
	{121, "missile (pylon)"}, {122, "missile (pylon)"}, {123, "radar / turret"}, {124, "equipment"},
	{109, "cockpit detail"}};

std::vector<GamePart> game_parts(const fe2::Image &image, int index)
{
	auto c = fe2::collect(image, index);
	std::map<std::string, int> count;
	for (auto &f : c.faces)
		if (f.group.find("~far") == std::string::npos)
			count[part_key(f.group)]++;
	std::vector<GamePart> out;
	std::vector<std::string> keys;
	for (auto &kv : count)
		keys.push_back(kv.first);
	std::sort(keys.begin(), keys.end(), [](const std::string &a, const std::string &b) {
		auto na = std::count(a.begin(), a.end(), '_'), nb = std::count(b.begin(), b.end(), '_');
		return na != nb ? na < nb : a < b;
	});
	for (auto &k : keys)
	{
		if (k.find('_') == std::string::npos)
		{
			out.push_back({k, "hull", count[k], true});
			continue;
		}
		int num = atoi(k.substr(k.rfind("_m") + 2).c_str());
		std::string label = "sub-model " + std::to_string(num);
		auto it = KNOWN_PARTS.find(num);
		if (it != KNOWN_PARTS.end())
			label += std::string(" (") + it->second + ")";
		out.push_back({k, label, count[k], num != 2});
	}
	return out;
}

Mesh Mesh::from_game(const fe2::Image &image, int index, bool gear, const std::set<std::string> *parts)
{
	auto c = fe2::collect(image, index);
	Mesh m;
	std::map<std::tuple<long long, long long, long long>, int> vid;
	for (auto &f : c.faces)
	{
		if (f.group.find("~far") != std::string::npos)
			continue;
		std::string key = part_key(f.group);
		if (parts)
		{
			if (!parts->count(key))
				continue;
		}
		else if (!gear && key.size() > 3 && key.compare(key.size() - 3, 3, "_m2") == 0)
			continue;
		std::vector<Vec3> opts;
		for (auto &p : f.pts)
			opts.push_back(Vec3(p.x / 1000.0, p.y / 1000.0, -p.z / 1000.0));
		std::optional<Vec3> nn;
		if (f.normal)
			nn = Vec3(f.normal->x, f.normal->y, -f.normal->z);
		opts = fe2::orient(opts, nn);
		Face face;
		for (auto &p : opts)
		{
			auto k = std::make_tuple(std::llround(p.x * 10000), std::llround(p.y * 10000), std::llround(p.z * 10000));
			auto it = vid.find(k);
			int id;
			if (it == vid.end())
			{
				id = (int)m.verts.size();
				vid[k] = id;
				m.verts.push_back(Vec3(std::get<0>(k) / 10000.0, std::get<1>(k) / 10000.0, std::get<2>(k) / 10000.0));
			}
			else
				id = it->second;
			if (face.v.empty() || face.v.back() != id)
				face.v.push_back(id);
		}
		if (face.v.size() > 1 && face.v.front() == face.v.back())
			face.v.pop_back();
		std::set<int> uniq(face.v.begin(), face.v.end());
		if (uniq.size() >= 3)
		{
			face.m = mat_name(f.colour, !f.normal.has_value());
			m.faces.push_back(face);
		}
	}
	return m;
}

/* -- OBJ --------------------------------------------------------------------------- */

static std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static void read_mtl(const std::string &path, std::map<std::string, Vec3> &kd, std::map<std::string, bool> &em)
{
	std::ifstream f(path);
	std::string line, cur;
	while (std::getline(f, line))
	{
		std::istringstream ss(line);
		std::string t;
		ss >> t;
		if (t == "newmtl")
			cur = trim(line.substr(line.find("newmtl") + 6));
		else if (!cur.empty() && (t == "Kd" || t == "Ke"))
		{
			Vec3 v;
			ss >> v.x >> v.y >> v.z;
			if (t == "Kd")
				kd[cur] = v;
			else
				em[cur] = v.x > 0 || v.y > 0 || v.z > 0;
		}
	}
}

bool Mesh::from_obj(const std::string &path, Mesh &out, std::string &err)
{
	std::ifstream f(path);
	if (!f)
	{
		err = "can't open " + path;
		return false;
	}
	out = Mesh();
	std::vector<Vec3> verts;
	std::map<std::string, Vec3> kd;
	std::map<std::string, bool> em;
	std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
	std::string mat, group, line;
	std::map<int, int> vmap;
	std::map<std::string, Hull> hulls;
	std::map<std::string, std::map<int, int>> hmap;
	while (std::getline(f, line))
	{
		std::istringstream ss(line);
		std::string t;
		ss >> t;
		if (t == "v")
		{
			Vec3 v;
			ss >> v.x >> v.y >> v.z;
			verts.push_back(v);
		}
		else if (t == "mtllib")
			read_mtl(dir + trim(line.substr(line.find("mtllib") + 6)), kd, em);
		else if (t == "usemtl")
			mat = trim(line.substr(line.find("usemtl") + 6));
		else if (t == "o" || t == "g")
		{
			std::string g = trim(line.substr(1));
			if (!g.empty())
				group = g;
		}
		else if (t == "f")
		{
			std::vector<int> idx;
			std::string tok;
			while (ss >> tok)
			{
				int i = atoi(tok.c_str());
				idx.push_back(i > 0 ? i - 1 : (int)verts.size() + i);
			}
			bool bad = false;
			for (int i : idx)
				if (i < 0 || i >= (int)verts.size())
					bad = true;
			if (bad || idx.size() < 3)
				continue;
			std::string g = group, lm = mat;
			for (auto &c : g)
				c = (char)tolower((unsigned char)c);
			for (auto &c : lm)
				c = (char)tolower((unsigned char)c);
			if (g.rfind("collision", 0) == 0 || g.rfind("col_", 0) == 0 || lm.rfind("collision", 0) == 0)
			{
				std::string name = group.empty() ? "collision" : group;
				Hull &h = hulls[name];
				h.name = name;
				std::vector<int> hf;
				for (int i : idx)
				{
					auto &hm = hmap[name];
					if (!hm.count(i))
					{
						hm[i] = (int)h.verts.size();
						h.verts.push_back(verts[i]);
					}
					hf.push_back(hm[i]);
				}
				h.faces.push_back(hf);
				continue;
			}
			Face face;
			for (int i : idx)
			{
				if (!vmap.count(i))
				{
					vmap[i] = (int)out.verts.size();
					out.verts.push_back(verts[i]);
				}
				face.v.push_back(vmap[i]);
			}
			if (lm.rfind("fe2_", 0) == 0)
				face.m = mat;
			else
			{
				auto k = kd.find(mat);
				auto e = em.find(mat);
				int c = fe2::material_colour(mat, k != kd.end() ? &k->second : nullptr, e != em.end() && e->second);
				face.m = mat_name(c, fe2::two_sided(mat));
			}
			out.faces.push_back(face);
		}
	}
	for (auto &kv : hulls)
		out.hulls.push_back(kv.second);
	return true;
}

bool Mesh::save_obj(const std::string &path, std::string &err) const
{
	std::string base = path;
	size_t dot = base.find_last_of('.');
	size_t slash = base.find_last_of("/\\");
	if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
		base = base.substr(0, dot);
	std::string name = base.substr(slash == std::string::npos ? 0 : slash + 1);
	std::ofstream m(base + ".mtl");
	std::ofstream o(base + ".obj");
	if (!m || !o)
	{
		err = "can't write " + base + ".obj";
		return false;
	}
	m << "# FE2 Ship Builder\n";
	for (auto &mn : materials())
	{
		Vec3 c = mat_rgb(mn, 0x666);
		m << "\nnewmtl " << mn << "\nKd " << c.x << " " << c.y << " " << c.z << "\nillum 1\n";
	}
	m << "\nnewmtl collision\nKd 1 0 1\nillum 1\n";
	o << "# FE2 Ship Builder: metres, Y up, nose towards -Z\nmtllib " << name << ".mtl\no " << name << "\n";
	auto used = used_verts();
	std::map<int, int> remap;
	int n = 1;
	char buf[128];
	for (int v : used)
	{
		remap[v] = n++;
		snprintf(buf, sizeof(buf), "v %.5f %.5f %.5f\n", verts[v].x, verts[v].y, verts[v].z);
		o << buf;
	}
	std::string cur;
	for (auto &f : faces)
	{
		if (f.m != cur)
		{
			o << "usemtl " << f.m << "\n";
			cur = f.m;
		}
		o << "f";
		for (int i : f.v)
			o << " " << remap[i];
		o << "\n";
	}
	int base_i = (int)used.size();
	for (auto &h : hulls)
	{
		o << "o " << h.name << "\nusemtl collision\n";
		for (auto &v : h.verts)
		{
			snprintf(buf, sizeof(buf), "v %.5f %.5f %.5f\n", v.x, v.y, v.z);
			o << buf;
		}
		for (auto &hf : h.faces)
		{
			o << "f";
			for (int i : hf)
				o << " " << base_i + i + 1;
			o << "\n";
		}
		base_i += (int)h.verts.size();
	}
	return true;
}

fe2::BuildMesh Mesh::to_build() const
{
	/* game mm, FE2 axes: z flipped (the builder expects OBJ winding). "/ 0.001"
	 * rather than "* 1000" so it matches tools/fe2ShipBuilder bit for bit */
	fe2::BuildMesh b;
	std::map<int, int> remap;
	for (auto &f : faces)
	{
		fe2::BuildMesh::Face bf;
		for (int i : f.v)
		{
			if (!remap.count(i))
			{
				remap[i] = (int)b.verts.size();
				b.verts.push_back(Vec3(verts[i].x / 0.001, verts[i].y / 0.001, -verts[i].z / 0.001));
			}
			bf.idx.push_back(remap[i]);
		}
		bf.mat = f.m;
		b.faces.push_back(bf);
	}
	for (auto &h : hulls)
	{
		int base = (int)b.verts.size();
		for (auto &v : h.verts)
			b.verts.push_back(Vec3(v.x / 0.001, v.y / 0.001, -v.z / 0.001));
		for (auto &hf : h.faces)
		{
			fe2::BuildMesh::Face bf;
			for (int i : hf)
				bf.idx.push_back(base + i);
			bf.mat = "collision";
			bf.group = h.name.rfind("collision", 0) == 0 ? h.name : "collision_" + h.name;
			b.faces.push_back(bf);
		}
	}
	return b;
}

/* -- editing ---------------------------------------------------------------------- */

std::vector<int> Mesh::add_part(const std::vector<Vec3> &v, const std::vector<std::vector<int>> &f, const std::string &mat)
{
	int base = (int)verts.size();
	verts.insert(verts.end(), v.begin(), v.end());
	for (auto &fi : f)
	{
		Face face;
		for (int i : fi)
			face.v.push_back(base + i);
		face.m = mat;
		faces.push_back(face);
	}
	std::vector<int> out;
	for (int i = base; i < (int)verts.size(); i++)
		out.push_back(i);
	return out;
}

std::set<int> Mesh::with_mirrors(const std::set<int> &vs) const
{
	std::set<int> out = vs;
	for (int i : vs)
	{
		const Vec3 &p = verts[i];
		if (std::fabs(p.x) < 1e-6)
			continue;
		for (int j = 0; j < (int)verts.size(); j++)
			if (std::fabs(verts[j].x + p.x) < 1e-4 && std::fabs(verts[j].y - p.y) < 1e-4 &&
				std::fabs(verts[j].z - p.z) < 1e-4)
				out.insert(j);
	}
	return out;
}

void Mesh::move(const std::set<int> &vs, const Vec3 &d, bool symmetric)
{
	if (!symmetric)
	{
		for (int i : vs)
			verts[i] = verts[i] + d;
		return;
	}
	for (int i : with_mirrors(vs))
		verts[i] = verts[i] + (vs.count(i) ? d : Vec3(-d.x, d.y, d.z));
}

Vec3 Mesh::centre_of(const std::set<int> &vs) const
{
	if (vs.empty())
		return Vec3();
	Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
	for (int i : vs)
		for (int a = 0; a < 3; a++)
		{
			lo[a] = std::min(lo[a], verts[i][a]);
			hi[a] = std::max(hi[a], verts[i][a]);
		}
	return (lo + hi) * 0.5;
}

void Mesh::scale(const std::set<int> &vs, const Vec3 &k)
{
	if (vs.empty())
		return;
	Vec3 c = centre_of(vs);
	for (int i : vs)
		for (int a = 0; a < 3; a++)
			verts[i][a] = c[a] + (verts[i][a] - c[a]) * k[a];
	if (k.x * k.y * k.z < 0)
		for (auto &f : faces)
		{
			bool all = true;
			for (int i : f.v)
				if (!vs.count(i))
					all = false;
			if (all)
				std::reverse(f.v.begin(), f.v.end());
		}
}

void Mesh::rotate(const std::set<int> &vs, int axis, double degrees)
{
	if (vs.empty())
		return;
	Vec3 c = centre_of(vs);
	double a = degrees * PI / 180, ca = std::cos(a), sa = std::sin(a);
	int u = (axis + 1) % 3, w = (axis + 2) % 3;
	for (int i : vs)
	{
		Vec3 p = verts[i] - c;
		double pu = p[u] * ca - p[w] * sa, pw = p[u] * sa + p[w] * ca;
		p[u] = pu;
		p[w] = pw;
		verts[i] = p + c;
	}
}

void Mesh::delete_faces(const std::set<int> &fs)
{
	std::vector<Face> keep;
	for (int k = 0; k < (int)faces.size(); k++)
		if (!fs.count(k))
			keep.push_back(faces[k]);
	faces = keep;
}

void Mesh::delete_verts(const std::set<int> &vs)
{
	std::vector<Face> keep;
	for (auto &f : faces)
	{
		bool hit = false;
		for (int i : f.v)
			if (vs.count(i))
				hit = true;
		if (!hit)
			keep.push_back(f);
	}
	faces = keep;
}

void Mesh::flip_faces(const std::set<int> &fs)
{
	for (int k : fs)
		if (k < (int)faces.size())
			std::reverse(faces[k].v.begin(), faces[k].v.end());
}

void Mesh::paint(const std::set<int> &fs, const std::string &mat)
{
	for (int k : fs)
		if (k < (int)faces.size())
			faces[k].m = mat;
}

bool Mesh::make_face(const std::vector<int> &vlist, const std::string &mat)
{
	if (vlist.size() < 3)
		return false;
	Face f{vlist, mat};
	Vec3 n = face_normal(f), lo, hi;
	bounds(lo, hi);
	if (fe2::dot(n, face_centre(f) - (lo + hi) * 0.5) < 0)
		std::reverse(f.v.begin(), f.v.end());
	faces.push_back(f);
	return true;
}

void Mesh::extrude(const std::set<int> &fs, double dist)
{
	for (int k : fs)
	{
		if (k >= (int)faces.size())
			continue;
		Vec3 n = face_normal(faces[k]);
		std::vector<int> old = faces[k].v, top;
		for (int i : old)
		{
			top.push_back((int)verts.size());
			verts.push_back(verts[i] + n * dist);
		}
		std::string m = faces[k].m;
		for (size_t j = 0; j < old.size(); j++)
		{
			size_t j2 = (j + 1) % old.size();
			faces.push_back({{old[j], old[j2], top[j2], top[j]}, m});
		}
		faces[k].v = top;
	}
}

void Mesh::mirror_x()
{
	std::vector<Face> keep, out;
	for (auto &f : faces)
	{
		bool right = true;
		for (int i : f.v)
			if (verts[i].x < -1e-4)
				right = false;
		if (right)
			keep.push_back(f);
	}
	std::map<int, int> vmap;
	for (auto &f : keep)
	{
		out.push_back(f);
		std::vector<int> mv;
		bool off_axis = false;
		for (int i : f.v)
		{
			if (std::fabs(verts[i].x) < 1e-4)
			{
				verts[i].x = 0;
				mv.push_back(i);
			}
			else
			{
				off_axis = true;
				if (!vmap.count(i))
				{
					vmap[i] = (int)verts.size();
					verts.push_back(Vec3(-verts[i].x, verts[i].y, verts[i].z));
				}
				mv.push_back(vmap[i]);
			}
		}
		if (off_axis)
		{
			std::reverse(mv.begin(), mv.end());
			out.push_back({mv, f.m});
		}
	}
	faces = out;
}

int Mesh::recalc_outside()
{
	/* the builder's orientation fix works in FE2 axes (z flipped) */
	fe2::BuildMesh b;
	for (auto &v : verts)
		b.verts.push_back(Vec3(v.x, v.y, -v.z));
	for (auto &f : faces)
		b.faces.push_back({f.v, f.m, ""});
	fe2::BuildOptions opt;
	/* reuse the build's orient pass through a tiny trick: compare windings
	 * before and after running it via build_model is too heavy, so it's
	 * done here directly */
	std::map<std::pair<int, int>, std::vector<int>> edges;
	for (int fi = 0; fi < (int)faces.size(); fi++)
		for (size_t k = 0; k < faces[fi].v.size(); k++)
		{
			int a = faces[fi].v[k], c = faces[fi].v[(k + 1) % faces[fi].v.size()];
			edges[{std::min(a, c), std::max(a, c)}].push_back(fi);
		}
	std::vector<bool> seen(faces.size(), false);
	int flipped = 0;
	auto directed = [&](int fi, int a, int c) {
		auto &v = faces[fi].v;
		for (size_t k = 0; k < v.size(); k++)
			if (v[k] == a && v[(k + 1) % v.size()] == c)
				return true;
		return false;
	};
	for (int s = 0; s < (int)faces.size(); s++)
	{
		if (seen[s])
			continue;
		std::vector<int> comp = {s}, todo = {s};
		seen[s] = true;
		while (!todo.empty())
		{
			int fi = todo.back();
			todo.pop_back();
			auto v = faces[fi].v;
			for (size_t k = 0; k < v.size(); k++)
			{
				int a = v[k], c = v[(k + 1) % v.size()];
				auto &sh = edges[{std::min(a, c), std::max(a, c)}];
				if (sh.size() != 2)
					continue;
				for (int nb : sh)
				{
					if (seen[nb])
						continue;
					seen[nb] = true;
					if (directed(nb, a, c))
					{
						std::reverse(faces[nb].v.begin(), faces[nb].v.end());
						flipped++;
					}
					comp.push_back(nb);
					todo.push_back(nb);
				}
			}
		}
		bool closed = true;
		for (int fi : comp)
			for (size_t k = 0; k < faces[fi].v.size(); k++)
			{
				int a = faces[fi].v[k], c = faces[fi].v[(k + 1) % faces[fi].v.size()];
				if (edges[{std::min(a, c), std::max(a, c)}].size() != 2)
					closed = false;
			}
		if (!closed)
			continue;
		double vol = 0;
		for (int fi : comp)
		{
			std::vector<Vec3> pts;
			for (int i : faces[fi].v)
				pts.push_back(verts[i]);
			vol += fe2::dot(pts[0], fe2::newell(pts));
		}
		if (vol < 0) /* right handed: outward faces give a positive volume */
		{
			for (int fi : comp)
				std::reverse(faces[fi].v.begin(), faces[fi].v.end());
			flipped += (int)comp.size();
		}
	}
	return flipped;
}

void Mesh::weld(double dist)
{
	std::map<std::tuple<long long, long long, long long>, int> grid;
	std::vector<int> remap(verts.size());
	for (int i = 0; i < (int)verts.size(); i++)
	{
		auto key = std::make_tuple(std::llround(verts[i].x / dist), std::llround(verts[i].y / dist),
								   std::llround(verts[i].z / dist));
		auto it = grid.find(key);
		remap[i] = it == grid.end() ? (grid[key] = i) : it->second;
	}
	std::vector<Face> keep;
	for (auto &f : faces)
	{
		std::vector<int> nv;
		for (int i : f.v)
		{
			int j = remap[i];
			if (nv.empty() || nv.back() != j)
				nv.push_back(j);
		}
		if (nv.size() > 1 && nv.front() == nv.back())
			nv.pop_back();
		std::set<int> u(nv.begin(), nv.end());
		if (u.size() >= 3)
			keep.push_back({nv, f.m});
	}
	faces = keep;
}

void Mesh::transform_all(double k, const Vec3 &off)
{
	for (auto &v : verts)
		v = v * k + off;
	for (auto &h : hulls)
		for (auto &v : h.verts)
			v = v * k + off;
}

void Mesh::add_hull_box(const Vec3 &lo, const Vec3 &hi)
{
	Hull h;
	h.name = "collision_" + std::to_string(hulls.size() + 1);
	prim_box(lo, hi, h.verts, h.faces);
	hulls.push_back(h);
}

/* -- primitives ------------------------------------------------------------------- */

void prim_box(const Vec3 &lo, const Vec3 &hi, std::vector<Vec3> &v, std::vector<std::vector<int>> &f)
{
	double x0 = lo.x, y0 = lo.y, z0 = lo.z, x1 = hi.x, y1 = hi.y, z1 = hi.z;
	v = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
	f = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
}

void prim_wedge(const Vec3 &s, std::vector<Vec3> &v, std::vector<std::vector<int>> &f)
{
	double w = s.x, h = s.y, l = s.z;
	v = {{0, 0, -l / 2}, {w / 2, 0, l / 2}, {-w / 2, 0, l / 2}, {0, h / 2, l / 3}, {0, -h / 2, l / 3}};
	f = {{0, 3, 1}, {0, 2, 3}, {0, 1, 4}, {0, 4, 2}, {1, 3, 2}, {1, 2, 4}};
}

void prim_cylinder(double r, double length, int sides, double r2, std::vector<Vec3> &v, std::vector<std::vector<int>> &f)
{
	v.clear();
	f.clear();
	for (int e = 0; e < 2; e++)
	{
		double zz = e ? length / 2 : -length / 2, rr = e ? r2 : r;
		for (int k = 0; k < sides; k++)
		{
			double a = 2 * PI * k / sides;
			v.push_back(Vec3(rr * std::cos(a), rr * std::sin(a), zz));
		}
	}
	for (int k = 0; k < sides; k++)
	{
		int j = (k + 1) % sides;
		f.push_back({k, j, sides + j, sides + k});
	}
	std::vector<int> c0, c1;
	for (int k = 0; k < sides; k++)
	{
		c0.push_back(k);
		c1.push_back(sides + k);
	}
	std::reverse(c1.begin(), c1.end());
	f.push_back(c0);
	f.push_back(c1);
}

void prim_pyramid(const Vec3 &s, std::vector<Vec3> &v, std::vector<std::vector<int>> &f)
{
	double w = s.x, h = s.y, l = s.z;
	v = {{-w / 2, -h / 2, l / 2}, {w / 2, -h / 2, l / 2}, {w / 2, h / 2, l / 2}, {-w / 2, h / 2, l / 2}, {0, 0, -l / 2}};
	f = {{0, 1, 2, 3}, {0, 4, 1}, {1, 4, 2}, {2, 4, 3}, {3, 4, 0}};
}

void prim_sphere(double r, int rings, int sides, std::vector<Vec3> &v, std::vector<std::vector<int>> &f)
{
	v = {Vec3(0, r, 0)};
	f.clear();
	for (int ri = 1; ri < rings; ri++)
	{
		double a = PI * ri / rings;
		for (int k = 0; k < sides; k++)
		{
			double b = 2 * PI * k / sides;
			v.push_back(Vec3(r * std::sin(a) * std::cos(b), r * std::cos(a), r * std::sin(a) * std::sin(b)));
		}
	}
	v.push_back(Vec3(0, -r, 0));
	for (int k = 0; k < sides; k++)
		f.push_back({0, 1 + (k + 1) % sides, 1 + k});
	for (int ri = 0; ri < rings - 2; ri++)
		for (int k = 0; k < sides; k++)
		{
			int a = 1 + ri * sides + k, b = 1 + ri * sides + (k + 1) % sides;
			f.push_back({a, b, b + sides, a + sides});
		}
	int last = (int)v.size() - 1, base = 1 + (rings - 2) * sides;
	for (int k = 0; k < sides; k++)
		f.push_back({last, base + k, base + (k + 1) % sides});
}

} // namespace studio
