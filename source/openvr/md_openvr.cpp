#ifdef WITH_OPENVR

#include "md.h"
#include "rc-vars.h"
#include "pd.h"
#include "openvr/openvr_interface.h"
#include <assert.h>

//yeah, it's a little weird to have "OpenVRInterface" as the interface to an interface. this done to avoid accidentally pulling in any lib dependencies, or having hard
//dependencies on the openvr module itself in the case of non-static linking. this way we can be pretty carefree if we update lib versions and don't have to resort to
//building our own libs to maintain an earlier platform target.

bool md::openvr_init()
{
	if (!dgen_openvr_enabled || !pd_screen_is_opengl())
	{
		return false;
	}

	if (mOpenVRLib)
	{
		return true;
	}

	mOpenVRLib = LoadLibrary("OpenVRInterface.dll");
	if (!mOpenVRLib)
	{
		return false;
	}

	SOVRInterface *(*pOpenVR_Interface_Init)(const float eyeOffsetX, const float eyeOffsetY, const uint32_t eyeTargetWidth, const uint32_t eyeTargetHeight, const float idealAspect, const float imagePerspectiveScale);
	*(uintptr_t *)&pOpenVR_Interface_Init = (uintptr_t)GetProcAddress(mOpenVRLib, "OpenVR_Interface_Init");
	if (pOpenVR_Interface_Init)
	{
		const float idealAspect = (dgen_openvr_idealaspect_height > 0) ? (float)dgen_openvr_idealaspect_width / dgen_openvr_idealaspect_height : 0.0f;
		const float imagePerspectiveScale = (float)dgen_openvr_imgpscale / 65536.0f;

		mpOVRI = pOpenVR_Interface_Init(dgen_openvr_eyex / 65536.0f, dgen_openvr_eyey / 65536.0f, dgen_openvr_eyewidth, dgen_openvr_eyeheight, idealAspect, imagePerspectiveScale);
		if (mpOVRI)
		{
			mpOVRI->OVR_SetSimultaneousEyeUpdates(dgen_openvr_eyes_sync != 0);
			mpOVRI->OVR_SetBilinearFiltering(dgen_openvr_bilinear != 0);
			return true;
		}
	}

	openvr_cleanup();
	return false;
}

void md::openvr_cleanup()
{
	if (mOpenVRLib)
	{
		assert(mpOVRI);
		mpOVRI->OVR_Shutdown();
		FreeLibrary(mOpenVRLib);
		mOpenVRLib = 0;
	}
	mpOVRI = NULL;
}

void md::openvr_get_poses()
{
	if (mpOVRI)
	{
		mpOVRI->OVR_GetPoses();
	}
}

bool md::openvr_submit_eyes()
{
	if (mpOVRI)
	{
		mpOVRI->OVR_SubmitEyes();
		return true;
	}
	return false;
}

#endif //WITH_OPENVR
