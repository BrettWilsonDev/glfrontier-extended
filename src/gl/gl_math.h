/*
 * gl_math.h - tiny column-major 4x4 matrix helpers (OpenGL convention).
 */
#ifndef GL_MATH_H
#define GL_MATH_H

#include <math.h>
#include <string.h>

#define GLM_PI      3.14159265358979323846f
#define GLM_RAD2DEG 57.295779513082323f

typedef struct
{
	float m[16];
} mat4;

static inline mat4 mat4_identity(void)
{
	mat4 r;
	memset(&r, 0, sizeof(r));
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

static inline mat4 mat4_mul(const mat4 *a, const mat4 *b)
{
	mat4 r;
	for (int col = 0; col < 4; col++)
	{
		const float *bc = &b->m[col * 4];
		for (int row = 0; row < 4; row++)
			r.m[col * 4 + row] =
				a->m[row] * bc[0] + a->m[4 + row] * bc[1] + a->m[8 + row] * bc[2] + a->m[12 + row] * bc[3];
	}
	return r;
}

static inline mat4 mat4_translate(float x, float y, float z)
{
	mat4 r = mat4_identity();
	r.m[12] = x;
	r.m[13] = y;
	r.m[14] = z;
	return r;
}

static inline mat4 mat4_scale(float x, float y, float z)
{
	mat4 r = mat4_identity();
	r.m[0] = x;
	r.m[5] = y;
	r.m[10] = z;
	return r;
}

/* Rotation around an arbitrary axis, angle in degrees */
static inline mat4 mat4_rotate(float deg, float ax, float ay, float az)
{
	float rad = deg * GLM_PI / 180.0f;
	float c = cosf(rad), s = sinf(rad), ic = 1.0f - c;
	float l = sqrtf(ax * ax + ay * ay + az * az);
	if (l > 0.0001f)
	{
		ax /= l;
		ay /= l;
		az /= l;
	}
	mat4 r = mat4_identity();
	r.m[0] = c + ax * ax * ic;
	r.m[1] = ay * ax * ic + az * s;
	r.m[2] = az * ax * ic - ay * s;
	r.m[4] = ax * ay * ic - az * s;
	r.m[5] = c + ay * ay * ic;
	r.m[6] = az * ay * ic + ax * s;
	r.m[8] = ax * az * ic + ay * s;
	r.m[9] = ay * az * ic - ax * s;
	r.m[10] = c + az * az * ic;
	return r;
}

static inline mat4 mat4_ortho(float l, float r, float b, float t, float n, float f)
{
	mat4 m = mat4_identity();
	m.m[0] = 2.0f / (r - l);
	m.m[5] = 2.0f / (t - b);
	m.m[10] = -2.0f / (f - n);
	m.m[12] = -(r + l) / (r - l);
	m.m[13] = -(t + b) / (t - b);
	m.m[14] = -(f + n) / (f - n);
	return m;
}

static inline mat4 mat4_perspective(float fov_deg, float aspect, float znear, float zfar)
{
	float f = 1.0f / tanf(fov_deg * GLM_PI / 360.0f);
	mat4 m;
	memset(&m, 0, sizeof(m));
	m.m[0] = f / aspect;
	m.m[5] = f;
	m.m[10] = (zfar + znear) / (znear - zfar);
	m.m[11] = -1.0f;
	m.m[14] = 2.0f * zfar * znear / (znear - zfar);
	return m;
}

/* out = M * (x, y, z, 1) */
static inline void mat4_xform(const mat4 *M, float x, float y, float z, float out[4])
{
	const float *m = M->m;
	out[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
	out[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
	out[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
	out[3] = m[3] * x + m[7] * y + m[11] * z + m[15];
}

/* out = upper-left 3x3 of M * v (directions / normals, no translation) */
static inline void mat4_xform_dir(const mat4 *M, const float v[3], float out[3])
{
	const float *m = M->m;
	out[0] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2];
	out[1] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2];
	out[2] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2];
}

/* Upper-left 3x3 as a column-major mat3 (normal matrix for rigid transforms) */
static inline void mat4_upper3(const mat4 *M, float out[9])
{
	for (int c = 0; c < 3; c++)
		for (int r = 0; r < 3; r++)
			out[c * 3 + r] = M->m[c * 4 + r];
}

static inline void vec3_normalize(float v[3])
{
	float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (l > 1e-20f)
	{
		v[0] /= l;
		v[1] /= l;
		v[2] /= l;
	}
}

#endif /* GL_MATH_H */
