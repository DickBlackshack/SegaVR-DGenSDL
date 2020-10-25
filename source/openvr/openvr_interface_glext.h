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

#define GL_ARRAY_BUFFER						0x8892
#define GL_ELEMENT_ARRAY_BUFFER				0x8893
#define GL_STATIC_DRAW						0x88E4

typedef intptr_t GLintptr;
typedef size_t GLsizeiptr;

GLEXT_API void (GLEXT_APICALL *glGenFramebuffers)(GLsizei n, GLuint * framebuffers);
GLEXT_API void (GLEXT_APICALL *glBindFramebuffer)(GLenum target, GLuint framebuffer);
GLEXT_API void (GLEXT_APICALL *glDeleteFramebuffers)(GLsizei n, const GLuint * framebuffers);
GLEXT_API void (GLEXT_APICALL *glGenRenderbuffers)(GLsizei n, GLuint * renderbuffers);
GLEXT_API void (GLEXT_APICALL *glDeleteRenderbuffers)(GLsizei n, const GLuint * renderbuffers);
GLEXT_API void (GLEXT_APICALL *glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLEXT_API GLenum (GLEXT_APICALL *glCheckFramebufferStatus)(GLenum target);

GLEXT_API void (GLEXT_APICALL *glBindBuffer)(GLenum target, GLuint buffer);
GLEXT_API void (GLEXT_APICALL *glDeleteBuffers)(GLsizei n, const GLuint *buffers);
GLEXT_API void (GLEXT_APICALL *glGenBuffers)(GLsizei n, GLuint *buffers);
GLEXT_API void (GLEXT_APICALL *glBufferData)(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
GLEXT_API void (GLEXT_APICALL *glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);


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

	GET_GL_EXT_PTR(glBindBuffer);
	GET_GL_EXT_PTR(glDeleteBuffers);
	GET_GL_EXT_PTR(glGenBuffers);
	GET_GL_EXT_PTR(glBufferData);
	GET_GL_EXT_PTR(glBufferSubData);

	return true;
}
#endif

#endif //_OPENVR_INTERFACE_GLEXT
