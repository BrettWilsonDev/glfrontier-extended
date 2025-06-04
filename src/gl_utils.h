#ifndef GL_UTILS_H
#define GL_UTILS_H

// #include <GL/gl.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to compute normals
void computeNormal(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat len = sqrtf(x * x + y * y + z * z);
    if (len > 0.0f)
        glNormal3f(x / len, y / len, z / len);
    else
        glNormal3f(0.0f, 0.0f, 1.0f);
}

void drawCylinder(GLdouble baseRadius, GLdouble topRadius, GLdouble height,
                  GLint slices, GLint stacks)
{
    GLdouble da = 2.0 * M_PI / slices;
    GLdouble dr = (topRadius - baseRadius) / stacks;
    GLdouble dz = height / stacks;
    GLfloat du = 1.0f / (GLfloat)slices;
    GLfloat dv = 1.0f / (GLfloat)stacks;
    GLfloat nz = (baseRadius - topRadius) / height;
    GLfloat tcx = 0.0f, tcy = 0.0f;
    GLfloat z, r;

    for (int i = 0; i < slices; i++) {
        GLfloat angle1 = i * da;
        GLfloat angle2 = (i + 1) * da;
        GLfloat x1 = -sinf(angle1);
        GLfloat y1 = cosf(angle1);
        GLfloat x2 = -sinf(angle2);
        GLfloat y2 = cosf(angle2);

        z = 0.0f;
        r = baseRadius;
        tcy = 0.0f;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= stacks; j++) {
            // Compute normals
            computeNormal(x1, y1, nz);
            glTexCoord2f(tcx, tcy);
            glVertex3f(x1 * r, y1 * r, z);

            computeNormal(x2, y2, nz);
            glTexCoord2f(tcx + du, tcy);
            glVertex3f(x2 * r, y2 * r, z);

            z += dz;
            r += dr;
            tcy += dv;
        }
        glEnd();

        tcx += du;
    }
}

void drawDisk(GLdouble innerRadius, GLdouble outerRadius,
              GLint slices, GLint loops)
{
    GLdouble a, da;
    GLfloat dr;
    GLfloat r1, r2, dtc;
    GLint s, l;
    GLfloat sa, ca;

    glNormal3f(0.0, 0.0, +1.0);

    da = 2.0 * M_PI / slices;
    dr = (outerRadius - innerRadius) / (GLfloat) loops;
    dtc = 2.0f * outerRadius;

    r1 = innerRadius;
    for (l = 0; l < loops; l++) {
        r2 = r1 + dr;
        glBegin(GL_QUAD_STRIP);
        for (s = 0; s <= slices; s++) {
            a = (s == slices) ? 0.0 : s * da;
            sa = sin(a);
            ca = cos(a);
            glTexCoord2f(0.5f + sa * r2 / dtc, 0.5f + ca * r2 / dtc);
            glVertex2f(r2 * sa, r2 * ca);
            glTexCoord2f(0.5f + sa * r1 / dtc, 0.5f + ca * r1 / dtc);
            glVertex2f(r1 * sa, r1 * ca);
        }
        glEnd();
        r1 = r2;
    }
}


void makePerspectiveMatrix(float fovy, float aspect, float zNear, float zFar)
{
    GLdouble ymax = zNear * tan(fovy * M_PI / 360.0);
    GLdouble ymin = -ymax;
    GLdouble xmin = ymin * aspect;
    GLdouble xmax = ymax * aspect;

    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

// static void multMatrixVec(const GLdouble m[16], const GLdouble in[4], GLdouble out[4]) {
// 	for (int i = 0; i < 4; i++) {
// 		out[i] = m[i * 4 + 0] * in[0] +
// 		         m[i * 4 + 1] * in[1] +
// 		         m[i * 4 + 2] * in[2] +
// 		         m[i * 4 + 3] * in[3];
// 	}
// }

int gl_project(GLdouble objx, GLdouble objy, GLdouble objz,
               const GLdouble model[16], const GLdouble proj[16],
               const GLint viewport[4],
               GLdouble *winx, GLdouble *winy, GLdouble *winz)
{
    GLdouble in[4] = { objx, objy, objz, 1.0 };
    GLdouble mv[4], clip[4];

    // model * in (column-major)
    for (int row = 0; row < 4; row++) {
        mv[row] = model[0 * 4 + row] * in[0] +
                  model[1 * 4 + row] * in[1] +
                  model[2 * 4 + row] * in[2] +
                  model[3 * 4 + row] * in[3];
    }

    // proj * mv (column-major)
    for (int row = 0; row < 4; row++) {
        clip[row] = proj[0 * 4 + row] * mv[0] +
                    proj[1 * 4 + row] * mv[1] +
                    proj[2 * 4 + row] * mv[2] +
                    proj[3 * 4 + row] * mv[3];
    }

    if (clip[3] == 0.0)
        return 0;

    // perspective division
    clip[0] /= clip[3];
    clip[1] /= clip[3];
    clip[2] /= clip[3];

    // Map x,y,z to range 0..1
    clip[0] = clip[0] * 0.5 + 0.5;
    clip[1] = clip[1] * 0.5 + 0.5;
    clip[2] = clip[2] * 0.5 + 0.5;

    // Map to viewport
    *winx = viewport[0] + clip[0] * viewport[2];
    *winy = viewport[1] + clip[1] * viewport[3];
    *winz = clip[2];

    return 1;
}

#endif // GL_UTILS_H