#include <windows.h>
#include <gl/GL.h>
#include <assert.h>
#include <algorithm>
#include <math.h>
#ifndef M_PI
	#define M_PI 3.141592653589793238462643383279502884196
#endif
#include "openvr.h"
#include "openvr_interface.h"

#define GLEXT_IMPL
#include "openvr_interface_glext.h"

namespace
{
	SOVRInterface sOvrInterface;
	vr::IVRSystem *spHMD = NULL;

	bool sValidFBO = false;
	GLuint sLeftEyeFBO = 0;
	GLuint sLeftEyeTexture = 0;
	GLuint sRightEyeFBO = 0;
	GLuint sRightEyeTexture = 0;

	uint32_t sEyeTargetWidth = 0;
	uint32_t sEyeTargetHeight = 0;

	GLuint sEyeTempTexture = 0;
	uint32_t sEyeTempWidth = 0;
	uint32_t sEyeTempHeight = 0;
	uint8_t *pEyeTempBuffer = NULL;

	float sAspectCrunch = 1.0f;
	float sImagePerspectiveScale = 0.25f;

	float sEyeOffsetX = 0.0f;
	float sEyeOffsetY = 0.0f;

	const GLenum skTextureSourceFormat = GL_BGRA_EXT;

	float sPitchYaw[2] = { 0.0f, 0.0f };

	const double skDegToRad = M_PI / 180.0;
	const double skRadToDeg = 180.0 / M_PI;
	const float skDegToRadf = (float)skDegToRad;
	const float skRadToDegf = (float)skRadToDeg;

	bool sSimultaneousEyeUpdates = true;
	bool sBilinearFiltering = false;

	vr::TrackedDevicePose_t sTrackedDevicePoses[vr::k_unMaxTrackedDeviceCount];

	void init_eye_texture(GLuint &eyeTexture, const uint32_t width, const uint32_t height, const uint32_t sourceFormat)
	{
		const GLint filterMode = (sBilinearFiltering) ? GL_LINEAR : GL_NEAREST;
		glGenTextures(1, &eyeTexture);
		glBindTexture(GL_TEXTURE_2D, eyeTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMode);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, sourceFormat, GL_UNSIGNED_BYTE, NULL);
	}

	GLenum init_eye_fbo(GLuint &eyeFBO, GLuint &eyeTexture)
	{
		glGenFramebuffers(1, &eyeFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, eyeFBO);

		init_eye_texture(eyeTexture, sEyeTargetWidth, sEyeTargetHeight, GL_RGBA);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTexture, 0);
		return glCheckFramebufferStatus(GL_FRAMEBUFFER);
	}

	void draw_eye_texture_to_viewport(const bool isLeft, const uint32_t eyeFbo)
	{
		//using the archaic arts feels a bit forbidden, it might be less evil to pass through the dimensions we want to restore for dgen
		glPushAttrib(GL_VIEWPORT_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, eyeFbo);
		glViewport(0, 0, sEyeTargetWidth, sEyeTargetHeight);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, sEyeTempTexture);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glMatrixMode(GL_PROJECTION);
		glPushMatrix(); //dgen will need this restored

		//you might think that you'd simply be able to present the image to each eye, but often times the eye is off-center.
		//this means we have to use the raw projection provided by openvr.
		float left, right, top, bottom;
		spHMD->GetProjectionRaw((isLeft) ? vr::Eye_Left : vr::Eye_Right, &left, &right, &top, &bottom);

		//now we use the raw values from the lib to create an orthographic projection matrix
		const float zNear = -1.0f;
		const float zFar = 1.0f;
		float projMat[16];
		
		projMat[0] = 2.0f / (right - left);
		projMat[1] = projMat[2] = projMat[3] = 0.0f;

		projMat[5] = 2.0f / (top - bottom);
		projMat[4] = projMat[6] = projMat[7] = 0.0f;

		projMat[10] = -2.0f / (zFar - zNear);
		projMat[8] = projMat[9] = projMat[11] = 0.0f;

		projMat[12] = -((right + left) / (right - left));
		projMat[13] = -((top + bottom) / (top - bottom));
		projMat[14] = -((zFar + zNear) / (zFar - zNear));
		projMat[15] = 1.0f;

		glLoadMatrixf(projMat);

		const float qLeft = sEyeOffsetX - sImagePerspectiveScale;
		const float qRight = sEyeOffsetX + sImagePerspectiveScale;
		const float qTop = sEyeOffsetY - sImagePerspectiveScale * sAspectCrunch;
		const float qBottom = sEyeOffsetY + sImagePerspectiveScale * sAspectCrunch;

		//i know no one wants you around anymore, immediate mode, but i still love you
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3f(qLeft, qTop, 0.0f);
			glTexCoord2f(1.0f, 0.0f);
			glVertex3f(qRight, qTop, 0.0f);
			glTexCoord2f(1.0f, 1.0f);
			glVertex3f(qRight, qBottom, 0.0f);
			glTexCoord2f(0.0f, 1.0f);
			glVertex3f(qLeft, qBottom, 0.0f);
		glEnd();

		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		glDisable(GL_TEXTURE_2D);

		glPopAttrib();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		assert(glGetError() == GL_NO_ERROR);
	}

	void copy_to_temp_buffer(uint8_t *pDst, const uint8_t *pSrc, const uint32_t width, const uint32_t height, const uint32_t stride)
	{
		uint8_t *pDstRow = pDst;
		const uint8_t *pSrcRow = pSrc;
		const uint32_t dstStride = width << 2;
		assert(dstStride <= stride);
		for (uint32_t y = 0; y < height; ++y)
		{
			memcpy(pDstRow, pSrcRow, dstStride);
			pSrcRow += stride;
			pDstRow += dstStride;
		}
	}
}

#ifdef _DEBUG
#define OVRI_VERIFY(c) { auto res = c; assert(res == vr::VRCompositorError_None); }
#else
#define OVRI_VERIFY(c) c
#endif

void OVR_Shutdown()
{
	if (spHMD)
	{
		vr::VR_Shutdown();
		spHMD = NULL;
	}
	if (sValidFBO)
	{
		glDeleteFramebuffers(1, &sLeftEyeFBO);
		glDeleteTextures(1, &sLeftEyeTexture);
		glDeleteFramebuffers(1, &sRightEyeFBO);
		glDeleteTextures(1, &sRightEyeTexture);
		sValidFBO = false;
	}
	if (pEyeTempBuffer)
	{
		free(pEyeTempBuffer);
		pEyeTempBuffer = NULL;
		glDeleteTextures(1, &sEyeTempTexture);
	}
}

bool OVR_ProvideEyeImage(const bool isLeft, const uint8_t *pData, const uint32_t bpp, const uint32_t width, const uint32_t height, const uint32_t stride)
{
	if (bpp != 32)
	{
		//just expect this to be consistent when a headset is present
		return false;
	}

	if (sEyeTempWidth != width || sEyeTempHeight != height)
	{
		if (pEyeTempBuffer)
		{
			free(pEyeTempBuffer);
			glDeleteTextures(1, &sEyeTempTexture);
		}
		pEyeTempBuffer = (uint8_t *)malloc(width * height * 4);
		init_eye_texture(sEyeTempTexture, width, height, skTextureSourceFormat);
		sEyeTempWidth = width;
		sEyeTempHeight = height;
	}

	if (!sSimultaneousEyeUpdates)
	{
		glBindTexture(GL_TEXTURE_2D, sEyeTempTexture);
		copy_to_temp_buffer(pEyeTempBuffer, pData, sEyeTempWidth, sEyeTempHeight, stride);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sEyeTempWidth, sEyeTempHeight, skTextureSourceFormat, GL_UNSIGNED_BYTE, pEyeTempBuffer);
		draw_eye_texture_to_viewport(isLeft, (isLeft) ? sLeftEyeFBO : sRightEyeFBO);
	}
	else
	{
		if (isLeft)
		{
			copy_to_temp_buffer(pEyeTempBuffer, pData, sEyeTempWidth, sEyeTempHeight, stride);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, sEyeTempTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sEyeTempWidth, sEyeTempHeight, skTextureSourceFormat, GL_UNSIGNED_BYTE, pEyeTempBuffer);
			draw_eye_texture_to_viewport(true, sLeftEyeFBO);

			copy_to_temp_buffer(pEyeTempBuffer, pData, sEyeTempWidth, sEyeTempHeight, stride);
			glBindTexture(GL_TEXTURE_2D, sEyeTempTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sEyeTempWidth, sEyeTempHeight, skTextureSourceFormat, GL_UNSIGNED_BYTE, pEyeTempBuffer);
			draw_eye_texture_to_viewport(false, sRightEyeFBO);
		}
	}

	return true;
}

void OVR_GetPoses()
{
	OVRI_VERIFY(vr::VRCompositor()->WaitGetPoses(sTrackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0));
	const vr::TrackedDevicePose_t &hmdPose = sTrackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd];
	if (hmdPose.bPoseIsValid)
	{
		//yank out pitch/yaw to feed back to sega vr
		const vr::HmdMatrix34_t &hmdMat = hmdPose.mDeviceToAbsoluteTracking;
		const float sp = std::min<float>(std::max<float>(hmdMat.m[1][2], -1.0f), 1.0f);
		const float theta = asinf(sp);
		sPitchYaw[0] = -theta * skRadToDegf;
		sPitchYaw[1] = atan2f(hmdMat.m[0][2], hmdMat.m[0][0]) * skRadToDegf;
	}
}

void OVR_SubmitEyes()
{
	//we're presenting our own stereo "projections" and don't want any smoothing (based on a projection we're not using) applied.
	const vr::EVRSubmitFlags kSubmitFlags = vr::Submit_FrameDiscontinuty;

	vr::Texture_t leftEyeTexture = { (void *)sLeftEyeTexture, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	OVRI_VERIFY(vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture, NULL, kSubmitFlags));
	vr::Texture_t rightEyeTexture = { (void *)sRightEyeTexture, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
	OVRI_VERIFY(vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture, NULL, kSubmitFlags));
}

void OVR_GetHMDPitchYaw(float *pAnglesOut)
{
	pAnglesOut[0] = sPitchYaw[0];
	pAnglesOut[1] = sPitchYaw[1];
}

void OVR_DrawEyes(const uint32_t windowWidth, const uint32_t windowHeight)
{
	//just stretch the eyes over the whole view
	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0, 0, windowWidth, windowHeight);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, sLeftEyeTexture);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(0.5f, 0.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(0.5f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, sRightEyeTexture);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(0.5f, 0.0f, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, 0.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(0.5f, 1.0f, 0.0f);
	glEnd();

	glDisable(GL_TEXTURE_2D);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopAttrib();
}

void OVR_PostSwapBuffers()
{
	//sync here if necessary
}

void OVR_SetSimultaneousEyeUpdates(const bool enabled)
{
	sSimultaneousEyeUpdates = enabled;
}

void OVR_SetBilinearFiltering(const bool enabled)
{
	sBilinearFiltering = enabled;
}

SOVRInterface *OpenVR_Interface_Init(const float eyeOffsetX, const float eyeOffsetY, const uint32_t eyeTargetWidth, const uint32_t eyeTargetHeight, const float idealAspect, const float imagePerspectiveScale)
{
	if (!assign_glext_function_pointers())
	{
		return NULL;
	}

	vr::EVRInitError eError = vr::VRInitError_None;
	spHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

	if (eError != vr::VRInitError_None || !vr::VRCompositor())
	{
		spHMD = NULL;
		return NULL;
	}

	uint32_t rw, rh;
	spHMD->GetRecommendedRenderTargetSize(&rw, &rh);

	sEyeTargetWidth = (eyeTargetWidth > 0) ? eyeTargetWidth : rw;
	sEyeTargetHeight = (eyeTargetHeight > 0) ? eyeTargetHeight : rh;

	const float displayAspect = ((float)sEyeTargetWidth * 2.0f) / sEyeTargetHeight;
	sAspectCrunch = (idealAspect > 0.0f) ? idealAspect / displayAspect : 1.0f;

	sImagePerspectiveScale = imagePerspectiveScale;

	sEyeOffsetX = eyeOffsetX;
	sEyeOffsetY = eyeOffsetY;

	if (init_eye_fbo(sLeftEyeFBO, sLeftEyeTexture) != GL_FRAMEBUFFER_COMPLETE || init_eye_fbo(sRightEyeFBO, sRightEyeTexture) != GL_FRAMEBUFFER_COMPLETE)
	{
		return NULL;
	}

	sValidFBO = true;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	sOvrInterface.OVR_Shutdown = OVR_Shutdown;
	sOvrInterface.OVR_ProvideEyeImage = OVR_ProvideEyeImage;
	sOvrInterface.OVR_GetPoses = OVR_GetPoses;
	sOvrInterface.OVR_SubmitEyes = OVR_SubmitEyes;
	sOvrInterface.OVR_GetHMDPitchYaw = OVR_GetHMDPitchYaw;
	sOvrInterface.OVR_DrawEyes = OVR_DrawEyes;
	sOvrInterface.OVR_PostSwapBuffers = OVR_PostSwapBuffers;
	sOvrInterface.OVR_SetSimultaneousEyeUpdates = OVR_SetSimultaneousEyeUpdates;
	sOvrInterface.OVR_SetBilinearFiltering = OVR_SetBilinearFiltering;

	return &sOvrInterface;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}
