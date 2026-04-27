#ifdef WITH_GL

/*
 * rendererGL.c - OpenGL 3.3 Core Profile renderer for Frontier: Elite 2
 *
 * Converted from legacy OpenGL 1.x fixed-function pipeline to OpenGL 3.3
 */

#include <SDL.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(__EMSCRIPTEN__) || defined(ANDROID)
#include <GLES3/gl3.h>
#else
#include "glad/glad.h"
#endif

#include "glutess.h"

#include "main.h"
#include "../m68000.h"
#include "renderer.h"
#include "render_nuklear.h"
#include "nuklear_impl.h"
#include "nuklear_sdl_gl3_impl.h"
#include "touch_input.h"

/* =========================================================================
 * Public globals (declared extern in renderer.h)
 * ========================================================================= */
unsigned long VideoBase;
unsigned char *VideoRaster;

int len_main_palette;
unsigned short MainPalette[256];
unsigned short CtrlPalette[16];
int fe2_bgcol;

unsigned int MainRGBPalette[256];
unsigned int CtrlRGBPalette[16];

unsigned long logscreen, logscreen2, physcreen, physcreen2;

SDL_Surface *sdlscrn;
BOOL bGrabMouse = FALSE;
BOOL bInFullScreen = FALSE;

enum RENDERERS use_renderer = R_GL;
int mouse_shown = 0;
float hack;

int screen_w = 640;
int screen_h = 480;

/* =========================================================================
 * Internal constants
 * ========================================================================= */
#define SCR_TEX_W 512
#define SCR_TEX_H 256
#define RAD_2_DEG 57.295779513082323f
#define CALLBACK /* nothing on non-Win32 */

/* =========================================================================
 * Simple 4x4 matrix math (column-major, matching OpenGL convention)
 * ========================================================================= */
typedef struct
{
	float m[16];
} mat4;

static mat4 mat4_identity(void)
{
	mat4 r = {0};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

static mat4 mat4_mul(const mat4 *a, const mat4 *b)
{
	mat4 r = {0};
	for (int col = 0; col < 4; col++)
		for (int row = 0; row < 4; row++)
			for (int k = 0; k < 4; k++)
				r.m[col * 4 + row] += a->m[k * 4 + row] * b->m[col * 4 + k];
	return r;
}

static mat4 mat4_translate(float x, float y, float z)
{
	mat4 r = mat4_identity();
	r.m[12] = x;
	r.m[13] = y;
	r.m[14] = z;
	return r;
}

static mat4 mat4_scale(float x, float y, float z)
{
	mat4 r = mat4_identity();
	r.m[0] = x;
	r.m[5] = y;
	r.m[10] = z;
	return r;
}

/* Rotation around an arbitrary axis (angle in degrees) */
static mat4 mat4_rotate(float deg, float ax, float ay, float az)
{
	float rad = deg * (float)M_PI / 180.0f;
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

static mat4 mat4_ortho(float l, float r, float b, float t, float n, float f)
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

static mat4 mat4_perspective(float fov_deg, float aspect, float near, float far)
{
	float f = 1.0f / tanf(fov_deg * (float)M_PI / 360.0f);
	mat4 m = {0};
	m.m[0] = f / aspect;
	m.m[5] = f;
	m.m[10] = (far + near) / (near - far);
	m.m[11] = -1.0f;
	m.m[14] = 2.0f * far * near / (near - far);
	return m;
}

/* Matrix stack (replaces glPushMatrix/glPopMatrix) */
#define MATRIX_STACK_DEPTH 32
static mat4 mv_stack[MATRIX_STACK_DEPTH];
static int mv_sp = 0; /* stack pointer - points to current top */

static mat4 proj_matrix;
static mat4 *mv = &mv_stack[0]; /* convenience pointer */

static void mat_push(void)
{
	assert(mv_sp < MATRIX_STACK_DEPTH - 1);
	mv_stack[mv_sp + 1] = mv_stack[mv_sp];
	mv_sp++;
	mv = &mv_stack[mv_sp];
}
static void mat_pop(void)
{
	assert(mv_sp > 0);
	mv_sp--;
	mv = &mv_stack[mv_sp];
}
static void mat_load_identity(void) { *mv = mat4_identity(); }

static void mat_translate(float x, float y, float z)
{
	mat4 t = mat4_translate(x, y, z);
	*mv = mat4_mul(mv, &t);
}
static void mat_scale(float x, float y, float z)
{
	mat4 s = mat4_scale(x, y, z);
	*mv = mat4_mul(mv, &s);
}
static void mat_rotate_deg(float deg, float ax, float ay, float az)
{
	mat4 r = mat4_rotate(deg, ax, ay, az);
	*mv = mat4_mul(mv, &r);
}
static void mat_mult(const float *col_major_16)
{
	mat4 tmp;
	memcpy(tmp.m, col_major_16, 64);
	*mv = mat4_mul(mv, &tmp);
}

static float cur_point_size = 1.0f;
void set_point_size(float size)
{
	cur_point_size = size;
}

/* =========================================================================
 * Shader sources
 * ========================================================================= */

#if defined(__EMSCRIPTEN__) || defined(ANDROID)
/*
 * GLES 3.0 shaders.
 *
 * KEY RULES that bite on Android:
 *  - NO layout(location=N) on inputs — many Mali/Adreno drivers ignore it.
 *    We use glBindAttribLocation() before linking instead (see init_shaders).
 *  - NO uniform bool — broken on a large swath of Android drivers (renders
 *    as if prog=0, producing the rainbow gradient).  Use uniform int.
 *  - highp for position/matrix math — mediump has only ~3 decimal digits of
 *    fractional precision; star-field coords are in the millions.
 */

/* ----- flat-colour vertex shader ----- */
static const char *VERT_FLAT_SRC =
	"#version 300 es\n"
	"precision highp float;\n"
	"in vec3 aPos;\n"
	"in vec3 aColor;\n"
	"uniform mat4 uMVP;\n"
	"uniform float uPointSize;\n"
	"out vec3 vColor;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	"  gl_PointSize = uPointSize;\n"
	"  vColor = aColor;\n"
	"}\n";

/* ----- flat-colour fragment shader ----- */
static const char *FRAG_FLAT_SRC =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec3 vColor;\n"
	"out vec4 fragColor;\n"
	"void main(){\n"
	"  fragColor = vec4(vColor, 1.0);\n"
	"}\n";

/* ----- lit vertex shader ----- */
static const char *VERT_LIT_SRC =
	"#version 300 es\n"
	"precision highp float;\n"
	"in vec3 aPos;\n"
	"in vec3 aNormal;\n"
	"uniform mat4 uMV;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat3 uNormalMat;\n"
	"out vec3 vNormalEye;\n"
	"out vec3 vPosEye;\n"
	"void main(){\n"
	"  vec4 posEye = uMV * vec4(aPos, 1.0);\n"
	"  vPosEye    = posEye.xyz;\n"
	"  vNormalEye = normalize(uNormalMat * aNormal);\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	"}\n";

/* ----- lit fragment shader ----- */
static const char *FRAG_LIT_SRC =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec3 vNormalEye;\n"
	"in vec3 vPosEye;\n"
	"uniform vec3 uLightDir;\n"
	"uniform vec3 uLightDiffuse;\n"
	"uniform vec3 uAmbient;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  float diff = max(dot(normalize(vNormalEye), normalize(uLightDir)), 0.0);\n"
	"  vec3 col   = uAmbient + diff * uLightDiffuse;\n"
	"  FragColor  = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
	"}\n";

/* ----- textured quad vertex shader ----- */
static const char *VERT_TEX_SRC =
	"#version 300 es\n"
	"precision highp float;\n"
	"in vec2 aPos;\n"
	"in vec2 aUV;\n"
	"uniform mat4 uMVP;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 0.0, 1.0);\n"
	"  vUV = aUV;\n"
	"}\n";

/* ----- textured quad fragment shader ----- */
/* NOTE: uniform int uBlendMode, NOT bool — Android drivers silently mis-compile
 * uniform bool, causing the program to behave as if prog=0 (rainbow screen). */
static const char *FRAG_TEX_SRC =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uBlendMode;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(uTex, vUV);\n"
	"  if (uBlendMode != 0 && c.a < 0.01) discard;\n"
	"  FragColor = c;\n"
	"}\n";
#else

// /* ----- flat-colour vertex shader ----- */
static const char *VERT_FLAT_SRC =
	"#version 330 core\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec3 aColor;\n"
	"uniform mat4 uMVP;\n"
	"uniform float uPointSize;\n" // Add this uniform
	"out vec3 vColor;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	"  gl_PointSize = uPointSize;\n" // Use the uniform here
	"  vColor = aColor;\n"
	"}\n";

/* ----- flat-colour fragment shader ----- */
static const char *FRAG_FLAT_SRC =
	"#version 330 core\n"
	"in  vec3 vColor;\n"
	"out vec4 FragColor;\n"
	"void main(){ FragColor = vec4(vColor, 1.0); }\n";

/* ----- lit vertex shader (for planet / cylinder) ----- */
static const char *VERT_LIT_SRC =
	"#version 330 core\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec3 aNormal;\n"
	"uniform mat4 uMV;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat3 uNormalMat;\n"
	"out vec3 vNormalEye;\n"
	"out vec3 vPosEye;\n"
	"void main(){\n"
	"  vec4 posEye = uMV * vec4(aPos, 1.0);\n"
	"  vPosEye    = posEye.xyz;\n"
	"  vNormalEye = normalize(uNormalMat * aNormal);\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	"}\n";

/* ----- lit fragment shader ----- */
static const char *FRAG_LIT_SRC =
	"#version 330 core\n"
	"in  vec3 vNormalEye;\n"
	"in  vec3 vPosEye;\n"
	"uniform vec3  uLightDir;      /* world-space, pre-normalised */\n"
	"uniform vec3  uLightDiffuse;\n"
	"uniform vec3  uAmbient;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  float diff = max(dot(normalize(vNormalEye), normalize(uLightDir)), 0.0);\n"
	"  vec3 col   = uAmbient + diff * uLightDiffuse;\n"
	"  FragColor  = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
	"}\n";

/* ----- textured quad vertex shader (2D UI blit) ----- */
static const char *VERT_TEX_SRC =
	"#version 330 core\n"
	"layout(location=0) in vec2 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"uniform mat4 uMVP;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 0.0, 1.0);\n"
	"  vUV = aUV;\n"
	"}\n";

/* ----- textured quad fragment shader ----- */
static const char *FRAG_TEX_SRC =
	"#version 330 core\n"
	"in  vec2 vUV;\n"
	"uniform sampler2D uTex;\n"
	"uniform bool      uBlend;\n"
	"out vec4 FragColor;\n"
	"void main(){\n"
	"  vec4 c = texture(uTex, vUV);\n"
	"  if (uBlend && c.a < 0.01) discard;\n"
	"  FragColor = c;\n"
	"}\n";
#endif

/* =========================================================================
 * Shader / program helpers
 * ========================================================================= */

/* Compile one shader stage.  Dumps the info-log unconditionally on Android
 * (even on success) so we can see driver warnings in logcat.  On other
 * platforms only logs on failure to avoid spam. */
static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);

	GLint ok = GL_FALSE;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

	GLint logLen = 0;
	glGetShaderiv(s, GL_INFO_LOG_LENGTH, &logLen);

#if defined(ANDROID) || defined(__EMSCRIPTEN__)
	if (logLen > 1)
	{
		char *buf = (char *)malloc(logLen + 1);
		if (buf)
		{
			glGetShaderInfoLog(s, logLen, NULL, buf);
			log_printf("GLSL %s log:\n%s",
					   (type == GL_VERTEX_SHADER) ? "VERT" : "FRAG", buf);
			free(buf);
		}
	}
	if (!ok)
		log_printf("GLSL compile FAILED (type=%d)", (int)type);
#else
	if (!ok && logLen > 1)
	{
		char buf[512];
		glGetShaderInfoLog(s, 512, NULL, buf);
		log_printf("Shader compile error: %s\n", buf);
	}
#endif
	return s;
}

/* Link a program.
 * attrib_names / attrib_locs: parallel arrays of length attrib_count.
 * Calling glBindAttribLocation BEFORE glLinkProgram is the only portable
 * way to fix attrib locations on Android — layout(location=N) in GLSL is
 * unreliable on many Mali and Adreno drivers. */
static GLuint link_program_ex(const char *vsrc, const char *fsrc,
							  const char **attrib_names,
							  const GLuint *attrib_locs,
							  int attrib_count)
{
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);

	/* Force attribute locations before linking */
	for (int i = 0; i < attrib_count; i++)
		glBindAttribLocation(prog, attrib_locs[i], attrib_names[i]);

	glLinkProgram(prog);

	GLint ok = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);

	GLint logLen = 0;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);

#if defined(ANDROID) || defined(__EMSCRIPTEN__)
	if (logLen > 1)
	{
		char *buf = (char *)malloc(logLen + 1);
		if (buf)
		{
			glGetProgramInfoLog(prog, logLen, NULL, buf);
			log_printf("GLSL link log:\n%s", buf);
			free(buf);
		}
	}
	if (!ok)
		log_printf("GLSL link FAILED");
#else
	if (!ok && logLen > 1)
	{
		char buf[512];
		glGetProgramInfoLog(prog, 512, NULL, buf);
		log_printf("Program link error: %s\n", buf);
	}
#endif

	/* Verify locations actually stuck (catches silent driver mis-linking) */
	for (int i = 0; i < attrib_count; i++)
	{
		GLint got = glGetAttribLocation(prog, attrib_names[i]);
		if (got != (GLint)attrib_locs[i])
		{
#if defined(ANDROID) || defined(__EMSCRIPTEN__)
			log_printf("ATTRIB LOC MISMATCH: '%s' wanted %d got %d",
					   attrib_names[i], attrib_locs[i], got);
#else
			log_printf("ATTRIB LOC MISMATCH: '%s' wanted %d got %d\n",
					   attrib_names[i], attrib_locs[i], got);
#endif
		}
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	return prog;
}

/* Convenience wrapper for the common 2-attrib case */
static GLuint link_program(const char *vsrc, const char *fsrc)
{
	/* No named attribs needed for desktop — layout(location) works fine */
	return link_program_ex(vsrc, fsrc, NULL, NULL, 0);
}

/* =========================================================================
 * Shader programs & cached uniform locations
 * ========================================================================= */
static GLuint prog_flat = 0; /* coloured geometry            */
static GLuint prog_lit = 0;	 /* Phong-lit geometry           */
static GLuint prog_tex = 0;	 /* 2-D textured quad            */

static struct
{
	GLint mvp, mv, normalMat;
	GLint lightDir, lightDiff, ambient;
} u_lit;
static struct
{
	GLint mvp;
	GLint pointSize;
} u_flat;
static struct
{
	GLint mvp, tex, blend;
} u_tex;

static void init_shaders(void)
{
#if defined(__EMSCRIPTEN__) || defined(ANDROID)
	/* On GLES, layout(location=N) is unreliable.  We strip those qualifiers
	 * from the GLES shaders above and instead force locations here. */
	const char *flat_attrs[] = {"aPos", "aColor"};
	const GLuint flat_locs[] = {0, 1};
	const char *lit_attrs[] = {"aPos", "aNormal"};
	const GLuint lit_locs[] = {0, 1};
	const char *tex_attrs[] = {"aPos", "aUV"};
	const GLuint tex_locs[] = {0, 1};

	prog_flat = link_program_ex(VERT_FLAT_SRC, FRAG_FLAT_SRC, flat_attrs, flat_locs, 2);
	prog_lit = link_program_ex(VERT_LIT_SRC, FRAG_LIT_SRC, lit_attrs, lit_locs, 2);
	prog_tex = link_program_ex(VERT_TEX_SRC, FRAG_TEX_SRC, tex_attrs, tex_locs, 2);
#else
	prog_flat = link_program(VERT_FLAT_SRC, FRAG_FLAT_SRC);
	prog_lit = link_program(VERT_LIT_SRC, FRAG_LIT_SRC);
	prog_tex = link_program(VERT_TEX_SRC, FRAG_TEX_SRC);
#endif

	/* flat */
	u_flat.mvp = glGetUniformLocation(prog_flat, "uMVP");
	u_flat.pointSize = glGetUniformLocation(prog_flat, "uPointSize");

	/* lit */
	u_lit.mvp = glGetUniformLocation(prog_lit, "uMVP");
	u_lit.mv = glGetUniformLocation(prog_lit, "uMV");
	u_lit.normalMat = glGetUniformLocation(prog_lit, "uNormalMat");
	u_lit.lightDir = glGetUniformLocation(prog_lit, "uLightDir");
	u_lit.lightDiff = glGetUniformLocation(prog_lit, "uLightDiffuse");
	u_lit.ambient = glGetUniformLocation(prog_lit, "uAmbient");

	/* tex */
	u_tex.mvp = glGetUniformLocation(prog_tex, "uMVP");
	u_tex.tex = glGetUniformLocation(prog_tex, "uTex");
	u_tex.blend = glGetUniformLocation(prog_tex, "uBlendMode");

	/* Startup diagnostics — any -1 means the uniform was stripped/renamed */
	// log_printf("GL: vendor=%s renderer=%s\n",
	//            glGetString(GL_VENDOR), glGetString(GL_RENDERER));
	// log_printf("GL: version=%s glsl=%s\n",
	//            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
	// log_printf("Programs: flat=%u lit=%u tex=%u\n", prog_flat, prog_lit, prog_tex);
	// log_printf("Uniforms flat: mvp=%d ps=%d\n",          u_flat.mvp, u_flat.pointSize);
	// log_printf("Uniforms lit:  mvp=%d mv=%d nm=%d ld=%d diff=%d amb=%d\n",
	//            u_lit.mvp, u_lit.mv, u_lit.normalMat,
	//            u_lit.lightDir, u_lit.lightDiff, u_lit.ambient);
	// log_printf("Uniforms tex:  mvp=%d tex=%d blend=%d\n",
	//            u_tex.mvp, u_tex.tex, u_tex.blend);
#if defined(ANDROID) || defined(__EMSCRIPTEN__)
	// log_printf("GL: vendor=%s renderer=%s version=%s",
	//         glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
	// log_printf("Programs: flat=%u lit=%u tex=%u", prog_flat, prog_lit, prog_tex);
	// log_printf("Uniforms flat: mvp=%d ps=%d", u_flat.mvp, u_flat.pointSize);
	// log_printf("Uniforms lit:  mvp=%d mv=%d nm=%d ld=%d diff=%d amb=%d",
	//         u_lit.mvp, u_lit.mv, u_lit.normalMat,
	//         u_lit.lightDir, u_lit.lightDiff, u_lit.ambient);
	// log_printf("Uniforms tex:  mvp=%d tex=%d blend=%d",
	//         u_tex.mvp, u_tex.tex, u_tex.blend);
#endif
}

/* =========================================================================
 * Dynamic vertex batch  (replaces glBegin/glVertex/glEnd)
 *
 * Usage:
 *   batch_begin(GL_TRIANGLES);
 *   batch_vertex3f(x, y, z, r, g, b);
 *   batch_end_flat();        -- draws with prog_flat, current MVP
 * ========================================================================= */
#define BATCH_MAX_VERTS 65536

typedef struct
{
	float x, y, z, r, g, b;
} FlatVert;
typedef struct
{
	float x, y, z, nx, ny, nz;
} LitVert;

static FlatVert flat_buf[BATCH_MAX_VERTS];
static LitVert lit_buf[BATCH_MAX_VERTS];
static int flat_count = 0;
static int lit_count = 0;
static GLenum batch_prim = GL_TRIANGLES;

/* Shared VAOs / VBOs - created once */
static GLuint vao_flat, vbo_flat;
static GLuint vao_lit, vbo_lit;

static void init_batch_buffers(void)
{
	/* flat */
	glGenVertexArrays(1, &vao_flat);
	glGenBuffers(1, &vbo_flat);
	glBindVertexArray(vao_flat);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_flat);
	glBufferData(GL_ARRAY_BUFFER, sizeof(flat_buf), NULL, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FlatVert), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FlatVert), (void *)(3 * sizeof(float)));
	glBindVertexArray(0);

	/* lit */
	glGenVertexArrays(1, &vao_lit);
	glGenBuffers(1, &vbo_lit);
	glBindVertexArray(vao_lit);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_lit);
	glBufferData(GL_ARRAY_BUFFER, sizeof(lit_buf), NULL, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LitVert), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LitVert), (void *)(3 * sizeof(float)));
	glBindVertexArray(0);
}

/* Current draw colour (replaces glColor3*) */
static float cur_r = 1, cur_g = 1, cur_b = 1;

static void set_color3ub(unsigned char r, unsigned char g, unsigned char b)
{
	cur_r = r / 255.0f;
	cur_g = g / 255.0f;
	cur_b = b / 255.0f;
}
static void set_color3f(float r, float g, float b)
{
	cur_r = r;
	cur_g = g;
	cur_b = b;
}

static void batch_begin(GLenum prim)
{
	batch_prim = prim;
	flat_count = 0;
}

static void batch_vertex3f(float x, float y, float z)
{
	if (flat_count >= BATCH_MAX_VERTS)
	{
		log_printf("BATCH OVERFLOW: flat_count=%d\n", flat_count);
		return;
	}
	assert(flat_count < BATCH_MAX_VERTS);
	flat_buf[flat_count++] = (FlatVert){x, y, z, cur_r, cur_g, cur_b};
}
static void batch_vertex3i(int x, int y, int z)
{
	batch_vertex3f((float)x, (float)y, (float)z);
}
static void batch_vertex3d(double x, double y, double z)
{
	batch_vertex3f((float)x, (float)y, (float)z);
}
static void batch_vertex3fv(const float *v) { batch_vertex3f(v[0], v[1], v[2]); }
static void batch_vertex3iv(const int *v) { batch_vertex3i(v[0], v[1], v[2]); }
static void batch_vertex3dv(const double *v) { batch_vertex3d(v[0], v[1], v[2]); }

/* Compute MVP and flush flat batch */
static void batch_end_flat(void)
{
	if (flat_count == 0)
		return;

	mat4 mvp = mat4_mul(&proj_matrix, mv);

	glUseProgram(prog_flat);
	glUniformMatrix4fv(u_flat.mvp, 1, GL_FALSE, mvp.m);
	glUniform1f(u_flat.pointSize, cur_point_size);

	glBindVertexArray(vao_flat);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_flat);
// #ifdef ANDROID
	glBufferData(GL_ARRAY_BUFFER, flat_count * sizeof(FlatVert), NULL, GL_STREAM_DRAW);
// #endif
	glBufferSubData(GL_ARRAY_BUFFER, 0, flat_count * sizeof(FlatVert), flat_buf);
	glDrawArrays(batch_prim, 0, flat_count);
	glBindVertexArray(0);
	flat_count = 0;
}

/* Lit batch helpers */
static float lit_nx = 0, lit_ny = 0, lit_nz = 1;
static void lit_normal3f(float x, float y, float z)
{
	lit_nx = x;
	lit_ny = y;
	lit_nz = z;
}
static void lit_normal3fv(const float *n)
{
	lit_nx = n[0];
	lit_ny = n[1];
	lit_nz = n[2];
}

static void lit_begin(GLenum prim)
{
	batch_prim = prim;
	lit_count = 0;
}
static void lit_vertex3fv(const float *v)
{
	assert(lit_count < BATCH_MAX_VERTS);
	lit_buf[lit_count++] = (LitVert){v[0], v[1], v[2], lit_nx, lit_ny, lit_nz};
}

/* Light state for prog_lit */
static float lit_light_dir[3] = {0, 0, 1};
static float lit_diffuse[3] = {1, 1, 1};
static float lit_ambient[3] = {0.2f, 0.2f, 0.2f};
static bool use_lighting = false;

static void batch_end_lit(void)
{
	if (lit_count == 0)
		return;
	mat4 mvp = mat4_mul(&proj_matrix, mv);

	/* normal matrix = upper-left 3x3 of MV (no non-uniform scale here) */
	float nm[9];
	for (int c = 0; c < 3; c++)
		for (int r2 = 0; r2 < 3; r2++)
			nm[c * 3 + r2] = mv->m[c * 4 + r2];

	glUseProgram(prog_lit);
	glUniformMatrix4fv(u_lit.mvp, 1, GL_FALSE, mvp.m);
	glUniformMatrix4fv(u_lit.mv, 1, GL_FALSE, mv->m);
	glUniformMatrix3fv(u_lit.normalMat, 1, GL_FALSE, nm);
	glUniform3fv(u_lit.lightDir, 1, lit_light_dir);
	glUniform3fv(u_lit.lightDiff, 1, lit_diffuse);
	glUniform3fv(u_lit.ambient, 1, lit_ambient);

	glBindVertexArray(vao_lit);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_lit);
// #ifdef ANDROID
	glBufferData(GL_ARRAY_BUFFER, lit_count * sizeof(LitVert), NULL, GL_STREAM_DRAW);
// #endif
	glBufferSubData(GL_ARRAY_BUFFER, 0, lit_count * sizeof(LitVert), lit_buf);
	glDrawArrays(batch_prim, 0, lit_count);
	glBindVertexArray(0);
	lit_count = 0;
}

/* =========================================================================
 * Textured quad VAO (for the 2-D UI blit)
 * ========================================================================= */
static GLuint vao_tex, vbo_tex;
static GLuint screen_tex;

static void init_tex_quad(void)
{
	glGenVertexArrays(1, &vao_tex);
	glGenBuffers(1, &vbo_tex);
	glBindVertexArray(vao_tex);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
	/* 4 verts, each: x y u v */
	glBufferData(GL_ARRAY_BUFFER, 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
	glBindVertexArray(0);
}

static void draw_tex_quad(float x0, float y0, float x1, float y1,
						  float u0, float v0, float u1, float v1,
						  bool blend)
{
	float verts[4][4] = {
		{x0, y0, u0, v1},
		{x1, y0, u1, v1},
		{x0, y1, u0, v0},
		{x1, y1, u1, v0},
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

	mat4 mvp = mat4_mul(&proj_matrix, mv);
	glUseProgram(prog_tex);
	glUniformMatrix4fv(u_tex.mvp, 1, GL_FALSE, mvp.m);
	glUniform1i(u_tex.tex, 0);
	glUniform1i(u_tex.blend, blend ? 1 : 0);

#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)
	glEnable(GL_PROGRAM_POINT_SIZE);
#endif

	if (blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	glBindVertexArray(vao_tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	if (blend)
		glDisable(GL_BLEND);
}

static inline int gl_project(double objx, double objy, double objz,
							 const double model[16], const double proj[16],
							 const int viewport[4],
							 double *winx, double *winy, double *winz)
{
	double in[4] = {objx, objy, objz, 1.0};
	double mv[4], clip[4];

	for (int row = 0; row < 4; row++)
	{
		mv[row] = model[0 * 4 + row] * in[0] + model[1 * 4 + row] * in[1] +
				  model[2 * 4 + row] * in[2] + model[3 * 4 + row] * in[3];
	}
	for (int row = 0; row < 4; row++)
	{
		clip[row] = proj[0 * 4 + row] * mv[0] + proj[1 * 4 + row] * mv[1] +
					proj[2 * 4 + row] * mv[2] + proj[3 * 4 + row] * mv[3];
	}
	if (clip[3] == 0.0)
		return 0;

	clip[0] /= clip[3];
	clip[1] /= clip[3];
	clip[2] /= clip[3];

	clip[0] = clip[0] * 0.5 + 0.5;
	clip[1] = clip[1] * 0.5 + 0.5;
	clip[2] = clip[2] * 0.5 + 0.5;

	*winx = viewport[0] + clip[0] * viewport[2];
	*winy = viewport[1] + clip[1] * viewport[3];
	*winz = clip[2];
	return 1;
}

/* =========================================================================
 * Letterboxing helpers
 * ========================================================================= */

#define GAME_W 320
#define GAME_H 240

static int lb_x = 0;
static int lb_y = 0;
static int lb_w = 640;
static int lb_h = 480;

static void update_letterbox(void)
{
	float aspect = (float)GAME_W / (float)GAME_H; /* 320/240 = 4/3 */

	lb_w = screen_w;
	lb_h = (int)(screen_w / aspect + 0.5f);

	if (lb_h > screen_h)
	{
		lb_h = screen_h;
		lb_w = (int)(screen_h * aspect + 0.5f);
	}

	lb_x = (screen_w - lb_w) / 2;
	lb_y = (screen_h - lb_h) / 2;
}

void call_update_letterbox(void)
{
	update_letterbox();
	reinit_touch_buttons();
}

void Screen_WindowToGame(int wx, int wy, int *gx, int *gy)
{
	int sdl_top = screen_h - lb_y - lb_h;

	int rx = wx - lb_x;
	int ry = wy - sdl_top;

	*gx = rx * GAME_W / lb_w;
	*gy = ry * GAME_H / lb_h;
}

int Screen_GetGameOffsetX(void) { return lb_x; }
int Screen_GetGameOffsetY(void) { return screen_h - lb_y - lb_h; }
int Screen_GetGameHeight(void) { return lb_h; }
int Screen_GetGameWidth(void) { return lb_w; }

/* Cached viewport — avoids glGetIntegerv (GPU sync stall) inside the
 * tessellator.  Updated whenever glViewport is called for the 3D view. */
static GLint cached_viewport[4];

/* 3D view: top 200/240 of the game area. Panel sits below it. */
static void set_main_viewport(void)
{
	int panel_h = lb_h * 38 / GAME_H; /* 40 virtual lines out of 240 */
	cached_viewport[0] = lb_x;
	cached_viewport[1] = lb_y + panel_h;
	cached_viewport[2] = lb_w;
	cached_viewport[3] = lb_h - panel_h;
	glViewport(cached_viewport[0], cached_viewport[1],
			   cached_viewport[2], cached_viewport[3]);
}

/* Full game area including panel */
static void set_ctrl_viewport(void)
{
	glViewport(lb_x, lb_y, lb_w, lb_h);
}

/* =========================================================================
 * Ortho push / pop  (replaces glMatrixMode + glOrtho stack)
 * ========================================================================= */

/* push_ortho / pop_ortho: panel internal coords stay 0..320, 0..200 */
static mat4 saved_proj;
static mat4 saved_mv;

static void push_ortho(void)
{
	glDisable(GL_DEPTH_TEST);
	saved_proj = proj_matrix;
	saved_mv = *mv;
	proj_matrix = mat4_ortho(0, 320, 0, 200, -1, 1);
	mat_load_identity();
}

static void pop_ortho(void)
{
	proj_matrix = saved_proj;
	*mv = saved_mv;
}

/* =========================================================================
 * SDL / GL context
 * ========================================================================= */
static SDL_Window *window = NULL;
static SDL_GLContext gl_context = NULL;

static void change_vidmode(void)
{
#if defined(__EMSCRIPTEN__) || defined(ANDROID)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

	// Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | (bInFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | SDL_WINDOW_ALLOW_HIGHDPI;
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | (bInFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | SDL_WINDOW_ALLOW_HIGHDPI;

#ifdef ANDROID
	// /* Lock to landscape — Android will rotate the surface automatically */
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
	// /* Hide the system status/nav bars for a true full-screen game feel */
	// SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

	flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | (bInFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN;
#endif

	window = SDL_CreateWindow("Frontier: Elite 2",
							  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
							  screen_w, screen_h, flags);

	if (!window)
	{
		log_printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		exit(-1);
	}

	gl_context = SDL_GL_CreateContext(window);
	if (!gl_context)
	{
		log_printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		exit(-1);
	}

#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)
	if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
	{
		log_printf("Failed to initialize GLAD\n");
		exit(1);
	}
#endif

	/* Core-profile state */
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glClearColor(0, 0, 0, 0);

	/* Perspective matrix (replaces gluPerspective) */
	proj_matrix = mat4_perspective(36.5f, 1.9f, 1.0f, 10000000000.0f);
	// proj_matrix = mat4_perspective(36.5f, 1.9f, 1.0f, 1000000000000000000.0f);
	mat_load_identity();

	/* Screen texture */
	glGenTextures(1, &screen_tex);
	glBindTexture(GL_TEXTURE_2D, screen_tex);
	static unsigned char zero_tex[SCR_TEX_W * SCR_TEX_H * 4] = {0};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_TEX_W, SCR_TEX_H, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, zero_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/* =========================================================================
 * GLU Tessellator (glutess is still used for complex polygon tessellation;
 * the callbacks now push into the flat batch instead of calling glBegin/glEnd)
 * ========================================================================= */
static GLUtesselator *tobj;

void CALLBACK beginCallback(GLenum which)
{
	/* start a new batch of the given primitive type */
	batch_begin(which);
}
void CALLBACK errorCallback(GLenum errorCode)
{
	(void)errorCode; /* silently ignore */
}
void CALLBACK endCallback(void)
{
	batch_end_flat();
}

static int complex_col[3]; /* r,g,b 0-255 */

void CALLBACK vertexCallback(GLvoid *vertex, GLvoid *poly_data)
{
	(void)poly_data;
	const GLdouble *p = (const GLdouble *)vertex;
	set_color3ub(complex_col[0], complex_col[1], complex_col[2]);
	batch_vertex3d(p[0], p[1], p[2]);
}
// void CALLBACK combineCallback(GLdouble coords[3],
// 							  GLdouble *vertex_data[4],
// 							  GLfloat weight[4], GLdouble **dataOut)
// {
// 	(void)vertex_data;
// 	(void)weight;
// 	GLdouble *vertex = (GLdouble *)malloc(3 * sizeof(GLdouble));
// 	vertex[0] = coords[0];
// 	vertex[1] = coords[1];
// 	vertex[2] = coords[2];
// 	*dataOut = vertex;
// }

// Add near the top of the file with other statics
#define TESS_ARENA_SIZE (4096 * 24) // 4096 combine verts max per frame
static GLdouble tess_arena[TESS_ARENA_SIZE / sizeof(GLdouble)];
static int tess_arena_pos = 0;

void CALLBACK combineCallback(GLdouble coords[3],
							  GLdouble *vertex_data[4],
							  GLfloat weight[4], GLdouble **dataOut)
{
	(void)vertex_data;
	(void)weight;
	// bump-allocate from arena instead of malloc
	if (tess_arena_pos + 3 > (int)(TESS_ARENA_SIZE / sizeof(GLdouble)))
	{
		tess_arena_pos = 0; // wrap rather than crash
	}
	GLdouble *vertex = &tess_arena[tess_arena_pos];
	tess_arena_pos += 3;
	vertex[0] = coords[0];
	vertex[1] = coords[1];
	vertex[2] = coords[2];
	*dataOut = vertex;
}

/* =========================================================================
 * Screen_Init / UnInit / ToggleFullScreen / ToggleRenderer
 * ========================================================================= */
void Screen_Init(void)
{
	change_vidmode();
	init_shaders();
	init_batch_buffers();
	init_tex_quad();

	tobj = gluNewTess();
	gluTessCallback(tobj, GLU_TESS_VERTEX_DATA, (_GLUfuncptr)vertexCallback);
	gluTessCallback(tobj, GLU_TESS_BEGIN, (_GLUfuncptr)beginCallback);
	gluTessCallback(tobj, GLU_TESS_END, (_GLUfuncptr)endCallback);
	gluTessCallback(tobj, GLU_TESS_ERROR, (_GLUfuncptr)errorCallback);
	gluTessCallback(tobj, GLU_TESS_COMBINE, (_GLUfuncptr)combineCallback);

	SDL_SetWindowTitle(window, "Frontier: Elite 2");
	SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
	SDL_ShowCursor(SDL_ENABLE);
#ifdef USE_NK
	nuklear_init_sdl(window);
#endif
}

void Screen_UnInit(void)
{
	glDeleteVertexArrays(1, &vao_flat);
	glDeleteBuffers(1, &vbo_flat);
	glDeleteVertexArrays(1, &vao_lit);
	glDeleteBuffers(1, &vbo_lit);
	glDeleteVertexArrays(1, &vao_tex);
	glDeleteBuffers(1, &vbo_tex);
	glDeleteProgram(prog_flat);
	glDeleteProgram(prog_lit);
	glDeleteProgram(prog_tex);
	glDeleteTextures(1, &screen_tex);
}

void Screen_ToggleFullScreen(void)
{
	bInFullScreen = !bInFullScreen;
	Uint32 flag = bInFullScreen ? SDL_WINDOW_FULLSCREEN : 0;
	if (SDL_SetWindowFullscreen(window, flag) != 0)
	{
		log_printf("SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
}

void Screen_ToggleRenderer(void)
{
	use_renderer++;
	if (use_renderer >= R_MAX)
		use_renderer = 0;
}

/* =========================================================================
 * Bitmap font & DrawStr  (unchanged — writes to ST framebuffer in RAM)
 * ========================================================================= */
static const unsigned char font_bmp[] = {
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0,
	0x80, 0x0, 0x0, 0x2, 0xa0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x50,
	0xf8, 0x50, 0x50, 0xf8, 0x50, 0x0, 0x0, 0x6, 0x20, 0xf0, 0xa0, 0xa0, 0xa0, 0xa0, 0xf0, 0x20,
	0x0, 0x5, 0x0, 0xc8, 0xd8, 0x30, 0x60, 0xd8, 0x98, 0x0, 0x0, 0x6, 0xa0, 0x0, 0xe0, 0xa0,
	0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2,
	0xc0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xc0, 0x0, 0x3, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40,
	0x40, 0xc0, 0x0, 0x3, 0x0, 0x0, 0x20, 0xf8, 0x50, 0xf8, 0x20, 0x0, 0x0, 0x6, 0x0, 0x0,
	0x40, 0xe0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x80,
	0x0, 0x2, 0x0, 0x0, 0x0, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0,
	0x0, 0x0, 0x80, 0x0, 0x0, 0x2, 0x0, 0x8, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x0, 0x0, 0x6,
	0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x40, 0xc0, 0x40, 0x40, 0x40, 0x40,
	0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0xe0, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20,
	0x20, 0xe0, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0x80, 0x80, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0x0,
	0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0,
	0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x0, 0x0, 0x4,
	0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0xa0, 0xa0, 0xe0, 0x20, 0x20,
	0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x80, 0x0, 0x80, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0,
	0x0, 0x80, 0x0, 0x0, 0x80, 0x80, 0x0, 0x2, 0xe0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0x0,
	0x0, 0x4, 0x0, 0x0, 0xe0, 0x0, 0xe0, 0x0, 0x0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xe0, 0xa0,
	0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x20, 0x20, 0xe0, 0x80, 0x0, 0x80, 0x0, 0x0, 0x4,
	0xfe, 0x82, 0xba, 0xa2, 0xba, 0x82, 0xfe, 0x0, 0x0, 0x8, 0xf0, 0x90, 0x90, 0x90, 0xf0, 0x90,
	0x90, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0xf8, 0x88, 0x88, 0xf8, 0x0, 0x0, 0x6, 0xe0, 0x80,
	0x80, 0x80, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xf8, 0x48, 0x48, 0x48, 0x48, 0x48, 0xf8, 0x0,
	0x0, 0x6, 0xf0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0xf0, 0x0, 0x0, 0x5, 0xf0, 0x80, 0x80, 0xe0,
	0x80, 0x80, 0x80, 0x0, 0x0, 0x4, 0xf0, 0x80, 0x80, 0x80, 0xb0, 0x90, 0xf0, 0x0, 0x0, 0x5,
	0x90, 0x90, 0x90, 0xf0, 0x90, 0x90, 0x90, 0x0, 0x0, 0x5, 0xe0, 0x40, 0x40, 0x40, 0x40, 0x40,
	0xe0, 0x0, 0x0, 0x4, 0xf0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xe0, 0x0, 0x0, 0x4, 0x90, 0xb0,
	0xe0, 0xc0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xe0, 0x0,
	0x0, 0x4, 0x88, 0xd8, 0xf8, 0xa8, 0x88, 0x88, 0x88, 0x0, 0x0, 0x6, 0x90, 0xd0, 0xf0, 0xb0,
	0x90, 0x90, 0x90, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0x90, 0x90, 0x90, 0xf0, 0x0, 0x0, 0x5,
	0xf0, 0x90, 0x90, 0xf0, 0x80, 0x80, 0x80, 0x0, 0x0, 0x5, 0xf0, 0x90, 0x90, 0x90, 0x90, 0xb0,
	0xf0, 0x18, 0x0, 0x5, 0xf0, 0x90, 0x90, 0xf0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0xf0, 0x80,
	0x80, 0xf0, 0x10, 0x10, 0xf0, 0x0, 0x0, 0x5, 0xe0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x0,
	0x0, 0x3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xf0, 0x0, 0x0, 0x5, 0x90, 0x90, 0x90, 0xb0,
	0xe0, 0xc0, 0x80, 0x0, 0x0, 0x5, 0x88, 0x88, 0x88, 0xa8, 0xf8, 0xd8, 0x88, 0x0, 0x0, 0x6,
	0x88, 0xd8, 0x70, 0x20, 0x70, 0xd8, 0x88, 0x0, 0x0, 0x6, 0x90, 0x90, 0x90, 0xf0, 0x20, 0x20,
	0x20, 0x0, 0x0, 0x5, 0xf0, 0x10, 0x30, 0x60, 0xc0, 0x80, 0xf0, 0x0, 0x0, 0x5, 0xa0, 0x0,
	0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x80, 0xc0, 0x60, 0x30, 0x18, 0x8, 0x0,
	0x0, 0x6, 0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0, 0x80, 0x80, 0x4, 0xe0, 0xa0, 0xe0, 0x0,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x0, 0x6,
	0xa0, 0x0, 0xe0, 0x20, 0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xe0, 0x20, 0xe0, 0xa0,
	0xe0, 0x0, 0x0, 0x4, 0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0,
	0xc0, 0x80, 0x80, 0x80, 0xc0, 0x0, 0x0, 0x3, 0x20, 0x20, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0,
	0x0, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0x80, 0x80, 0xc0,
	0x80, 0x80, 0x80, 0x0, 0x0, 0x3, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x20, 0xe0, 0x4,
	0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0x80, 0x0, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x0, 0x0, 0x2, 0x40, 0x0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xc0, 0x0, 0x3, 0x80, 0x80,
	0xb0, 0xe0, 0xe0, 0xb0, 0x90, 0x0, 0x0, 0x5, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x0,
	0x0, 0x2, 0x0, 0x0, 0xf8, 0xa8, 0xa8, 0xa8, 0xa8, 0x0, 0x0, 0x6, 0x0, 0x0, 0xe0, 0xa0,
	0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4,
	0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x80, 0x80, 0x4, 0x0, 0x0, 0xe0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x20, 0x30, 0x4, 0x0, 0x0, 0xc0, 0x80, 0x80, 0x80, 0x80, 0x0, 0x0, 0x3, 0x0, 0x0,
	0xc0, 0x80, 0xc0, 0x40, 0xc0, 0x0, 0x0, 0x3, 0x80, 0x80, 0xc0, 0x80, 0x80, 0x80, 0xc0, 0x0,
	0x0, 0x3, 0x0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xa0, 0xa0,
	0xe0, 0xc0, 0x80, 0x0, 0x0, 0x4, 0x0, 0x0, 0x88, 0xa8, 0xf8, 0xd8, 0x88, 0x0, 0x0, 0x6,
	0x0, 0x0, 0xa0, 0xe0, 0x40, 0xe0, 0xa0, 0x0, 0x0, 0x4, 0x0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x20, 0xe0, 0x4, 0x0, 0x0, 0xf0, 0x30, 0x60, 0xc0, 0xf0, 0x0, 0x0, 0x5, 0x81, 0x8d,
	0xe1, 0xa0, 0xa0, 0xa0, 0xa0, 0x0, 0x0, 0x9, 0x2, 0x1a, 0xc2, 0x80, 0xc0, 0x40, 0xc0, 0x0,
	0x0, 0x8, 0xfe, 0xfc, 0xf8, 0xfc, 0xfe, 0xdf, 0x8e, 0x4, 0x0, 0x7, 0x7f, 0x3f, 0x1f, 0x3f,
	0x7f, 0xfb, 0x71, 0x20, 0x0, 0x8, 0x4, 0x8e, 0xdf, 0xfe, 0xfc, 0xf8, 0xfc, 0xfe, 0x0, 0x8,
	0x20, 0x71, 0xfb, 0x7f, 0x3f, 0x1f, 0x3f, 0x7f, 0x0, 0x7, 0xff, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0xff, 0x0, 0x9, 0x0, 0x0, 0xe0, 0x80, 0x80, 0x80, 0xe0, 0x40, 0xc0, 0x4, 0x60, 0x0,
	0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0,
	0x0, 0x4, 0x40, 0xa0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x0, 0x0, 0x4, 0x40, 0xa0, 0xe0, 0x20,
	0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0x40, 0xa0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x0, 0x0, 0x4,
	0x40, 0xa0, 0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0x0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xe0, 0x0, 0x0, 0x4, 0xc0, 0x0, 0xe0, 0x20, 0xe0, 0xa0, 0xe0, 0x0, 0x0, 0x4, 0xe0, 0xa0,
	0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0x0, 0x0, 0x4, 0xc0, 0xa0, 0xa0, 0xc0, 0xa0, 0xa0, 0xc0, 0x0,
	0x0, 0x4, 0xe0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4, 0xc0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0xc0, 0x0, 0x0, 0x4, 0xe0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0xe0, 0x0, 0x0, 0x4,
	0xe0, 0x80, 0x80, 0xe0, 0x80, 0x80, 0x80, 0x0, 0x0, 0x4};

static int DrawChar(int col, int xoffset, char *scrline, int chr)
{
	const char *font_pos = (const char *)font_bmp + (chr & 0xff) * 10;
	scrline += xoffset;
	if (xoffset < 0)
	{
		font_pos += 9;
		return xoffset + *font_pos;
	}
	for (int i = 0; i < 8; i++, font_pos++, scrline += SCREENBYTES_LINE)
	{
		char *pix = scrline;
		if (xoffset > 319)
			continue;
		if (*font_pos & 0x80)
			*pix = col;
		pix++;
		if (xoffset + 1 > 319)
			continue;
		if (*font_pos & 0x40)
			*pix = col;
		pix++;
		if (xoffset + 2 > 319)
			continue;
		if (*font_pos & 0x20)
			*pix = col;
		pix++;
		if (xoffset + 3 > 319)
			continue;
		if (*font_pos & 0x10)
			*pix = col;
		pix++;
		if (xoffset + 4 > 319)
			continue;
		if (*font_pos & 0x08)
			*pix = col;
		pix++;
		if (xoffset + 5 > 319)
			continue;
		if (*font_pos & 0x04)
			*pix = col;
		pix++;
		if (xoffset + 6 > 319)
			continue;
		if (*font_pos & 0x02)
			*pix = col;
		pix++;
		if (xoffset + 7 > 319)
			continue;
		if (*font_pos & 0x01)
			*pix = col;
	}
	return xoffset + (int)(unsigned char)font_pos[1];
}

#define MAX_QUEUED_STRINGS 200
struct QueuedString
{
	int x, y, col;
	unsigned char str[64];
} queued_strings[MAX_QUEUED_STRINGS];
int queued_string_pos;

void Nu_QueueDrawStr(void)
{
	assert(queued_string_pos < MAX_QUEUED_STRINGS);
	strncpy((char *)queued_strings[queued_string_pos].str, GetReg(REG_A0) + STRam, 64);
	queued_strings[queued_string_pos].x = GetReg(REG_D1);
	queued_strings[queued_string_pos].y = GetReg(REG_D2);
	queued_strings[queued_string_pos++].col = GetReg(REG_D0);
}

int DrawStr(int xpos, int ypos, int col, unsigned char *str, bool shadowed)
{
	int x = xpos, y = ypos, chr;
	char *screen;
	if ((y > 192) || (y < 0))
		return x;
set_line:
	screen = LOGSCREEN2;
	screen += SCREENBYTES_LINE * y;
	while (*str)
	{
		chr = *(str++);
		if (chr < 0x1e)
		{
			if (chr == '\r')
			{
				y += 10;
				x = xpos;
				goto set_line;
			}
			else if (chr == 1)
				col = *(str++);
			continue;
		}
		else if (chr == 0x1e)
		{
			x = (*(str++)) * 2;
			continue;
		}
		else if (chr < 0x20)
		{
			x = (*(str++)) * 2;
			y = *(str++);
			goto set_line;
		}
		if (shadowed)
			DrawChar(0, x + 1, screen + SCREENBYTES_LINE, chr - 0x20);
		x = DrawChar(col, x, screen, chr - 0x20);
	}
	return x;
}

/* =========================================================================
 * Palette / colour helpers
 * ========================================================================= */
static void _BuildRGBPalette(unsigned int *rgb, unsigned short *st, int len)
{
	for (int i = 0; i < len; i++, st++)
	{
		int c = *st;
		int b = (c & 0xf) << 4, g = (c & 0xf0), r = (c & 0xf00) >> 4;
		rgb[i] = 0xff000000 | (b << 16) | (g << 8) | r;
	}
}
static inline void split_rgb444b(int rgb, int *r, int *g, int *b)
{
	*r = (rgb & 0xf00) >> 4;
	*g = (rgb & 0xf0);
	*b = (rgb & 0xf) << 4;
}
static inline void split_rgb444i(unsigned int rgb,
								 unsigned int *r, unsigned int *g, unsigned int *b)
{
	*r = (rgb & 0xf00) << 20;
	*g = (rgb & 0xf0) << 24;
	*b = (rgb & 0xf) << 28;
}
static inline void read_m68k_vertex(int st_vptr, int output[3])
{
	output[0] = STMemory_ReadLong(st_vptr);
	output[1] = STMemory_ReadLong(st_vptr + 4);
	output[2] = -STMemory_ReadLong(st_vptr + 8);
}

/* =========================================================================
 * ZNode / object data stream
 * ========================================================================= */
struct ZNode
{
	unsigned int z;
	struct ZNode *less, *more;
	void *data;
};

#define MAX_OBJ_DATA (2 << 18)
static unsigned char obj_data_area[MAX_OBJ_DATA];
static int obj_data_pos;
#define MAX_ZNODES 1000
static struct ZNode znode_buf[MAX_ZNODES];
static int znode_buf_pos;
static struct ZNode *znode_start;
static struct ZNode *znode_cur;

static inline void znode_databegin(void) { znode_cur->data = &obj_data_area[obj_data_pos]; }

static inline void znode_wrlong(int v)
{
	if (obj_data_pos + 4 > MAX_OBJ_DATA)
	{
		log_printf("OBJ_DATA OVERFLOW at pos %d\n", obj_data_pos);
		return;
	}

	if (obj_data_pos + 4 > MAX_OBJ_DATA)
		return; // drop rather than corrupt
	*((int *)(obj_data_area + obj_data_pos)) = v;
	obj_data_pos += 4;
}
static inline void znode_wrword(short v)
{
	if (obj_data_pos + 2 > MAX_OBJ_DATA)
		return;
	*((short *)(obj_data_area + obj_data_pos)) = v;
	obj_data_pos += 2;
}
static inline void znode_wrbyte(char v)
{
	if (obj_data_pos + 1 > MAX_OBJ_DATA)
		return;
	*((char *)(obj_data_area + obj_data_pos)) = v;
	obj_data_pos++;
}

static inline void znode_wrnormal(p68K loc)
{
	znode_wrword(STMemory_ReadWord(loc));
	znode_wrword(STMemory_ReadWord(loc + 2));
	znode_wrword(STMemory_ReadWord(loc + 4));
}
static void znode_wrmatrix(p68K loc)
{
	for (int i = 0; i < 9; i++)
		znode_wrword(STMemory_ReadWord(loc + i * 2));
}
static inline void znode_wrvertex(p68K loc)
{
	znode_wrlong(STMemory_ReadLong(loc));
	znode_wrlong(STMemory_ReadLong(loc + 4));
	znode_wrlong(-STMemory_ReadLong(loc + 8));
}
static inline void znode_wrlightsource(p68K loc)
{
	znode_wrlong(-STMemory_ReadWord(loc));
	znode_wrlong(-STMemory_ReadWord(loc + 2));
	znode_wrlong(STMemory_ReadWord(loc + 4));
}
static inline void znode_wrcolor(int rgb444col)
{
	int r, g, b;
	split_rgb444b(rgb444col, &r, &g, &b);
	znode_wrbyte(r);
	znode_wrbyte(g);
	znode_wrbyte(b);
	znode_wrbyte(0);
}

static inline int znode_rdlong(void **d)
{
	int v = *((int *)(*d));
	*d = (char *)(*d) + 4;
	return v;
}
static inline short znode_rdword(void **d)
{
	short v = *((short *)(*d));
	*d = (char *)(*d) + 2;
	return v;
}
static inline char znode_rdbyte(void **d)
{
	char v = *((char *)(*d));
	*d = (char *)(*d) + 1;
	return v;
}

static void znode_rdmatrix(void **data, GLfloat m[16])
{
	short val;
#define rdmatrixval(idx)                   \
	{                                      \
		val = znode_rdword(data);          \
		m[idx] = ((float)val) / -32768.0f; \
	}
	rdmatrixval(0);
	rdmatrixval(1);
	rdmatrixval(2);
	m[3] = 0.0f;
	rdmatrixval(4);
	rdmatrixval(5);
	rdmatrixval(6);
	m[7] = 0.0f;
	rdmatrixval(8);
	rdmatrixval(9);
	rdmatrixval(10);
	m[11] = 0.0f;
	m[12] = m[13] = m[14] = 0.0f;
	m[15] = 1.0f;
#undef rdmatrixval
}
static inline void znode_rdnormal(void **d, short n[3])
{
	n[0] = znode_rdword(d);
	n[1] = znode_rdword(d);
	n[2] = znode_rdword(d);
}
static inline void znode_rdvertex(void **d, int v[3])
{
	v[0] = znode_rdlong(d);
	v[1] = znode_rdlong(d);
	v[2] = znode_rdlong(d);
}
static inline void znode_rdvertexf(void **d, float v[3])
{
	v[0] = (float)znode_rdlong(d);
	v[1] = (float)znode_rdlong(d);
	v[2] = (float)znode_rdlong(d);
}
static inline void znode_rdvertexd(void **d, GLdouble v[3])
{
	v[0] = znode_rdlong(d);
	v[1] = znode_rdlong(d);
	v[2] = znode_rdlong(d);
}
static inline void znode_rdcolorv(void **d, int *rgb)
{
	rgb[0] = (unsigned char)znode_rdbyte(d);
	rgb[1] = (unsigned char)znode_rdbyte(d);
	rgb[2] = (unsigned char)znode_rdbyte(d);
	*d = (char *)(*d) + 1;
}
static inline void znode_rdcolor(void **d, int *r, int *g, int *b)
{
	*r = znode_rdbyte(d);
	*g = znode_rdbyte(d);
	*b = znode_rdbyte(d);
	*d = (char *)(*d) + 1;
}

/* =========================================================================
 * Primitive enum
 * ========================================================================= */
enum NuPrimitive
{
	NU_END,
	NU_TRIANGLE,
	NU_QUAD,
	NU_LINE,
	NU_BEZIER_LINE,
	NU_TEARDROP,
	NU_COMPLEX_SNEXT,
	NU_COMPLEX_START,
	NU_COMPLEX_END,
	NU_COMPLEX_INNER,
	NU_COMPLEX_BEZIER,
	NU_TWINKLYCIRCLE,
	NU_PLANET,
	NU_CIRCLE,
	NU_CYLINDER,
	NU_BLOB,
	NU_OVALTHINGY,
	NU_POINT,
	NU_2DLINE,
	NU_MAX
};

static inline void end_node(void) { znode_wrlong(0); }

/* =========================================================================
 * ZNode tree
 * ========================================================================= */
static void add_node(struct ZNode **node, unsigned int zval)
{
	assert(znode_buf_pos < MAX_ZNODES);
	if (znode_cur)
		end_node();
	*node = znode_cur = &znode_buf[znode_buf_pos++];
	znode_cur->z = zval;
	znode_cur->less = znode_cur->more = NULL;
	znode_databegin();
}
static void znode_insert(struct ZNode *node, unsigned int zval)
{
	if (zval > node->z)
	{
		if (node->more)
			znode_insert(node->more, zval);
		else
			add_node(&node->more, zval);
	}
	else
	{
		if (node->less)
			znode_insert(node->less, zval);
		else
			add_node(&node->less, zval);
	}
}

static bool no_znodes_kthx;

void Nu_InsertZNode(void)
{
	unsigned int zval = GetReg(4);
	if (use_renderer == R_OLD)
		return;
	if (no_znodes_kthx)
		return;
	if (!znode_start)
		add_node(&znode_start, zval);
	else
		znode_insert(znode_start, zval);
}

void Nu_3DViewInit(void)
{
	queued_string_pos = 0;
	znode_buf_pos = 0;
	obj_data_pos = 0;
	znode_start = znode_cur = NULL;
	no_znodes_kthx = FALSE;
}

/* =========================================================================
 * Lighting helpers (replaces glLightfv / glEnable GL_LIGHTING)
 *
 * We set uniforms on prog_lit directly; use_lighting gates which
 * shader is selected in batch_end_*.
 * ========================================================================= */
static void lighting_on(float light_vec[4], int rgb444_light_col,
						int rgb444_extra_col, int rgb444_obj_col)
{
	bool do_not_light = (rgb444_obj_col & (1 << 8)) != 0;
	if (do_not_light)
		rgb444_obj_col ^= (1 << 8);

	unsigned int er, eg, eb, or_, og, ob, lr, lg, lb;

	split_rgb444i(rgb444_light_col, &lr, &lg, &lb);
	if (rgb444_obj_col & (1 << 4))
	{
		rgb444_obj_col ^= (1 << 4);
		split_rgb444i(rgb444_obj_col, &or_, &og, &ob);
		split_rgb444i(rgb444_extra_col, &er, &eg, &eb);
		or_ += er;
		og += eg;
		ob += eb;
	}
	else
	{
		split_rgb444i(rgb444_obj_col, &or_, &og, &ob);
	}

	if (do_not_light)
	{
		use_lighting = false;
		set_color3f(or_ / 4294967295.0f, og / 4294967295.0f, ob / 4294967295.0f);
	}
	else
	{
		use_lighting = true;
		lit_light_dir[0] = light_vec[0];
		lit_light_dir[1] = light_vec[1];
		lit_light_dir[2] = light_vec[2];
		lit_diffuse[0] = lr / 4294967295.0f;
		lit_diffuse[1] = lg / 4294967295.0f;
		lit_diffuse[2] = lb / 4294967295.0f;
		lit_ambient[0] = or_ / 4294967295.0f;
		lit_ambient[1] = og / 4294967295.0f;
		lit_ambient[2] = ob / 4294967295.0f;
	}
}
static void lighting_off(void) { use_lighting = false; }

/* =========================================================================
 * Bezier helper
 * ========================================================================= */
#define BEZIER_STEPS 10
static void eval_bezier(GLdouble *out, float t, float cp[4][3])
{
	float t2 = t * t, c = 1 - t, d = t2 * t;
	float a = c * c * c, b = c * c * t * 3, cc_ = c * 3 * t2;
	out[0] = cp[0][0] * a + cp[1][0] * b + cp[2][0] * cc_ + cp[3][0] * d;
	out[1] = cp[0][1] * a + cp[1][1] * b + cp[2][1] * cc_ + cp[3][1] * d;
	out[2] = cp[0][2] * a + cp[1][2] * b + cp[2][2] * cc_ + cp[3][2] * d;
}

/* =========================================================================
 * Complex polygon tessellation state
 * ========================================================================= */
#define MAX_TESS_VERTICES 400
static GLdouble tess_vertices[MAX_TESS_VERTICES][3];
static int tess_vpos;

/* We need the model/projection matrices for gluProject replacement */
static GLdouble tessModelMatrix[16];
static GLdouble tessProjMatrix[16];
static GLint tessViewport[4];

static bool do_start_complex;
static int complex_col_rgb444;

static void put_complex_start_4real(void)
{
	znode_wrlong(NU_COMPLEX_START);
	znode_wrcolor(complex_col_rgb444);
	no_znodes_kthx = TRUE;
}

static inline void push_tess_vertex(GLdouble v[3])
{
	static double prev[3];
	if (v[0] == prev[0] && v[1] == prev[1] && v[2] == prev[2])
		return;
	prev[0] = v[0];
	prev[1] = v[1];
	prev[2] = v[2];

	if (!gl_project(v[0], v[1], v[2], tessModelMatrix, tessProjMatrix, tessViewport,
					&v[0], &v[1], &v[2]))
	{
		tess_vpos--;
	}
	else
	{
		if (prev[2] >= 0.0)
			return;
		gluTessVertex(tobj, v, v);
	}
}

static void drawDisk(GLdouble innerRadius, GLdouble outerRadius,
					 GLint slices, GLint loops)
{
	GLfloat dr = (outerRadius - innerRadius) / (GLfloat)loops;
	GLdouble da = 2.0 * M_PI / slices;
	GLfloat r1 = (GLfloat)innerRadius;

	for (int l = 0; l < loops; l++)
	{
		GLfloat r2 = r1 + dr;
		batch_begin(GL_TRIANGLE_STRIP);
		for (int s = 0; s <= slices; s++)
		{
			double a = (s == slices) ? 0.0 : s * da;
			float sa = sinf((float)a), ca = cosf((float)a);
			batch_vertex3f(r2 * sa, r2 * ca, 0.0f);
			batch_vertex3f(r1 * sa, r1 * ca, 0.0f);
		}
		batch_end_flat();
		r1 = r2;
	}
}

static void drawCylinder(GLdouble baseRadius, GLdouble topRadius, GLdouble height,
						 GLint slices, GLint stacks)
{
	GLdouble da = 2.0 * M_PI / slices;
	GLdouble dr = (topRadius - baseRadius) / stacks;
	GLdouble dz = height / stacks;
	GLfloat nz = (GLfloat)((baseRadius - topRadius) / height);

	for (int i = 0; i < slices; i++)
	{
		float a1 = (float)(i * da), a2 = (float)((i + 1) * da);
		float x1 = -sinf(a1), y1 = cosf(a1);
		float x2 = -sinf(a2), y2 = cosf(a2);
		float nl = sqrtf(x1 * x1 + y1 * y1 + nz * nz);
		if (nl < 0.00001f)
			nl = 1.0f;

		lit_begin(GL_TRIANGLE_STRIP);
		double z = 0.0, r = baseRadius;
		for (int j = 0; j <= stacks; j++)
		{
			float vA[3] = {x1 * (float)r, y1 * (float)r, (float)z};
			float vB[3] = {x2 * (float)r, y2 * (float)r, (float)z};
			lit_normal3f(x1 / nl, y1 / nl, nz / nl);
			lit_vertex3fv(vA);
			lit_normal3f(x2 / nl, y2 / nl, nz / nl);
			lit_vertex3fv(vB);
			z += dz;
			r += dr;
		}
		batch_end_lit();
	}
}

/* =========================================================================
 * Nu_Draw* — all immediate-mode calls replaced with batch helpers
 * ========================================================================= */

/* --- Triangle --- */
void Nu_PutTriangle(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_TRIANGLE);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrvertex(GetReg(REG_A2) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
static void Nu_DrawTriangle(void **data)
{
	float v1[3], v2[3], v3[3];
	int rgb[3];
	znode_rdvertexf(data, v1);
	znode_rdvertexf(data, v2);
	znode_rdvertexf(data, v3);
	znode_rdcolorv(data, rgb);
	set_color3ub(rgb[0], rgb[1], rgb[2]);
	if (use_renderer == R_GLWIRE)
	{
		batch_begin(GL_LINE_STRIP);
		batch_vertex3fv(v1);
		batch_vertex3fv(v2);
		batch_vertex3fv(v3);
		batch_vertex3fv(v1);
	}
	else
	{
		batch_begin(GL_TRIANGLES);
		batch_vertex3fv(v1);
		batch_vertex3fv(v2);
		batch_vertex3fv(v3);
	}
	batch_end_flat();
}

/* --- Quad --- */
void Nu_PutQuad(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_QUAD);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrvertex(GetReg(REG_A2) + 4);
	znode_wrvertex(GetReg(REG_A3) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
static void Nu_DrawQuad(void **data)
{
	int v1[3], v2[3], v3[3], v4[3], r, g, b;
	znode_rdvertex(data, v1);
	znode_rdvertex(data, v2);
	znode_rdvertex(data, v3);
	znode_rdvertex(data, v4);
	znode_rdcolor(data, &r, &g, &b);
	set_color3ub(r, g, b);
	if (use_renderer == R_GLWIRE)
	{
		batch_begin(GL_LINE_STRIP);
		batch_vertex3iv(v1);
		batch_vertex3iv(v2);
		batch_vertex3iv(v3);
		batch_vertex3iv(v4);
		batch_vertex3iv(v1);
	}
	else
	{
		batch_begin(GL_TRIANGLE_STRIP);
		batch_vertex3iv(v1);
		batch_vertex3iv(v2);
		batch_vertex3iv(v4);
		batch_vertex3iv(v3);
	}
	batch_end_flat();
}

/* --- Line --- */
void Nu_PutLine(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_LINE);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
static void Nu_DrawLine(void **data)
{
	int v1[3], v2[3], r, g, b;
	znode_rdvertex(data, v1);
	znode_rdvertex(data, v2);
	znode_rdcolor(data, &r, &g, &b);
	set_color3ub(r, g, b);
	batch_begin(GL_LINES);
	batch_vertex3iv(v1);
	batch_vertex3iv(v2);
	batch_end_flat();
}

/* --- Bezier line --- */
void Nu_PutBezierLine(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_BEZIER_LINE);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrvertex(GetReg(REG_A2) + 4);
	znode_wrvertex(GetReg(REG_A3) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
static void Nu_DrawBezierLine(void **data)
{
	GLfloat cp[4][3];
	GLdouble out[3];
	int r, g, b;
	znode_rdvertexf(data, cp[0]);
	znode_rdvertexf(data, cp[1]);
	znode_rdvertexf(data, cp[2]);
	znode_rdvertexf(data, cp[3]);
	znode_rdcolor(data, &r, &g, &b);
	set_color3ub(r, g, b);
	batch_begin(GL_LINE_STRIP);
	for (int i = 0; i <= 20; i++)
	{
		eval_bezier(out, i / 20.0f, cp);
		batch_vertex3dv(out);
	}
	batch_end_flat();
}

/* --- Teardrop --- */
void Nu_PutTeardrop(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_TEARDROP);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
#define TD_STRETCH 1.3333333333f
#define TD_BROADEN 0.33f
#define TD_BEZIER_STEPS 40
static void Nu_DrawTeardrop(void **data)
{
	GLfloat dir[3], cp[4][3];
	GLdouble out[3];
	int r, g, b;
	if (use_renderer == R_OLD)
		return;
	znode_rdvertexf(data, dir);
	znode_rdvertexf(data, cp[0]);
	znode_rdcolor(data, &r, &g, &b);
	dir[0] -= cp[0][0];
	dir[1] -= cp[0][1];
	dir[2] -= cp[0][2];
	float ppd[3] = {-dir[1], dir[0], dir[2]};
	cp[1][0] = cp[0][0] + TD_STRETCH * dir[0] + TD_BROADEN * ppd[0];
	cp[1][1] = cp[0][1] + TD_STRETCH * dir[1] + TD_BROADEN * ppd[1];
	cp[1][2] = cp[0][2] + dir[2];
	cp[2][0] = cp[0][0] + TD_STRETCH * dir[0] - TD_BROADEN * ppd[0];
	cp[2][1] = cp[0][1] + TD_STRETCH * dir[1] - TD_BROADEN * ppd[1];
	cp[2][2] = cp[0][2] + dir[2];
	cp[3][0] = cp[0][0];
	cp[3][1] = cp[0][1];
	cp[3][2] = cp[0][2];
	set_color3ub(r, g, b);
	/* GL_TRIANGLE_FAN is unreliable on GLES/WebGL2 — expand manually */
	{
		GLdouble pts[TD_BEZIER_STEPS + 1][3];
		for (int i = 0; i <= TD_BEZIER_STEPS; i++)
			eval_bezier(pts[i], i / (float)TD_BEZIER_STEPS, cp);
		batch_begin(GL_TRIANGLES);
		for (int i = 0; i < TD_BEZIER_STEPS; i++)
		{
			batch_vertex3f((float)cp[0][0], (float)cp[0][1], (float)cp[0][2]);
			batch_vertex3dv(pts[i]);
			batch_vertex3dv(pts[i + 1]);
		}
		batch_end_flat();
	}
}

/* --- Point --- */
void Nu_PutColoredPoint(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_POINT);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrcolor(GetReg(REG_D0));
	znode_wrlong(2);
}
void Nu_PutPoint(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_POINT);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrcolor(0xfff);
	znode_wrlong(1);
}
static void Nu_DrawPoint(void **data)
{
	int v1[3], point_size, r, g, b;
	if (use_renderer == R_OLD)
		return;
	znode_rdvertex(data, v1);
	znode_rdcolor(data, &r, &g, &b);
	point_size = znode_rdlong(data);
	// glPointSize((float)point_size);
	set_point_size((float)point_size);
	set_color3ub(r, g, b);
	batch_begin(GL_POINTS);
	batch_vertex3iv(v1);
	batch_end_flat();
	// glPointSize(1.0f);
	set_point_size(1.0f);
}

/* --- 2D Line --- */
void Nu_Put2DLine(void)
{
	if (use_renderer == R_OLD)
		return;
	if (!znode_start)
		add_node(&znode_start, 0);
	else
		znode_insert(znode_start, 0);
	znode_wrlong(NU_2DLINE);
	znode_wrword(GetReg(REG_D0));
	znode_wrword(GetReg(REG_D1));
	znode_wrword(GetReg(REG_D2));
	znode_wrword(GetReg(REG_D3));
	znode_wrword(GetReg(REG_D4));
}
static void Nu_Draw2DLine(void **data)
{
	short x1 = znode_rdword(data), y1 = znode_rdword(data);
	short x2 = znode_rdword(data), y2 = znode_rdword(data);
	int col = MainRGBPalette[(znode_rdword(data) & 0xffff) >> 2];
	(void)col; /* colour unused in original too */
	push_ortho();
	set_ctrl_viewport();
	set_color3ub(0, 255, 0);
	batch_begin(GL_LINES);
	batch_vertex3f(x1, 199 - y1, 0);
	batch_vertex3f(x2, 199 - y2, 0);
	batch_end_flat();
	set_main_viewport();
	pop_ortho();
}

/* --- Twinkly circle --- */
void Nu_PutTwinklyCircle(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_TWINKLYCIRCLE);
	znode_wrlong(GetReg(REG_D2));
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrcolor(GetReg(REG_D6));
}

static void Nu_DrawTwinklyCircle(void **data)
{
	int v1[3];
	unsigned int dreg2;
	int r, g, b;
	dreg2 = znode_rdlong(data);
	znode_rdvertex(data, v1);
	znode_rdcolor(data, &r, &g, &b);
	set_color3ub(r, g, b);
	float size = -0.002f * ((short)dreg2) * v1[2];
	mat_push();
	mat_translate(v1[0], v1[1], v1[2]);
	if (size > 0.0f)
		drawDisk(0.0, size, 32, 1);
	size = -0.002f * ((short)dreg2) * v1[2] - 0.016f * v1[2];
	if (size > 0.0f)
	{
		batch_begin(GL_LINES);
		batch_vertex3f(-size, 0, 0);
		batch_vertex3f(+size, 0, 0);
		batch_vertex3f(0, -size, 0);
		batch_vertex3f(0, +size, 0);
		batch_end_flat();
	}
	mat_pop();
}

/* --- Circle --- */
void Nu_PutCircle(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_CIRCLE);
	znode_wrlong(GetReg(REG_D2));
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrcolor(GetReg(REG_D6));
}
static void Nu_DrawCircle(void **data)
{
	int v1[3];
	unsigned int dreg2;
	int r, g, b;
	dreg2 = znode_rdlong(data);
	znode_rdvertex(data, v1);
	znode_rdcolor(data, &r, &g, &b);
	set_color3ub(r, g, b);
	float size = -0.002f * ((short)dreg2) * v1[2];
	mat_push();
	mat_translate(v1[0], v1[1], v1[2]);
	drawDisk(0.0, size, 32, 1);
	mat_pop();
}

/* --- Complex polygon --- */
void Nu_PutComplexStart(void) { /* host-side only */ }
void Nu_ComplexStart(void)
{
	if (use_renderer == R_OLD)
		return;
	do_start_complex = TRUE;
	complex_col_rgb444 = GetReg(REG_D6);
}
static void Nu_DrawComplexStart(void **data)
{
	tess_vpos = 0;
	if (use_renderer == R_GL)
	{
		/* Capture current MVP as double matrices for gluProject */
		for (int i = 0; i < 16; i++)
			tessModelMatrix[i] = mv->m[i];
		mat4 p = proj_matrix;
		for (int i = 0; i < 16; i++)
			tessProjMatrix[i] = p.m[i];
		/* Use cached viewport — glGetIntegerv forces a CPU/GPU sync stall */
		memcpy(tessViewport, cached_viewport, sizeof(cached_viewport));

		/* Switch to screen-space ortho for tessellator output */
		proj_matrix = mat4_ortho(tessViewport[0], tessViewport[0] + tessViewport[2],
								 tessViewport[1], tessViewport[1] + tessViewport[3], -1, 1);
		mat_push();
		mat_load_identity();

		gluTessNormal(tobj, 0, 0, 1);
		gluTessProperty(tobj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
		gluTessBeginPolygon(tobj, NULL);
		gluTessBeginContour(tobj);
	}
	else
	{
		batch_begin(GL_LINE_STRIP);
	}
	znode_rdcolor(data, &complex_col[0], &complex_col[1], &complex_col[2]);
	set_color3ub(complex_col[0], complex_col[1], complex_col[2]);
}

void Nu_ComplexSNext(void)
{
	if (use_renderer == R_OLD)
		return;
	if (do_start_complex)
	{
		put_complex_start_4real();
		do_start_complex = FALSE;
	}
	znode_wrlong(NU_COMPLEX_SNEXT);
	znode_wrvertex(GetReg(REG_A0) + 4);
}
static void Nu_DrawComplexSNext(void **data)
{
	if (use_renderer == R_GLWIRE)
	{
		znode_rdvertexd(data, tess_vertices[tess_vpos]);
		batch_vertex3dv(tess_vertices[tess_vpos++]);
	}
	else
	{
		assert(tess_vpos < MAX_TESS_VERTICES);
		znode_rdvertexd(data, tess_vertices[tess_vpos]);
		push_tess_vertex(tess_vertices[tess_vpos]);
		tess_vpos++;
	}
}
void Nu_ComplexSBegin(void) { Nu_ComplexSNext(); }

void Nu_ComplexEnd(void)
{
	if (use_renderer == R_OLD)
		return;
	if (do_start_complex)
	{
		put_complex_start_4real();
		do_start_complex = FALSE;
	}
	znode_wrlong(NU_COMPLEX_END);
	do_start_complex = FALSE;
	no_znodes_kthx = FALSE;
}
static void Nu_DrawComplexEnd(void **data)
{
	(void)data;
	if (use_renderer == R_GL)
	{
		gluTessEndContour(tobj);
		gluTessEndPolygon(tobj);
		/* Restore matrices */
		mat_pop();
		proj_matrix = saved_proj; /* tessellator saved these in DrawComplexStart */
	}
	else if (use_renderer == R_GLWIRE)
	{
		batch_vertex3dv(tess_vertices[0]);
		batch_end_flat();
	}
}

void Nu_ComplexStartInner(void)
{
	if (use_renderer == R_OLD)
		return;
	if (do_start_complex)
	{
		put_complex_start_4real();
		do_start_complex = FALSE;
	}
	znode_wrlong(NU_COMPLEX_INNER);
}
static void Nu_DrawComplexStartInner(void **data)
{
	(void)data;
	if (use_renderer == R_GL)
	{
		gluTessEndContour(tobj);
		gluTessBeginContour(tobj);
	}
	else if (use_renderer == R_GLWIRE)
	{
		batch_end_flat();
		batch_begin(GL_LINE_STRIP);
		tess_vpos = 0;
	}
}

void Nu_ComplexBezier(void)
{
	if (use_renderer == R_OLD)
		return;
	if (do_start_complex)
	{
		put_complex_start_4real();
		do_start_complex = FALSE;
	}
	znode_wrlong(NU_COMPLEX_BEZIER);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrvertex(GetReg(REG_A1) + 4);
	znode_wrvertex(GetReg(REG_A2) + 4);
	znode_wrvertex(GetReg(REG_A3) + 4);
}
static void Nu_DrawComplexBezier(void **data)
{
	GLfloat cp[4][3];
	GLdouble v[3];
	int bezier_steps = 10;
	znode_rdvertexf(data, cp[0]);
	znode_rdvertexf(data, cp[1]);
	znode_rdvertexf(data, cp[2]);
	znode_rdvertexf(data, cp[3]);
	assert(tess_vpos + bezier_steps < MAX_TESS_VERTICES);
	float delta = 1.0f / bezier_steps;
	if (use_renderer == R_GLWIRE)
	{
		tess_vertices[tess_vpos][0] = cp[0][0];
		tess_vertices[tess_vpos][1] = cp[0][1];
		tess_vertices[tess_vpos++][2] = cp[0][2];
		for (int i = 0; i <= bezier_steps; i++)
		{
			eval_bezier(v, i * delta, cp);
			batch_vertex3dv(v);
		}
		return;
	}
	for (int i = 0; i <= bezier_steps; i++)
	{
		eval_bezier(&tess_vertices[tess_vpos][0], i * delta, cp);
		push_tess_vertex(tess_vertices[tess_vpos]);
		tess_vpos++;
	}
}

/* --- Sphere (icosphere subdivision, used by planet) --- */
static float nus_vdata[12][3] = {
	{-0.525731f, 0, 0.850651f}, {0.525731f, 0, 0.850651f}, {-0.525731f, 0, -0.850651f}, {0.525731f, 0, -0.850651f}, {0, 0.850651f, 0.525731f}, {0, 0.850651f, -0.525731f}, {0, -0.850651f, 0.525731f}, {0, -0.850651f, -0.525731f}, {0.850651f, 0.525731f, 0}, {-0.850651f, 0.525731f, 0}, {0.850651f, -0.525731f, 0}, {-0.850651f, -0.525731f, 0}};
static int nus_tindices[20][3] = {
	{0, 4, 1}, {0, 9, 4}, {9, 5, 4}, {4, 5, 8}, {4, 8, 1}, {8, 10, 1}, {8, 3, 10}, {5, 3, 8}, {5, 2, 3}, {2, 7, 3}, {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6}, {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5}, {7, 2, 11}};

static void Normalise(float v[3])
{
	float d = 1.0f / sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;
}

static void nuSubdivide(float v1[3], float v2[3], float v3[3], int depth)
{
	if (depth == 0)
	{
		/* normal = vertex position on unit sphere */
		lit_normal3fv(v1);
		lit_vertex3fv(v1);
		lit_normal3fv(v2);
		lit_vertex3fv(v2);
		lit_normal3fv(v3);
		lit_vertex3fv(v3);
		return;
	}
	float v12[3], v23[3], v31[3];
	for (int i = 0; i < 3; i++)
	{
		v12[i] = v1[i] + v2[i];
		v23[i] = v2[i] + v3[i];
		v31[i] = v3[i] + v1[i];
	}
	Normalise(v12);
	Normalise(v23);
	Normalise(v31);
	nuSubdivide(v1, v12, v31, depth - 1);
	nuSubdivide(v2, v23, v12, depth - 1);
	nuSubdivide(v3, v31, v23, depth - 1);
	nuSubdivide(v12, v23, v31, depth - 1);
}

#define NUSPHERE_SUBDIVS 4
static void nuSphere(float size)
{
	mat_scale(size, size, size);
	lit_begin(GL_TRIANGLES);
	for (int i = 0; i < 20; i++)
		nuSubdivide(nus_vdata[nus_tindices[i][0]],
					nus_vdata[nus_tindices[i][1]],
					nus_vdata[nus_tindices[i][2]], NUSPHERE_SUBDIVS);
	batch_end_lit();
}

/* --- Planet --- */
void Nu_PutPlanet(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_PLANET);
	znode_wrlong(GetReg(REG_D6));
	znode_wrlong(GetReg(REG_D1));
	znode_wrlong(GetReg(REG_D0));
	znode_wrlightsource(GetReg(REG_A1));
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrmatrix(GetReg(REG_A6) - 36);
}
static void Nu_DrawPlanet(void **data)
{
	int v1[3], size;
	float light_vec[4];
	GLfloat rot_matrix[16];
	unsigned int or_, og, ob, lr, lg, lb;

	split_rgb444i(znode_rdlong(data), &or_, &og, &ob);
	split_rgb444i(znode_rdlong(data), &lr, &lg, &lb);
	size = znode_rdlong(data);
	znode_rdvertexf(data, light_vec);
	light_vec[3] = 0;
	znode_rdvertex(data, v1);
	znode_rdmatrix(data, rot_matrix);

	/* Set lighting uniforms */
	lit_light_dir[0] = light_vec[0];
	lit_light_dir[1] = light_vec[1];
	lit_light_dir[2] = light_vec[2];
	lit_diffuse[0] = lr / 4294967295.0f;
	lit_diffuse[1] = lg / 4294967295.0f;
	lit_diffuse[2] = lb / 4294967295.0f;
	lit_ambient[0] = or_ / 4294967295.0f;
	lit_ambient[1] = og / 4294967295.0f;
	lit_ambient[2] = ob / 4294967295.0f;
	use_lighting = true;

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	mat_push();
	mat_translate(v1[0], v1[1], v1[2]);
	mat_rotate_deg(180, 1, 0, 0);
	mat_rotate_deg(180, 0, 1, 0);
	mat_mult(rot_matrix);
	nuSphere(size * 1.0080f);
	mat_pop();
	glDisable(GL_CULL_FACE);
	use_lighting = false;
}

/* --- Blob --- */
void Nu_PutBlob(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_BLOB);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrlong(GetReg(REG_D0));
	znode_wrlong(GetReg(REG_D1));
}
static void Nu_DrawBlob(void **data)
{
	int v1[3];
	unsigned int r, g, b;
	int rad;
	znode_rdvertex(data, v1);
	split_rgb444i(znode_rdlong(data), &r, &g, &b);
	rad = znode_rdlong(data) & 0xffff;
	int edges = rad + 4;
	set_color3f(r / 4294967295.0f, g / 4294967295.0f, b / 4294967295.0f);
	if (rad < 3)
	{
		// glPointSize((float)(rad / 2 + 1));
		set_point_size((float)(rad / 2 + 1));
		batch_begin(GL_POINTS);
		batch_vertex3iv(v1);
		batch_end_flat();
		// glPointSize(1.0f);
		set_point_size(1.0f);
	}
	else
	{
		mat_push();
		mat_translate(v1[0], v1[1], v1[2]);
		drawDisk(0.0, -0.002 * (rad)*v1[2], edges, 1);
		mat_pop();
	}
}

/* --- Oval --- */
void Nu_PutOval(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_OVALTHINGY);
	znode_wrvertex(GetReg(REG_A0) + 4);
	znode_wrlong(GetReg(REG_D3));
	znode_wrlong(GetReg(REG_D4));
	znode_wrlong(GetReg(REG_D5));
	znode_wrlong(GetReg(REG_D6));
}
static void Nu_DrawOval(void **data)
{
	int v1[3], rad, r, g, b;
	unsigned short d, e, f;
	znode_rdvertex(data, v1);
	r = g = b = 0;
	d = (short)znode_rdlong(data);
	e = (short)znode_rdlong(data);
	f = (short)znode_rdlong(data);
	rad = (short)znode_rdlong(data);
	set_color3ub(r, g, b);
	mat_push();
	mat_translate(v1[0], v1[1], v1[2]);
	drawDisk(0.0, rad, 32, 1);
	mat_pop();
}

/* --- Cylinder --- */
void Nu_PutCylinder(void)
{
	if (use_renderer == R_OLD)
		return;
	znode_wrlong(NU_CYLINDER);
	znode_wrlightsource(GetReg(REG_A4));
	znode_wrlong(GetReg(REG_D3));
	znode_wrlong(GetReg(REG_D2));
	znode_wrlong(GetReg(REG_D6));
	znode_wrvertex(GetReg(REG_A2) + 4);
	znode_wrvertex(GetReg(REG_A3) + 4);
	znode_wrlong(GetReg(REG_D0));
	znode_wrlong(GetReg(REG_D1));
	znode_wrlong(GetReg(REG_D5));
	znode_wrlong(GetReg(REG_D4));
}
static void Nu_DrawCylinder(void **data)
{
	float light_vec[4];
	int v1[3], v2[3];
	float vdiff[3];
	int light_col, obj_col, extra_col;
	int radOne, radTwo;

	znode_rdvertexf(data, light_vec);
	light_col = znode_rdlong(data);
	obj_col = znode_rdlong(data);
	extra_col = znode_rdlong(data);
	znode_rdvertex(data, v1);
	znode_rdvertex(data, v2);
	vdiff[0] = v2[0] - v1[0];
	vdiff[1] = v2[1] - v1[1];
	vdiff[2] = v2[2] - v1[2];
	float h = sqrtf(vdiff[0] * vdiff[0] + vdiff[1] * vdiff[1] + vdiff[2] * vdiff[2]);
	radOne = znode_rdlong(data) & 0xffff;
	radTwo = znode_rdlong(data) & 0xffff;

	mat_push();
	mat_translate(v1[0], v1[1], v1[2]);
	mat_rotate_deg(-RAD_2_DEG * (atan2f(vdiff[2], vdiff[0]) - (float)M_PI / 2), 0, 1, 0);
	mat_rotate_deg(-RAD_2_DEG * asinf(vdiff[1] / h), 1, 0, 0);

#define CYLINDER_POOP 20
	lighting_on(light_vec, light_col, extra_col, znode_rdlong(data));
	drawDisk(0.0, radOne, CYLINDER_POOP, 1);
	mat_translate(0, 0, h);
	lighting_on(light_vec, light_col, extra_col, znode_rdlong(data));
	drawDisk(0.0, radTwo, CYLINDER_POOP, 1);
	mat_translate(0, 0, -h);

	glEnable(GL_CULL_FACE);
	lighting_on(light_vec, light_col, extra_col, obj_col);
	drawCylinder(radOne, radTwo, h, CYLINDER_POOP, 1);
	glDisable(GL_CULL_FACE);

	mat_pop();
	lighting_off();
}

/* =========================================================================
 * Nu_IsGLRenderer / Nu_GLClearArea
 * ========================================================================= */
void Nu_IsGLRenderer(void)
{
	SetReg(0, use_renderer != R_OLD ? 1 : 0);
}

void Nu_GLClearArea(void)
{
	if (use_renderer == R_OLD)
		return;
	int x1 = GetReg(0) & 0xffff, y1 = GetReg(1) & 0xffff;
	int x2 = GetReg(2) & 0xffff, y2 = GetReg(3) & 0xffff;

	push_ortho();
	set_ctrl_viewport();
	set_color3f(0, 0, 0);
	batch_begin(GL_TRIANGLE_STRIP);
	batch_vertex3f(x1, 200 - y1, 0);
	batch_vertex3f(x2, 200 - y1, 0);
	batch_vertex3f(x1, 200 - y2, 0);
	batch_vertex3f(x2, 200 - y2, 0);
	batch_end_flat();
	set_main_viewport();
	pop_ortho();

	unsigned char *screen = (unsigned char *)PHYSCREEN + SCREENBYTES_LINE * y1;
	unsigned char *screen2 = (unsigned char *)LOGSCREEN + SCREENBYTES_LINE * y1;
	for (int y = y1; y < y2; y++)
	{
		for (int x = x1; x < x2; x++)
		{
			screen[x] = 255;
			screen2[x] = 255;
		}
		screen += SCREENBYTES_LINE;
		screen2 += SCREENBYTES_LINE;
	}
}

/* =========================================================================
 * Draw functions table
 * ========================================================================= */
typedef void (*NU_DRAWFUNC)(void **);
static NU_DRAWFUNC nu_drawfuncs[NU_MAX] = {
	NULL,
	&Nu_DrawTriangle,
	&Nu_DrawQuad,
	&Nu_DrawLine,
	&Nu_DrawBezierLine,
	&Nu_DrawTeardrop,
	&Nu_DrawComplexSNext,
	&Nu_DrawComplexStart,
	&Nu_DrawComplexEnd,
	&Nu_DrawComplexStartInner,
	&Nu_DrawComplexBezier,
	&Nu_DrawTwinklyCircle,
	&Nu_DrawPlanet,
	&Nu_DrawCircle,
	&Nu_DrawCylinder,
	&Nu_DrawBlob,
	&Nu_DrawOval,
	&Nu_DrawPoint,
	&Nu_Draw2DLine,
};

static void Nu_DrawPrimitive(void *data)
{
	for (;;)
	{
		int fnum = znode_rdlong(&data);
		if (!fnum)
			return;
		nu_drawfuncs[fnum](&data);
	}
}

static void draw_3dview(struct ZNode *root)
{
	if (!root)
		return;

	// explicit stack — max depth is log2(MAX_ZNODES) but use a safe upper bound
	struct ZNode *stack[MAX_ZNODES];
	int sp = 0;
	struct ZNode *node = root;

	// Morris-style iterative in-order traversal
	while (node || sp > 0)
	{
		// go as far 'more' as possible
		while (node)
		{
			stack[sp++] = node;
			node = node->more;
		}
		node = stack[--sp];
		if (use_renderer)
			Nu_DrawPrimitive(node->data);
		node = node->less;
	}
}

/* =========================================================================
 * draw_control_panel - 2D UI overlay
 * ========================================================================= */

void draw_hit_region(int x1, int y1, int x2, int y2)

{
	mat4 prev_proj = proj_matrix;

	float gy1 = 200.0f - (y2 * 200.0f / 240.0f); /* y2 maps to lower gl_y */
	float gy2 = 200.0f - (y1 * 200.0f / 240.0f); /* y1 maps to upper gl_y */

	set_color3f(1, 0, 0);
	batch_begin(GL_LINE_LOOP);
	batch_vertex3f(x1, gy1, 0);
	batch_vertex3f(x2, gy1, 0);
	batch_vertex3f(x2, gy2, 0);
	batch_vertex3f(x1, gy2, 0);
	batch_end_flat();

	proj_matrix = prev_proj;
}

/* =========================================================================
 * Screen-space ortho helpers for touch overlay drawing.
 * These push a projection covering the full SDL window (0..screen_w,
 * 0..screen_h) with Y=0 at the top, matching SDL pixel conventions so
 * touch-button positions calculated in touch_input.c map 1:1.
 * ========================================================================= */
static mat4 saved_proj_screen;
static mat4 saved_mv_screen;

static void push_screen_ortho(void)
{
	glViewport(0, 0, screen_w, screen_h);
	glDisable(GL_DEPTH_TEST);
	saved_proj_screen = proj_matrix;
	saved_mv_screen = *mv;
	/* Y axis flipped: top=0, bottom=screen_h */
	// proj_matrix = mat4_ortho(0, (float)screen_w, (float)screen_h, 0, -1, 1);
	proj_matrix = mat4_ortho(0, (float)screen_w, (float)screen_h, 0, -1, 1);
	mat_load_identity();
}

static void pop_screen_ortho(void)
{
	proj_matrix = saved_proj_screen;
	*mv = saved_mv_screen;
}

/* -------------------------------------------------------------------------
 * GL equivalents of the SDL touch icon draw helpers.
 * All coordinates are in window-pixel space (matches touch_input.c layout).
 * ------------------------------------------------------------------------- */

static void gl_draw_line(float x1, float y1, float x2, float y2)
{
	batch_begin(GL_LINES);
	batch_vertex3f(x1, y1, 0);
	batch_vertex3f(x2, y2, 0);
	batch_end_flat();
}

static void gl_draw_rect_outline(float x, float y, float w, float h)
{
	batch_begin(GL_LINE_LOOP);
	batch_vertex3f(x, y, 0);
	batch_vertex3f(x + w, y, 0);
	batch_vertex3f(x + w, y + h, 0);
	batch_vertex3f(x, y + h, 0);
	batch_end_flat();
}

static void gl_draw_circle_outline(float cx, float cy, float radius, int segments)
{
	batch_begin(GL_LINE_LOOP);
	for (int i = 0; i < segments; i++)
	{
		float theta = 2.0f * (float)M_PI * i / segments;
		batch_vertex3f(cx + cosf(theta) * radius, cy + sinf(theta) * radius, 0);
	}
	batch_end_flat();
}

/* Virtual joystick knob */
static void gl_render_virtual_joystick(void)
{
	if (!vjoy.active)
		return;
	set_color3ub(139, 137, 139);
	/* draw a few concentric outlines for thickness */
	for (int t = 0; t < 4; t++)
		gl_draw_circle_outline(vjoy.knob_x, vjoy.knob_y, vjoy.radius / 2.0f + t, 48);
}

/* Arrow icon (matches SDL draw_arrow_icon) */
static void gl_draw_arrow_icon(float x, float y, float w, float h,
							   SDL_Color col, int direction, int lineWidth)
{
	set_color3ub(col.r, col.g, col.b);

	float cx = x + w / 2.0f;
	float cy = y + h / 2.0f;
	float padding = fmaxf(2.0f, fminf(w, h) / 6.0f);
	float aw = fmaxf(3.0f, (w - 2 * padding) / 2.0f);
	float ah = fmaxf(3.0f, (h - 2 * padding) / 2.0f);

	for (int i = -lineWidth / 2; i <= lineWidth / 2; i++)
	{
		float o = (float)i;
		switch (direction)
		{
		case 0: /* Up */
			gl_draw_line(cx + o, y + h - padding, cx - aw + o, cy);
			gl_draw_line(cx + o, y + h - padding, cx + aw + o, cy);
			gl_draw_line(cx - aw + o, cy, cx + aw + o, cy);
			break;
		case 1: /* Right */
			gl_draw_line(x + padding, cy + o, cx, cy - ah + o);
			gl_draw_line(x + padding, cy + o, cx, cy + ah + o);
			gl_draw_line(cx, cy - ah + o, cx, cy + ah + o);
			break;
		case 2: /* Down */
			gl_draw_line(cx + o, y + padding, cx - aw + o, cy);
			gl_draw_line(cx + o, y + padding, cx + aw + o, cy);
			gl_draw_line(cx - aw + o, cy, cx + aw + o, cy);
			break;
		case 3: /* Left */
			gl_draw_line(x + w - padding + o, cy, cx, cy - ah + o);
			gl_draw_line(x + w - padding + o, cy, cx, cy + ah + o);
			gl_draw_line(cx, cy - ah + o, cx, cy + ah + o);
			break;
		}
	}
}

/* Thrust icon (matches SDL draw_thrust_icon) */
static void gl_draw_thrust_icon(float x, float y, float w, float h,
								SDL_Color col, int direction, int lineWidth)
{
	(void)lineWidth;
	set_color3ub(col.r, col.g, col.b);

	float cx = x + w / 2.0f;
	float cy = y + h / 2.0f;
	float padding = 6;
	float bfw = w / 4.0f;
	float bfl = h / 2.0f;
	float flameLengths[3] = {bfl, bfl * 0.8f, bfl * 0.6f};
	float spacing = bfw / 1.5f;

	for (int j = -1; j <= 1; j++)
	{
		float fl = flameLengths[j + 1];
		float p0x, p0y, p1x, p1y, p2x, p2y;
		switch (direction)
		{
		case 0:
		{ /* UP */
			float fx = cx + j * spacing, fy = y + padding;
			p0x = fx - bfw / 2;
			p0y = fy + fl;
			p1x = fx;
			p1y = fy;
			p2x = fx + bfw / 2;
			p2y = fy + fl;
			break;
		}
		case 1:
		{ /* RIGHT */
			float fx = x + w - padding - fl, fy = cy + j * spacing;
			p0x = fx;
			p0y = fy - bfw / 2;
			p1x = fx + fl;
			p1y = fy;
			p2x = fx;
			p2y = fy + bfw / 2;
			break;
		}
		case 2:
		{ /* DOWN */
			float fx = cx + j * spacing, fy = y + h - padding - fl;
			p0x = fx - bfw / 2;
			p0y = fy;
			p1x = fx;
			p1y = fy + fl;
			p2x = fx + bfw / 2;
			p2y = fy;
			break;
		}
		default:
		{ /* LEFT */
			float fx = x + padding, fy = cy + j * spacing;
			p0x = fx + fl;
			p0y = fy - bfw / 2;
			p1x = fx;
			p1y = fy;
			p2x = fx + fl;
			p2y = fy + bfw / 2;
			break;
		}
		}
		batch_begin(GL_LINE_STRIP);
		batch_vertex3f(p0x, p0y, 0);
		batch_vertex3f(p1x, p1y, 0);
		batch_vertex3f(p2x, p2y, 0);
		batch_vertex3f(p0x, p0y, 0);
		batch_end_flat();
	}
}

/* Burger menu icon */
static void gl_draw_burger_menu(float cx, float cy, float scale)
{
	float lineWidth = 15.0f * scale;
	float lineHeight = 4.0f * scale;
	float lineSpacing = 2.0f * scale;
	float totalHeight = 3 * lineHeight + 2 * lineSpacing;
	float startX = cx - lineWidth / 2;
	float startY = cy - totalHeight / 2;

	set_color3ub(255, 255, 255);
	for (int i = 0; i < 3; i++)
	{
		float yo = i * (lineHeight + lineSpacing);
		gl_draw_line(startX, startY + yo, startX + lineWidth, startY + yo);
	}
}

/* Arrow-touch (d-pad) icon */
static void gl_draw_arrow_touch_icon(float cx, float cy, float scale)
{
	float keySize = 8.0f * scale;
	float spacing = 2.0f * scale;
	float bx = cx - (3 * keySize + 2 * spacing) / 2.0f;
	float by = cy - (2 * keySize + spacing) / 2.0f;

	set_color3ub(255, 255, 255);
	gl_draw_rect_outline(bx + keySize + spacing, by, keySize, keySize);							  /* Up    */
	gl_draw_rect_outline(bx + keySize + spacing, by + keySize + spacing, keySize, keySize);		  /* Down  */
	gl_draw_rect_outline(bx, by + keySize + spacing, keySize, keySize);							  /* Left  */
	gl_draw_rect_outline(bx + 2 * (keySize + spacing), by + keySize + spacing, keySize, keySize); /* Right */
}

/* Zoom icon (+/-) */
static void gl_draw_zoom_icon(float cx, float cy, float radius, char symbol)
{
	const int segments = 32;
	const float lw = radius * 0.6f;

	set_color3ub(255, 255, 255);
	batch_begin(GL_LINE_LOOP);
	for (int i = 0; i < segments; i++)
	{
		float theta = 2.0f * (float)M_PI * i / segments;
		batch_vertex3f(cx + cosf(theta) * radius, cy + sinf(theta) * radius, 0);
	}
	batch_end_flat();

	if (symbol == '+')
		gl_draw_line(cx, cy - lw / 2, cx, cy + lw / 2);
	if (symbol == '+' || symbol == '-')
		gl_draw_line(cx - lw / 2, cy, cx + lw / 2, cy);
}

/* P icon */
static void gl_draw_p_icon(float cx, float cy, float scale)
{
	float stemH = 40.0f * scale;
	float loopH = 20.0f * scale;
	float loopW = 20.0f * scale;
	float sx = cx - loopW / 2;
	float sy = cy - stemH / 2;

	set_color3ub(255, 255, 255);
	gl_draw_line(sx, sy, sx, sy + stemH); /* spine */
	gl_draw_line(sx, sy, sx + loopW, sy); /* top   */
	gl_draw_line(sx, sy + loopH, sx + loopW, sy + loopH);
	gl_draw_line(sx + loopW, sy, sx + loopW, sy + loopH);
}

/* C icon */
static void gl_draw_c_icon(float cx, float cy, float scale)
{
	const float radius = 10.0f * scale;
	const int segments = 40;
	const float startAngle = (float)M_PI / 4.0f;
	const float endAngle = (float)M_PI * 7.0f / 4.0f;

	set_color3ub(255, 255, 255);
	batch_begin(GL_LINE_STRIP);
	for (int i = 0; i <= segments; i++)
	{
		float t = startAngle + (endAngle - startAngle) * i / segments;
		batch_vertex3f(cx + cosf(t) * radius, cy + sinf(t) * radius, 0);
	}
	batch_end_flat();
}

/* Cogwheel icon */
static void gl_draw_cogwheel_icon(float cx, float cy, float scale)
{
	const int toothCount = 8;
	const float outerRadius = 16.0f * scale;
	const float innerRadius = 10.0f * scale;
	const float centerHoleRadius = 4.0f * scale;
	const float toothW = (float)M_PI / (toothCount * 2);
	const float full = 2.0f * (float)M_PI;
	const int segments = 40;

	set_color3ub(255, 255, 255);

	for (int i = 0; i < toothCount; i++)
	{
		float angle = i * (full / toothCount);
		float a1 = angle - toothW, a2 = angle + toothW;
		float ox1 = cx + cosf(a1) * outerRadius, oy1 = cy + sinf(a1) * outerRadius;
		float ox2 = cx + cosf(a2) * outerRadius, oy2 = cy + sinf(a2) * outerRadius;
		float ix1 = cx + cosf(a1) * innerRadius, iy1 = cy + sinf(a1) * innerRadius;
		float ix2 = cx + cosf(a2) * innerRadius, iy2 = cy + sinf(a2) * innerRadius;
		gl_draw_line(ix1, iy1, ox1, oy1);
		gl_draw_line(ix2, iy2, ox2, oy2);
		gl_draw_line(ox1, oy1, ox2, oy2);
	}
	gl_draw_circle_outline(cx, cy, innerRadius, segments);
	gl_draw_circle_outline(cx, cy, centerHoleRadius, segments);

	float left   = cx - outerRadius;
	float right  = cx + outerRadius;
	float top    = cy - outerRadius;
	float bottom = cy + outerRadius;
}

/* Shoot button circle */
static void gl_draw_shoot_button(void)
{
	float cx = shoot_button.x + shoot_button.width / 2.0f;
	float cy = shoot_button.y + shoot_button.height / 2.0f;
	float r = shoot_button.width / 4.0f;

	set_color3ub(255, 0, 0);
	gl_draw_circle_outline(cx, cy, r, 32);

	set_color3ub(shoot_button.color.r, shoot_button.color.g, shoot_button.color.b);
	gl_draw_rect_outline(shoot_button.x, shoot_button.y,
						 shoot_button.width, shoot_button.height);
}

/* Rocket nozzle thruster icon.
 * cx,cy = centre of the button hit area.
 * sz    = button width (used to scale everything).
 * dir   = 0 up-thrust, 1 right-thrust, 2 down-thrust, 3 left-thrust.
 * The nozzle bell opens in the direction of thrust; the exhaust cone
 * fans out beyond it with concentric glow rings fading outward. */
static void gl_draw_nozzle_thruster(float cx, float cy, float sz, int dir)
{
	/* All geometry defined for dir==0 (thrust upward, nozzle opens down),
	 * then rotated by mapping axes for other directions. */
	float s = sz * 0.42f;  /* half-width of nozzle throat */
	float nl = sz * 0.35f; /* nozzle chamber length (above throat) */
	float el = sz * 0.55f; /* exhaust cone length (below throat) */
	float ew = sz * 0.62f; /* exhaust cone half-width at tip */

	/* Rotation helpers: rotate vector (dx,dy) by dir*90° around centre */
#define ROT(dx, dy, rx, ry)   \
	do                        \
	{                         \
		switch (dir)          \
		{                     \
		case 0:               \
			(rx) = cx + (dx); \
			(ry) = cy + (dy); \
			break;            \
		case 1:               \
			(rx) = cx + (dy); \
			(ry) = cy - (dx); \
			break;            \
		case 2:               \
			(rx) = cx - (dx); \
			(ry) = cy - (dy); \
			break;            \
		default:              \
			(rx) = cx - (dy); \
			(ry) = cy + (dx); \
			break;            \
		}                     \
	} while (0)

	float ax, ay, bx, by, ex, ey;

	/* --- nozzle bell outline (chamber + throat taper) --- */
	/* chamber top-left to top-right */
	set_color3ub(200, 220, 255);
	ROT(-s * 0.7f, -nl, ax, ay);
	ROT(s * 0.7f, -nl, bx, by);
	gl_draw_line(ax, ay, bx, by);
	/* left side of chamber */
	ROT(-s * 0.7f, -nl, ax, ay);
	ROT(-s, 0, bx, by);
	gl_draw_line(ax, ay, bx, by);
	/* right side of chamber */
	ROT(s * 0.7f, -nl, ax, ay);
	ROT(s, 0, bx, by);
	gl_draw_line(ax, ay, bx, by);
	/* throat (the narrowest part = centre line) */
	ROT(-s, 0, ax, ay);
	ROT(s, 0, bx, by);
	gl_draw_line(ax, ay, bx, by);
	/* diverging nozzle walls */
	ROT(-s, 0, ax, ay);
	ROT(-ew * 0.55f, el * 0.6f, bx, by);
	gl_draw_line(ax, ay, bx, by);
	ROT(s, 0, ax, ay);
	ROT(ew * 0.55f, el * 0.6f, bx, by);
	gl_draw_line(ax, ay, bx, by);

	/* --- exhaust cone with 3 fading glow rings --- */
	struct
	{
		float dist;
		unsigned char r, g, b;
	} rings[3] = {
		{el * 0.30f, 255, 200, 80},
		{el * 0.55f, 255, 120, 30},
		{el * 0.85f, 200, 60, 10},
	};
	for (int ri = 0; ri < 3; ri++)
	{
		float d = rings[ri].dist;
		float hw = ew * (d / el) * 0.85f; /* half-width at this distance */
		set_color3ub(rings[ri].r, rings[ri].g, rings[ri].b);
		ROT(-hw, d, ax, ay);
		ROT(hw, d, bx, by);
		gl_draw_line(ax, ay, bx, by);
		/* short diagonal ticks at ends of ring */
		ROT(-hw, d, ax, ay);
		ROT(-hw * 0.7f, d * 0.85f, ex, ey);
		gl_draw_line(ax, ay, ex, ey);
		ROT(hw, d, ax, ay);
		ROT(hw * 0.7f, d * 0.85f, ex, ey);
		gl_draw_line(ax, ay, ex, ey);
	}
	/* tip dot */
	set_color3ub(255, 240, 120);
	ROT(0, el, ax, ay);
	batch_begin(GL_POINTS);
	batch_vertex3f(ax, ay, 0);
	batch_end_flat();

#undef ROT
}

/* Main GL touch controls draw — mirrors draw_touch_controls() in rendererSDL.c */
static void draw_touch_controls_gl(void)
{
	push_screen_ortho();

	gl_render_virtual_joystick();

	if (toggle_arrow_keys_touch)
	{
		for (int i = 0; i < (int)(sizeof(arrow_buttons) / sizeof(arrow_buttons[0])); i++)
		{
			float bx = arrow_buttons[i].x;
			float by = arrow_buttons[i].y;
			float bw = arrow_buttons[i].width;
			float bh = arrow_buttons[i].height;
			/* direction mapping: 0=down,1=left,2=up,3=right stored in struct order */
			int dirs[4] = {0, 1, 2, 3}; /* draw arrow pointing: down,left,up,right */
			gl_draw_arrow_icon(bx, by, bw, bh, arrow_buttons[i].color, dirs[i], 2);
		}
	}

	if (toggle_thrust_keys_touch)
	{
		/* draw each thruster using its own button rect */
		for (int i = 0; i < (int)(sizeof(thrust_buttons) / sizeof(thrust_buttons[0])); i++)
		{
			float bx = thrust_buttons[i].x;
			float by = thrust_buttons[i].y;
			float bw = thrust_buttons[i].width;
			float cx = bx + bw / 2.0f;
			float cy = by + thrust_buttons[i].height / 2.0f;
			int ndir[4] = {2, 1, 0, 3};
			gl_draw_nozzle_thruster(cx, cy, bw, ndir[i]);
		}

		gl_draw_shoot_button();
	}

	/* Dropdown buttons */
	for (int i = 0; i < (int)(sizeof(dropdown_buttons) / sizeof(dropdown_buttons[0])); i++)
	{
		if (!toggle_dropdown_keys_touch && i != 0)
			continue;

		SDL_Color col = dropdown_buttons[i].color;
		set_color3ub(col.r, col.g, col.b);
		gl_draw_rect_outline(dropdown_buttons[i].x, dropdown_buttons[i].y,
							 dropdown_buttons[i].width, dropdown_buttons[i].height);

		float bx = dropdown_buttons[i].x + dropdown_buttons[i].width / 2.0f;
		float by = dropdown_buttons[i].y + dropdown_buttons[i].height / 2.0f;

		gl_draw_burger_menu(dropdown_buttons[0].x + dropdown_buttons[0].width / 2.0f,
							dropdown_buttons[0].y + dropdown_buttons[0].height / 2.0f, 1);
		if (i == 1)
			gl_draw_arrow_touch_icon(bx, by, 1);
		else if (i == 2)
			gl_draw_zoom_icon(bx, by, 12, '+');
		else if (i == 3)
			gl_draw_zoom_icon(bx, by, 12, '-');
		else if (i == 4)
			gl_draw_p_icon(bx, by, 0.5f);
		else if (i == 5)
			gl_draw_c_icon(bx, by, 1);
		else if (i == 6)
			gl_draw_cogwheel_icon(bx, by, 0.8f);
	}

	pop_screen_ortho();
}

static void draw_on_top_of_screen(void)
{
	if (toggle_touch_controls)
	{
		draw_touch_controls_gl();
	}
	else
	{
		gl_draw_cogwheel_icon(5, 194, 0.3f);
	}

#ifdef USE_NK
	if (toggle_m68k_menu)
		render_nuklear();
#endif

	// draw_hit_region(18, 226, 31, 240); // f2
}

static void draw_control_panel(void)
{
	int x, y;
	unsigned int *pal;

	set_ctrl_viewport();

	/* star-system name hack */
	y = logscreen2;
	logscreen2 = physcreen;
	for (x = 0; x < queued_string_pos; x++)
		DrawStr(queued_strings[x].x, queued_strings[x].y,
				queued_strings[x].col, queued_strings[x].str, FALSE);
	logscreen2 = y;

	/* black bar at the very bottom */
	push_ortho();
	set_color3f(0, 0, 0);
	batch_begin(GL_TRIANGLE_STRIP);
	batch_vertex3f(0, 32, 0);
	batch_vertex3f(319, 32, 0);
	batch_vertex3f(0, 0, 0);
	batch_vertex3f(319, 0, 0);
	batch_end_flat();

	/* Build the entire 320x200 RGBA image in one CPU-side buffer, then upload once */
	static unsigned int framebuf[320 * 200]; // static: no stack pressure

	uint8_t *src = VideoRaster; // use indexed access like SDL version
	for (y = 0; y < 200; y++)
	{
		pal = (y < 168) ? MainRGBPalette : CtrlRGBPalette;
		for (x = 0; x < 320; x++)
		{
			uint8_t idx = src[y * 320 + x]; // indexed, not pointer-walked
			framebuf[y * 320 + x] = (idx == 255) ? 0 : pal[idx];
		}
	}

	/* Single upload — one wasm→JS boundary crossing instead of 200 */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, screen_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 320, 200,
					GL_RGBA, GL_UNSIGNED_BYTE, framebuf);

	float u1 = 320.0f / SCR_TEX_W, v1 = 200.0f / SCR_TEX_H;
	bool blend = (use_renderer != R_OLD);
	draw_tex_quad(0, 0, 320, 200, 0, 0, u1, v1, blend);

	glBindTexture(GL_TEXTURE_2D, 0);
	draw_on_top_of_screen();
	pop_ortho();
}

/* =========================================================================
 * Nu_DrawScreen - called once per frame
 * ========================================================================= */
static void set_gl_clear_col(int rgb)
{
	float r = (rgb & 0xff) / 255.0f;
	float g = ((rgb >> 8) & 0xff) / 255.0f;
	float b = ((rgb >> 16) & 0xff) / 255.0f;
	glClearColor(r, g, b, 0);
}

void Nu_DrawScreen(void)
{
#if defined(ANDROID) || defined(__EMSCRIPTEN__)
	// {
	// 	static Uint32 fps_last = 0;
	// 	static int    fps_count = 0;
	// 	fps_count++;
	// 	Uint32 now = SDL_GetTicks();
	// 	if (now - fps_last >= 5000)
	// 	{
	// 		float fps = fps_count * 1000.0f / (float)(now - fps_last + 1);
	// 		// log_printf("FPS=%.1f znodes=%d objdata=%d", fps, znode_buf_pos, obj_data_pos);
	// 		fps_last  = now;
	// 		fps_count = 0;
	// 	}
	// }
#endif
	tess_arena_pos = 0;

	// Build palettes ONCE
	_BuildRGBPalette(MainRGBPalette, MainPalette, len_main_palette);
	_BuildRGBPalette(CtrlRGBPalette, CtrlPalette, 16);

	glViewport(0, 0, screen_w, screen_h);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (use_renderer == R_GLWIRE)
		glClearColor(0, 0, 0, 0);
	else
		set_gl_clear_col(MainRGBPalette[fe2_bgcol]);

	glViewport(lb_x, lb_y, lb_w, lb_h);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	mat_load_identity();
	set_main_viewport();

	if (znode_cur)
		end_node();
	draw_3dview(znode_start);

	if (mouse_shown)
	{
		SDL_ShowCursor(SDL_ENABLE);
		mouse_shown = 0;
	}
	else
		SDL_ShowCursor(SDL_DISABLE);

	draw_control_panel();
	glFlush();
	SDL_GL_SwapWindow(window);
}

#endif /* WITH_GL */