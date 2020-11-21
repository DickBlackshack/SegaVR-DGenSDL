//(c) 2020 Rich Whitehouse, see segavr/readme.txt

#ifdef WITH_SEGAVR

#include "md.h"
#include "mem.h"
#include "rc-vars.h"
#include "pd.h"
#ifdef WITH_OPENVR
#include "openvr/openvr_interface.h"
#endif
#include <algorithm>

namespace
{
	const int32_t skHMDThMask = 0x60; //expect th and tr marked for write
	const int32_t skHMDIdle = 0x60;
	const int32_t skHMDReset = 0x40;

	const int32_t skHmdAckResponse = 0x60;
	const int32_t skHmdID_0 = 0x08;
	const int32_t skHmdID_1 = 0x00;

	const int32_t skHmdRequestIndex_Reset = -4;
	const int32_t skHmdRequestIndex_Idle = -3;
	const int32_t skHmdRequestIndex_ID0 = -2;
	const int32_t skHmdRequestIndex_ID1 = -1;

	const float skHmdMaxAngles[2] = { 30.0f, 360.0f };

	INLINE uint32_t encode_hmd_angles(const float *pAngles)
	{
		//assumes angles have already been clamped
		const uint32_t pitch = (pAngles[0] >= 0.0f) ? (uint8_t)pAngles[0] : ((uint8_t)-pAngles[0] ^ 0xFF) | (1 << 16);
		const uint32_t yaw = (uint32_t)pAngles[1];
		uint32_t enc = pitch | ((yaw & 0xFF) << 8) | ((yaw & 0x100) << 9);
		return enc;
	}

#ifdef MONO_AS_LUMA
	INLINE float bgr_to_mono_full_calculation(const uint8_t *pBgr)
	{
		return pBgr[2] * 0.2126f + pBgr[1] * 0.7152f + pBgr[0] * 0.0722f;
	}
#else
	INLINE float gamma_to_linear(const float v)
	{
		return (v <= 0.03928f) ? v / 12.92f : powf((v + 0.055f) / (1.0f + 0.055f), 2.4f);
	}

	INLINE float bgr_to_mono_full_calculation(const uint8_t *pBgr)
	{
		const float r = gamma_to_linear(pBgr[2] / 255.0f);
		const float g = gamma_to_linear(pBgr[1] / 255.0f);
		const float b = gamma_to_linear(pBgr[0] / 255.0f);
		const float l = r * 0.2126f + g * 0.7152f + b * 0.0722f;
		//luminance to lightness, see https://en.wikipedia.org/wiki/Lightness#1976
		//(6 / 29) ^ 3 = 216 / 24389 = 0.00885645167903563081717167575546
		//(29 / 3) ^ 3 = 24389 / 27 = 903.2962962962962962962962962963
		const float kTransitionPoint = 216.0f / 24389.0f;
		const float kSlope = 24389.0f / 27.0f;
		const float lStar = (l > kTransitionPoint) ? 116.0f * powf(l, 1.0f / 3.0f) - 16.0f : l * kSlope;
		return lStar / 100.0f * 255.0f;
	}
#endif

#ifdef WITH_OPENVR
	INLINE void ovri_update_eye_image(SOVRInterface *pOVRI, bool isLeft, struct bmap *pFrame)
	{
		uint32_t width, height;
		uint8_t *pData = pd_screen_filter_ptr(*pFrame, width, height);
		pOVRI->OVR_ProvideEyeImage(isLeft, pData, pFrame->bpp, width, height, pFrame->pitch);
	}
#endif
}

void md::segavr_init()
{
	mHmdThRwMask = 0;
	mHmdRequestBit = 0;
	mHmdRequestIndex = 0;
	mHmdIsActive = 0;
	mHmdVblankCounter = 0;
	mHmdVblankedOnLeft = mHmdScannedOnLeft = false;
	mHmdAngles[0] = mHmdAngles[1] = mHmdAVel[0] = mHmdAVel[1] = 0.0f;
	mHmdEncoded = 0;
	mStereoShotCount = 0;
	mStereoShotKickoff = false;
}

void md::segavr_cleanup()
{
	if (mpEyeMap)
	{
		free(mpEyeMap);
		mpEyeMap = NULL;
	}
	if (mpLightnessTable)
	{
		free(mpLightnessTable);
		mpLightnessTable = NULL;
		mpLightnessTableEntryCalculated = NULL;
	}
}

void md::segavr_register_vblank()
{
	if (!dgen_segavr_enabled)
	{
		return;
	}

	++mHmdVblankCounter;
	mHmdVblankedOnLeft = (mHmdEncoded & 0x80000) != 0;
	if (mHmdIsActive && dgen_segavr_timeoutinterval > 0 && mHmdVblankCounter >= dgen_segavr_timeoutinterval)
	{
		mHmdIsActive = 0;
	}
}

void md::segavr_begin_scan()
{
	//store the last eye we vblanked on as the one we're about to scan out
	mHmdScannedOnLeft = mHmdVblankedOnLeft;
}

bool md::segavr_allow_frameskip()
{
#ifdef WITH_OPENVR
	if (mpOVRI)
	{
		return true;
	}
#endif

	//don't allow frameskip when active, we're tracking and combing frames
	return !mHmdIsActive;
}

bool md::segavr_steal_frame(bmap *pFrame)
{
	if (!dgen_segavr_enabled)
	{
#ifdef WITH_OPENVR
		if (mpOVRI)
		{
			//if sega vr is disabled but the headset is enabled, just provide the same image for both eyes
			ovri_update_eye_image(mpOVRI, true, pFrame);
			ovri_update_eye_image(mpOVRI, false, pFrame);
		}
#endif
		return false;
	}

	bool isLeft = (!dgen_segavr_swapeyeframes) ? mHmdScannedOnLeft : !mHmdScannedOnLeft;
	if (dgen_segavr_flipbetweenpolls && (mHmdVblankCounter & 1))
	{ //continue alternating in between hmd polls
		isLeft = !isLeft;
	}

	if (mStereoShotCount > 0)
	{
		if (!mStereoShotKickoff)
		{ //want to make sure we kick off on the first frame of the left eye
			mStereoShotKickoff = (isLeft && mHmdVblankCounter == 0);
		}

		if (mStereoShotKickoff)
		{
			const intptr_t preserveSetting = dgen_raw_screenshots;
			dgen_raw_screenshots = 1;
			pd_do_screenshot(*this, isLeft ? "eye_left-" : "eye_right-", true);
			dgen_raw_screenshots = preserveSetting;
			if (!--mStereoShotCount)
			{
				mStereoShotKickoff = false;
			}
		}
	}

#ifdef WITH_OPENVR
	if (mpOVRI)
	{
		ovri_update_eye_image(mpOVRI, isLeft, pFrame);
		if (!mHmdIsActive)
		{
			//provide the same image for the other eye if the HMD isn't active
			ovri_update_eye_image(mpOVRI, !isLeft, pFrame);
		}

		//override the angles with the latest from the real headset
		float pitchYaw[2];
		mpOVRI->OVR_GetHMDPitchYaw(pitchYaw);
		pitchYaw[0] += dgen_openvr_pitch_offset;
		pitchYaw[1] += dgen_openvr_yaw_offset;
		segavr_update_hmd_angles(pitchYaw);
	}
#endif

	//only support processing raw rgb(a)
	const bool frameStealingEnabled = (mHmdIsActive && pFrame->bpp >= 24 && dgen_segavr_displaymode);

#ifdef WITH_OPENVR
	const int32_t desiredSwapValue = (mpOVRI) ? dgen_openvr_swapinterval : dgen_segavr_swapinterval;
#else
	const int32_t desiredSwapValue = dgen_segavr_swapinterval;
#endif

	if (desiredSwapValue != 0)
	{
		//try to adjust swap interval dynamically depending on whether frame stealing is enabled
		const int32_t ivl = (desiredSwapValue > 0) ? desiredSwapValue :
		                                             frameStealingEnabled ? 2 : 1;
		pd_set_swap_interval(ivl);
	}

	if (!frameStealingEnabled)
	{
		return false;
	}

	if (mpEyeMap &&
		(mpEyeMap->w != pFrame->w || mpEyeMap->h != pFrame->h || mpEyeMap->pitch != pFrame->pitch || mpEyeMap->bpp != pFrame->bpp))
	{
		//we'll need to re-create the eye buffer if the frame format/size changed
		segavr_cleanup();
	}

	const int32_t frameSize = pFrame->pitch * pFrame->h;

	if (!mpEyeMap)
	{
		mpEyeMap = (bmap *)malloc(sizeof(bmap) + frameSize);
		memcpy(mpEyeMap, pFrame, sizeof(bmap));

		mpEyeMap->data = (uint8_t *)(mpEyeMap + 1);
		memcpy(mpEyeMap->data, pFrame->data, frameSize);

		return false;
	}

	if (isLeft)
	{
		//grab it and store it to combine after the next vblank
		memcpy(mpEyeMap->data, pFrame->data, frameSize);
		return true;
	}

	//could speed this up a good bit

	const float leftR = dgen_segavr_left_r / 255.0f;
	const float leftG = dgen_segavr_left_g / 255.0f;
	const float leftB = dgen_segavr_left_b / 255.0f;
	const float rightR = dgen_segavr_right_r / 255.0f;
	const float rightG = dgen_segavr_right_g / 255.0f;
	const float rightB = dgen_segavr_right_b / 255.0f;

#ifndef MONO_AS_LUMA
	if (dgen_segavr_displaymode == 1 && !mpLightnessTable)
	{
		mpLightnessTable = (float *)malloc(sizeof(float) * 0x10000 + sizeof(bool) * 0x10000);
		mpLightnessTableEntryCalculated = (bool *)(mpLightnessTable + 0x10000);
		memset(mpLightnessTableEntryCalculated, 0, sizeof(bool) * 0x10000);
	}
#endif

	const int32_t bytesPerPixel = pFrame->bpp >> 3;
	for (int32_t y = 0; y < pFrame->h; ++y)
	{
		const uint8_t *pOldEyeRow = mpEyeMap->data + y * mpEyeMap->pitch;
		uint8_t *pEyeRow = pFrame->data + y * pFrame->pitch;
		for (int32_t x = 0; x < pFrame->w; ++x)
		{
			if (dgen_segavr_displaymode == 1)
			{
				float oldMono = bgr_to_mono(pOldEyeRow);
				float newMono = bgr_to_mono(pEyeRow);
				pEyeRow[2] = (uint8_t)std::min<float>(oldMono * leftR + newMono * rightR, 255.0f);
				pEyeRow[1] = (uint8_t)std::min<float>(oldMono * leftG + newMono * rightG, 255.0f);
				pEyeRow[0] = (uint8_t)std::min<float>(oldMono * leftB + newMono * rightB, 255.0f);
			}
			else
			{
				pEyeRow[2] = (uint8_t)std::min<float>(pOldEyeRow[2] * leftR + pEyeRow[2] * rightR, 255.0f);
				pEyeRow[1] = (uint8_t)std::min<float>(pOldEyeRow[1] * leftG + pEyeRow[1] * rightG, 255.0f);
				pEyeRow[0] = (uint8_t)std::min<float>(pOldEyeRow[0] * leftB + pEyeRow[0] * rightB, 255.0f);
			}

			pEyeRow += bytesPerPixel;
			pOldEyeRow += bytesPerPixel;
		}
	}

	return false;
}

void md::segavr_set_hmd_movement(const uint32_t axis, const float amount)
{
	mHmdAVel[axis] = amount;
}

void md::segavr_update_hmd_angles(const float *pAngleOverride)
{
	if (pAngleOverride)
	{
		mHmdAngles[0] = pAngleOverride[0];
		mHmdAngles[1] = pAngleOverride[1];
	}

	mHmdAngles[0] = std::min<float>(std::max<float>(mHmdAngles[0], -skHmdMaxAngles[0]), skHmdMaxAngles[0]);
	mHmdAngles[1] = fmodf(mHmdAngles[1], skHmdMaxAngles[1]);
	if (mHmdAngles[1] < 0.0f)
	{
		mHmdAngles[1] += skHmdMaxAngles[1];
	}

	//preserve L-R bits
	mHmdEncoded = (mHmdEncoded & 0xC0000) | encode_hmd_angles(mHmdAngles);
}

void md::segavr_apply_hmd_movement()
{
	if (mHmdAVel[0] == 0.0f && mHmdAVel[1] == 0.0f)
	{
		return;
	}

	mHmdAngles[0] += mHmdAVel[0];
	mHmdAngles[1] += mHmdAVel[1];
	segavr_update_hmd_angles(NULL);
}

bool md::segavr_catch_io_write(uint32_t a, uint8_t d)
{
	if (!dgen_segavr_enabled)
	{
		return false;
	}

	if (a == 0xA10005 && mHmdThRwMask == skHMDThMask)
	{
		if (d == skHMDIdle)
		{
			mHmdRequestIndex = skHmdRequestIndex_Idle;
			mHmdRequestBit = 0;
		}
		else if (d == skHMDReset)
		{
			mHmdRequestIndex = skHmdRequestIndex_Reset;
			mHmdRequestBit = 0;
			mHmdIsActive = 1; //consider a reset to mean the title expects us to be here
			mHmdVblankCounter = 0;
			mHmdAngles[0] = mHmdAngles[1] = 0.0f;
			mHmdEncoded = 0;
		}
		else
		{
			if (mHmdRequestBit != (d & 0x20))
			{
				mHmdRequestBit = (d & 0x20);
				mHmdRequestIndex = (mHmdRequestIndex + 1) % 5;
			}
		}
		return true;
	}
	else if (a == 0xA1000B)
	{
		mHmdThRwMask = d;
		mHmdRequestBit = 0;
		//we'll want to spin up to index 0 on the first request unless an explicit idle or reset is given
		mHmdRequestIndex = skHmdRequestIndex_ID1;
		//underlying code doesn't handle this anyway, so always return true
		return true;
	}

	return false;
}

bool md::segavr_catch_io_read(uint8_t &valueOut, uint32_t a)
{
	if (!dgen_segavr_enabled)
	{
		return false;
	}

	if (a == 0xA10005 && mHmdThRwMask == skHMDThMask)
	{
		switch (mHmdRequestIndex)
		{
		case skHmdRequestIndex_Reset:
			valueOut = 0;
			break;
		case skHmdRequestIndex_Idle:
			valueOut = 0x10 | skHmdAckResponse;
			break;
		case skHmdRequestIndex_ID0:
			valueOut = skHmdID_0;
			break;
		case skHmdRequestIndex_ID1:
			valueOut = 0x10 | skHmdID_1;
			break;
		case 0: //update the l-r bits on read
			if (mHmdVblankCounter >= dgen_segavr_eyeswapinterval)
			{
				//toggle which eye we'll be rendering on the next vblank
				//if vblank count is odd, should we keep the eye bit because it indicates a missed frame? can't say, since we don't have the hardware or more games to test with
				const uint32_t eyeBit = (mHmdEncoded & 0x80000) ? 0x40000 : 0x80000;
				mHmdEncoded = (mHmdEncoded & 0x3FFFF) | eyeBit;
				mHmdVblankCounter = 0;
			}
			//fall through
		default:
			valueOut = (mHmdRequestIndex & 1) ? 0x10 : 0;
			valueOut |= (mHmdEncoded >> (16 - (mHmdRequestIndex << 2))) & 0xF;
			break;
		}
		return true;
	}

	return false;
}

float md::bgr_to_mono(const uint8_t *pBgr)
{
#ifdef MONO_AS_LUMA
	return bgr_to_mono_full_calculation(pBgr);
#else
	//rather than calculating a table that assumes expanded rgb values based on the quantized rgb565 key, we calculate entries as they come in.
	//this way we gracefully handle the genesis palette mapping changing on us, assuming that there aren't any overlapping colors after our quantization takes place.
	//(and the palette probably ought to go for a slightly more accurate mapping, but haven't had cause to try to improve other aspects of this thing)
	const uint16_t key = (pBgr[0] >> 3) | ((pBgr[1] >> 2) << 5) | ((pBgr[2] >> 3) << 11);
	if (mpLightnessTableEntryCalculated[key])
	{
		return mpLightnessTable[key];
	}

	mpLightnessTable[key] = bgr_to_mono_full_calculation(pBgr);
	mpLightnessTableEntryCalculated[key] = true;
	return mpLightnessTable[key];
#endif
}

#endif //WITH_SEGAVR
