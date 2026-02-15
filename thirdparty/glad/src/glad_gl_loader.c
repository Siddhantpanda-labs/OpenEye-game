/**
 * Glad 2 header-only loader: single translation unit that instantiates the
 * implementation and provides a global context + gladLoadGL for compatibility.
 */
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

static GladGLContext g_glContext;

GLAD_API_CALL GladGLContext* glad_gl_get_context(void) {
    return &g_glContext;
}

GLAD_API_CALL int gladLoadGL(GLADloadfunc load) {
    return gladLoadGLContext(&g_glContext, load);
}
