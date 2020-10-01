#pragma once
#ifndef _OPENVR_INTERFACE_GLEXT

#ifdef GLEXT_IMPL
#define GET_GL_EXT_PTR(fn) { *(uintptr_t *)&fn = (uintptr_t)wglGetProcAddress(#fn); if (!fn) { return false; } }
#define GLEXT_API
#else
#define GLEXT_API extern
#endif
#define GLEXT_APICALL WINAPI

#define GL_FRAMEBUFFER_COMPLETE				0x8CD5
#define GL_COLOR_ATTACHMENT0				0x8CE0
#define GL_FRAMEBUFFER						0x8D40
#define GL_FRAMEBUFFER_BINDING				0x8CA6

GLEXT_API void (GLEXT_APICALL *glGenFramebuffers)(GLsizei n, GLuint * framebuffers);
GLEXT_API void (GLEXT_APICALL *glBindFramebuffer)(GLenum target, GLuint framebuffer);
GLEXT_API void (GLEXT_APICALL *glDeleteFramebuffers)(GLsizei n, const GLuint * framebuffers);
GLEXT_API void (GLEXT_APICALL *glGenRenderbuffers)(GLsizei n, GLuint * renderbuffers);
GLEXT_API void (GLEXT_APICALL *glDeleteRenderbuffers)(GLsizei n, const GLuint * renderbuffers);
GLEXT_API void (GLEXT_APICALL *glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLEXT_API GLenum (GLEXT_APICALL *glCheckFramebufferStatus)(GLenum target);

#ifdef GLEXT_IMPL
static bool assign_glext_function_pointers()
{
	GET_GL_EXT_PTR(glGenFramebuffers);
	GET_GL_EXT_PTR(glBindFramebuffer);
	GET_GL_EXT_PTR(glDeleteFramebuffers);
	GET_GL_EXT_PTR(glGenRenderbuffers);
	GET_GL_EXT_PTR(glDeleteRenderbuffers);
	GET_GL_EXT_PTR(glFramebufferTexture2D);
	GET_GL_EXT_PTR(glCheckFramebufferStatus);

	return true;
}
#endif

#endif //_OPENVR_INTERFACE_GLEXT
