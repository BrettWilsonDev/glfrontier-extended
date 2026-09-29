/*
 * gl_api.h - single place that pulls in the OpenGL headers.
 *
 * Desktop: OpenGL 3.3 core via glad.  Android / Web: OpenGL ES 3.0.
 */
#ifndef GL_API_H
#define GL_API_H

#if defined(__EMSCRIPTEN__) || defined(ANDROID)
#include <GLES3/gl3.h>
#define GL_IS_GLES 1
#else
#include "glad/glad.h"
#define GL_IS_GLES 0
#endif

#endif /* GL_API_H */
