/**
 * Compatibility header for Glad 2 (header-only, MX): includes gl.h and
 * provides gl* macros and gladLoadGL so existing code using glad/glad.h
 * and gladLoadGLLoader continues to work.
 */
#ifndef GLAD_GLAD_H_
#define GLAD_GLAD_H_

#include <glad/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global context getter and loader (implemented in glad_gl_loader.c) */
GLAD_API_CALL GladGLContext* glad_gl_get_context(void);
GLAD_API_CALL int gladLoadGL(GLADloadfunc load);

#ifdef __cplusplus
}
#endif

/* Compatibility: old Glad 1 used gladLoadGLLoader(GLADloadproc). */
#define gladLoadGLLoader(load) gladLoadGL((GLADloadfunc)(load))

/* Runtime extension checks (context holds int ARB_bindless_texture etc.) */
#define GLAD_GL_ARB_bindless_texture   (glad_gl_get_context()->ARB_bindless_texture)
#define GLAD_GL_NV_shader_buffer_load (glad_gl_get_context()->NV_shader_buffer_load)
#define GLAD_GL_ARB_texture_barrier   (glad_gl_get_context()->ARB_texture_barrier)
#define GLAD_GL_ARB_buffer_storage    (glad_gl_get_context()->ARB_buffer_storage)

/* gl* function macros (context-based) */
#include <glad/glad_gl_aliases.h>

#endif /* GLAD_GLAD_H_ */
