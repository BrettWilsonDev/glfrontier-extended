/*
 * view.cpp - see view.h.
 */
#include "view.h"

#include <glad/glad.h>

#include <cmath>
#include <cstdio>

#include "fe2build.h"

namespace studio
{

static const double PI = 3.14159265358979323846;

/* -- camera -------------------------------------------------------------------- */

void Camera::axes(Vec3 &right, Vec3 &up, Vec3 &fwd) const
{
	double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
	right = Vec3(cy, 0, -sy);
	up = Vec3(-sy * sp, cp, -cy * sp);
	fwd = Vec3(sy * cp, sp, cy * cp); /* from the target towards the camera */
}

Vec3 Camera::eye() const
{
	Vec3 r, u, f;
	axes(r, u, f);
	return target + f * dist;
}

bool Camera::project(const Vec3 &p, float w, float h, float &sx, float &sy) const
{
	Vec3 r, u, f;
	axes(r, u, f);
	Vec3 d = p - eye();
	double z = -fe2::dot(d, f);
	if (z < 0.01)
		return false;
	double k = (h / 2) / std::tan(fov * PI / 360);
	sx = (float)(w / 2 + fe2::dot(d, r) * k / z);
	sy = (float)(h / 2 - fe2::dot(d, u) * k / z);
	return true;
}

void Camera::ray(float sx, float sy, float w, float h, Vec3 &origin, Vec3 &dir) const
{
	Vec3 r, u, f;
	axes(r, u, f);
	double k = (h / 2) / std::tan(fov * PI / 360);
	origin = eye();
	dir = fe2::normalize(r * ((sx - w / 2) / k) - u * ((sy - h / 2) / k) - f);
}

/* -- GL helpers ------------------------------------------------------------------ */

static unsigned int shader(GLenum type, const char *src)
{
	unsigned int s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	int ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		fprintf(stderr, "shader: %s\n", log);
	}
	return s;
}

static unsigned int program(const char *vs, const char *fs)
{
	unsigned int p = glCreateProgram();
	glAttachShader(p, shader(GL_VERTEX_SHADER, vs));
	glAttachShader(p, shader(GL_FRAGMENT_SHADER, fs));
	glBindAttribLocation(p, 0, "pos");
	glBindAttribLocation(p, 1, "nrm");
	glBindAttribLocation(p, 2, "col");
	glLinkProgram(p);
	return p;
}

static const char *FACE_VS = R"(#version 330 core
in vec3 pos; in vec3 nrm; in vec4 col;
uniform mat4 mvp;
out vec3 v_n; out vec4 v_c;
void main() { gl_Position = mvp * vec4(pos, 1.0); v_n = nrm; v_c = col; })";

/* col.a: 1 = lit, 0 = glow (not lit), 2 = selected + lit */
static const char *FACE_FS = R"(#version 330 core
in vec3 v_n; in vec4 v_c;
uniform vec3 light;
out vec4 frag;
void main() {
	vec3 n = normalize(v_n);
	if (!gl_FrontFacing) n = -n;
	float lam = max(dot(n, light), 0.0);
	vec3 c = v_c.rgb;
	if (v_c.a > 0.5) c = c * (0.35 + 0.65 * lam) + 0.06;
	if (v_c.a > 1.5) c = mix(c, vec3(1.0, 0.85, 0.2), 0.45);
	frag = vec4(c, 1.0);
})";

static const char *LINE_VS = R"(#version 330 core
in vec3 pos; in vec3 nrm; in vec4 col;
uniform mat4 mvp; uniform float psize;
out vec4 v_c;
void main() { gl_Position = mvp * vec4(pos, 1.0); gl_PointSize = psize; v_c = col; })";

static const char *LINE_FS = R"(#version 330 core
in vec4 v_c; out vec4 frag;
void main() { frag = v_c; })";

bool Renderer::init()
{
	prog = program(FACE_VS, FACE_FS);
	line_prog = program(LINE_VS, LINE_FS);
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	return true;
}

struct V
{
	float p[3], n[3], c[4];
};

static void mat_mul(const float *a, const float *b, float *out)
{
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++)
		{
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += a[k * 4 + r] * b[c * 4 + k];
			out[c * 4 + r] = s;
		}
}

static void camera_matrix(const Camera &cam, float aspect, float *mvp)
{
	Vec3 r, u, f;
	cam.axes(r, u, f);
	Vec3 e = cam.eye();
	float view[16] = {(float)r.x, (float)u.x, (float)f.x, 0, (float)r.y, (float)u.y, (float)f.y, 0,
					  (float)r.z, (float)u.z, (float)f.z, 0, (float)-fe2::dot(r, e), (float)-fe2::dot(u, e),
					  (float)-fe2::dot(f, e), 1};
	float n = (float)std::max(0.05, cam.dist * 0.01), fa = (float)(cam.dist * 50 + 1000);
	float t = (float)(1.0 / std::tan(cam.fov * PI / 360));
	float proj[16] = {t / aspect, 0, 0, 0, 0, t, 0, 0, 0, 0, (fa + n) / (n - fa), -1, 0, 0, 2 * fa * n / (n - fa), 0};
	mat_mul(proj, view, mvp);
}

static void put(std::vector<V> &out, const Vec3 &p, const Vec3 &n, float r, float g, float b, float a)
{
	out.push_back({{(float)p.x, (float)p.y, (float)p.z}, {(float)n.x, (float)n.y, (float)n.z}, {r, g, b, a}});
}

unsigned int Renderer::render(int w, int h, const Camera &cam, const std::vector<DrawFace> &faces,
							  const std::vector<std::vector<Vec3>> &hull_lines, const std::vector<Vec3> &points,
							  const std::vector<Vec3> &selected_points, const ViewOptions &opt)
{
	if (w < 8 || h < 8)
		return tex;
	if (w != tw || h != th)
	{
		if (fbo)
		{
			glDeleteFramebuffers(1, &fbo);
			glDeleteTextures(1, &tex);
			glDeleteRenderbuffers(1, &depth);
		}
		glGenFramebuffers(1, &fbo);
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenRenderbuffers(1, &depth);
		glBindRenderbuffer(GL_RENDERBUFFER, depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
		tw = w;
		th = h;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
	glClearColor(0, 0, 34 / 255.0f, 1); /* the game's space blue, 0x002 */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	float mvp[16];
	camera_matrix(cam, (float)w / h, mvp);

	/* faces: one sided (culled), then two sided */
	std::vector<V> one, two, lines, pts;
	for (auto &f : faces)
	{
		if (f.pts.size() < 3)
			continue;
		Vec3 n = fe2::normalize(fe2::newell(f.pts));
		Vec3 c = fe2::display_rgb(f.colour, opt.paint, false);
		float a = (f.colour & 0x100) ? 0.0f : 1.0f;
		if (f.selected)
			a = 2.0f;
		auto &dst = (f.two_sided || !opt.cull) ? two : one;
		for (auto &t : fe2::triangulate(f.pts, n))
			for (int k = 0; k < 3; k++)
				put(dst, f.pts[t[k]], n, (float)c.x, (float)c.y, (float)c.z, a);
		/* outline */
		for (size_t i = 0; i < f.pts.size(); i++)
		{
			float oc = f.selected ? 1.0f : 0.0f;
			float r = opt.wire ? (float)c.x : oc * 1.0f, g = opt.wire ? (float)c.y : oc * 0.8f,
				  b = opt.wire ? (float)c.z : oc * 0.2f;
			float al = opt.wire || f.selected ? 1.0f : 0.35f;
			put(lines, f.pts[i], n, r, g, b, al);
			put(lines, f.pts[(i + 1) % f.pts.size()], n, r, g, b, al);
		}
	}
	/* grid and axes */
	std::vector<V> grid;
	for (int i = -10; i <= 10; i++)
	{
		float g = i ? 0.13f : 0.27f;
		put(grid, Vec3(i * 5, 0, -50), Vec3(), g, g, g * 2, 1);
		put(grid, Vec3(i * 5, 0, 50), Vec3(), g, g, g * 2, 1);
		put(grid, Vec3(-50, 0, i * 5), Vec3(), g, g, g * 2, 1);
		put(grid, Vec3(50, 0, i * 5), Vec3(), g, g, g * 2, 1);
	}
	put(grid, Vec3(), Vec3(), 0.9f, 0.25f, 0.25f, 1);
	put(grid, Vec3(4, 0, 0), Vec3(), 0.9f, 0.25f, 0.25f, 1);
	put(grid, Vec3(), Vec3(), 0.25f, 0.9f, 0.25f, 1);
	put(grid, Vec3(0, 4, 0), Vec3(), 0.25f, 0.9f, 0.25f, 1);
	put(grid, Vec3(), Vec3(), 0.3f, 0.5f, 1, 1);
	put(grid, Vec3(0, 0, -8), Vec3(), 0.3f, 0.5f, 1, 1); /* the nose points this way */
	std::vector<V> hl;
	if (opt.hulls)
		for (auto &l : hull_lines)
			for (size_t i = 0; i + 1 < l.size(); i++)
			{
				put(hl, l[i], Vec3(), 1, 0.2f, 1, 1);
				put(hl, l[i + 1], Vec3(), 1, 0.2f, 1, 1);
			}
	for (auto &p : points)
		put(pts, p, Vec3(), 0.65f, 0.75f, 0.9f, 1);
	std::vector<V> spts;
	for (auto &p : selected_points)
		put(spts, p, Vec3(), 1, 0.8f, 0.15f, 1);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	auto upload = [&](const std::vector<V> &v) {
		glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(V), v.data(), GL_STREAM_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void *)0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void *)(3 * sizeof(float)));
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), (void *)(6 * sizeof(float)));
	};

	glEnable(GL_DEPTH_TEST);
	glUseProgram(line_prog);
	glUniformMatrix4fv(glGetUniformLocation(line_prog, "mvp"), 1, GL_FALSE, mvp);
	glUniform1f(glGetUniformLocation(line_prog, "psize"), 1);
	upload(grid);
	glDrawArrays(GL_LINES, 0, (GLsizei)grid.size());

	glUseProgram(prog);
	glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, mvp);
	Vec3 L = fe2::normalize(Vec3(0.35, 0.8, 0.5));
	glUniform3f(glGetUniformLocation(prog, "light"), (float)L.x, (float)L.y, (float)L.z);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1, 1);
	if (!opt.wire)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		upload(one);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)one.size());
		glDisable(GL_CULL_FACE);
		upload(two);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)two.size());
	}
	glDisable(GL_POLYGON_OFFSET_FILL);

	glUseProgram(line_prog);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	upload(lines);
	glDrawArrays(GL_LINES, 0, (GLsizei)lines.size());
	glDisable(GL_DEPTH_TEST);
	upload(hl);
	glDrawArrays(GL_LINES, 0, (GLsizei)hl.size());
	glEnable(GL_PROGRAM_POINT_SIZE);
	glUniform1f(glGetUniformLocation(line_prog, "psize"), 5);
	upload(pts);
	glDrawArrays(GL_POINTS, 0, (GLsizei)pts.size());
	glUniform1f(glGetUniformLocation(line_prog, "psize"), 8);
	upload(spts);
	glDrawArrays(GL_POINTS, 0, (GLsizei)spts.size());
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);
	return tex;
}

/* -- picking ------------------------------------------------------------------------ */

int pick_face(const Camera &cam, float sx, float sy, float w, float h, const std::vector<DrawFace> &faces, bool cull)
{
	Vec3 o, d;
	cam.ray(sx, sy, w, h, o, d);
	double best = 1e300;
	int hit = -1;
	for (auto &f : faces)
	{
		if (f.pts.size() < 3 || f.id < 0)
			continue;
		Vec3 n = fe2::newell(f.pts);
		if (cull && !f.two_sided && fe2::dot(n, d) >= 0)
			continue;
		for (auto &t : fe2::triangulate(f.pts, n))
		{
			const Vec3 &a = f.pts[t[0]], &b = f.pts[t[1]], &c = f.pts[t[2]];
			Vec3 e1 = b - a, e2 = c - a, p = fe2::cross(d, e2);
			double det = fe2::dot(e1, p);
			if (std::fabs(det) < 1e-12)
				continue;
			double inv = 1 / det;
			Vec3 s = o - a;
			double u = fe2::dot(s, p) * inv;
			if (u < 0 || u > 1)
				continue;
			Vec3 q = fe2::cross(s, e1);
			double v = fe2::dot(d, q) * inv;
			if (v < 0 || u + v > 1)
				continue;
			double tt = fe2::dot(e2, q) * inv;
			if (tt > 0 && tt < best)
			{
				best = tt;
				hit = f.id;
			}
		}
	}
	return hit;
}

int pick_vertex(const Camera &cam, float sx, float sy, float w, float h, const std::vector<Vec3> &verts,
				const std::set<int> &candidates, float radius)
{
	int best = -1;
	float bd = radius;
	for (int i : candidates)
	{
		float x, y;
		if (!cam.project(verts[i], w, h, x, y))
			continue;
		float d = std::hypot(x - sx, y - sy);
		if (d < bd)
		{
			bd = d;
			best = i;
		}
	}
	return best;
}

} // namespace studio
