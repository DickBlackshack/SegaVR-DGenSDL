#pragma once
#ifndef _OPENVR_INTERFACE_H
#define _OPENVR_INTERFACE_H

#ifdef OVR_INTERFACE_DLL
	#define OVR_API extern "C" __declspec(dllexport)
#else
	#define OVR_API extern "C" __declspec(dllimport)
#endif

struct SOVRInterface
{
	void (*OVR_Shutdown)();
	bool (*OVR_ProvideEyeImage)(const bool isLeft, const uint8_t *pData, const uint32_t bpp, const uint32_t width, const uint32_t height, const uint32_t stride);
	void (*OVR_GetPoses)();
	void (*OVR_SubmitEyes)();
	void (*OVR_GetHMDPitchYaw)(float *pAnglesOut);
	void (*OVR_DrawEyes)(const uint32_t windowWidth, const uint32_t windowHeight);
	void (*OVR_PostSwapBuffers)();
	void (*OVR_SetSimultaneousEyeUpdates)(const bool enabled);
	void (*OVR_SetBilinearFiltering)(const bool enabled);
	void (*OVR_SetLensModel)(const uint8_t *pLensData, const uint32_t dataSize);
};

OVR_API SOVRInterface *OpenVR_Interface_Init(const float eyeOffsetX, const float eyeOffsetY, const uint32_t eyeTargetWidth, const uint32_t eyeTargetHeight, const float idealAspect, const float imagePerspectiveScale);

#endif //_OPENVR_INTERFACE_H
