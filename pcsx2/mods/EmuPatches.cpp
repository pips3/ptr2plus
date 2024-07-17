#include <pcsx2/Config.h>
#include <pcsx2/vtlb.h>
#include "GS/Renderers/Common/GSDevice.h"

// Reactive Aspect Ratio Patch
// This one won't be visible in UI
void PTR2AspectRatioSet(AspectRatioType ar)
{
	char buf[4] = {};

	switch (ar)
	{
		// Adapted from Parotaku's widescreen patch
		case AspectRatioType::R4_3:
			// patch=1,EE,0016066c,word,3c013f40
			vtlb_memSafeWriteBytes(0x0016066c, &buf, 4);
			vtlb_memSafeWriteBytes(0x00160678, &buf, 4);
			vtlb_memSafeWriteBytes(0x0016067c, &buf, 4);
			break;
		case AspectRatioType::RAuto4_3_3_2:
			vtlb_memSafeWriteBytes(0x0016066c, &buf, 4);
			vtlb_memSafeWriteBytes(0x00160678, &buf, 4);
			vtlb_memSafeWriteBytes(0x0016067c, &buf, 4);
			break;
		case AspectRatioType::R16_9:
		
			{
				// patch=1,EE,0016066c,word,3c013f40
				u8 buf2[4] = {0x40, 0x3F, 0x01, 0x3C};
				vtlb_memSafeWriteBytes(0x0016066c, &buf2, 4);
				// patch=1,EE,00160678,word,44810000
				u8 buf3[4] = {00, 00, 0x81, 0x44};
				vtlb_memSafeWriteBytes(0x00160678, &buf3, 4);
				// patch=1,EE,0016067c,word,4600c602
				u8 buf4[4] = {0x02, 0xC6, 0x00, 0x46};
				vtlb_memSafeWriteBytes(0x0016067c, &buf4, 4);
			}
			break;
		case AspectRatioType::Stretch:
				u32 width = g_gs_device->GetWindowWidth();
				u32 height = g_gs_device->GetWindowHeight();

				// patch=1,EE,0016066c,word,3c013f40
				char buf[4] = {0x40, 0x3F, 0x01, 0x3C};
				vtlb_memSafeWriteBytes(0x0016066c, &buf, 4);

				// Replace the "3f40" (0.75 for 16:9) with our new multiplier
				float heightReal = height * 1.071; // To get 448 to 480
				heightReal = heightReal * 1.25; // This number has no meaning its eyeballed 
				float multiplier = heightReal / float(width);
				char buf2[4] = {};
				memcpy(buf2, &multiplier, 4);
				vtlb_memSafeWriteBytes(0x0016066c, &buf2[2], 1);
				vtlb_memSafeWriteBytes(0x0016066d, &buf2[3], 1);

				// patch=1,EE,00160678,word,44810000
				u8 buf3[4] = {00, 00, 0x81, 0x44};
				vtlb_memSafeWriteBytes(0x00160678, &buf3, 4);
				// patch=1,EE,0016067c,word,4600c602
				u8 buf4[4] = {0x02, 0xC6, 0x00, 0x46};
				vtlb_memSafeWriteBytes(0x0016067c, &buf4, 4);
		break;
	}
}

void PTR2AspectRatioSet()
{
	AspectRatioType ar = EmuConfig.GS.AspectRatio;
	PTR2AspectRatioSet(ar);
}

static void CreateAsyncFunc()
{
	/* u8 buf[48] =
	{
		0xE0, 0xFF, 0xBD, 0x27, // addiu $sp, $sp, -0x20
		0x10, 0x00, 0xA4, 0xFF, // sd a0, 0x10(sp)
		0x00, 0x00, 0x00, 0x00, // nop					<-------------
		0xF6, 0x05, 0x04, 0x0C, // jal 0x001017D8 (MtcWait)			 |
		0x01, 0x00, 0x04, 0x24, // addiu a0,zero,0x1  (li a0, 0x1)	 |
		0x00, 0x00, 0x00, 0x00, // nop								 |
		0x00, 0x00, 0xA4, 0x8F, // lw a0, (sp)						 |
		0xFA, 0xFF, 0x00, 0x10, // beqz, a0, 0x00104E98  -------------  (prev. 0xFA, 0xFF, 0x80, 0x10)
		0x00, 0x00, 0x00, 0x00, // nop
		0x10, 0x00, 0xA4, 0xDF, // ld a0, 0x10(sp)
		0x00, 0x00, 0x00, 0x00, // nop (gets patched with branch back to async hook return address in PTR2Hooks.cpp)
		0x20, 0x00, 0xBD, 0x27  // addiu sp,sp,0x20
	}; */

	u8 patch[] = /* Makes a thread with ID 3, entry point 'xtrView' */
	{
		0x12, 0x00, 0x04, 0x3C, // lui    $a0, 0x12
		0x03, 0x00, 0x05, 0x24, // addiu  $a1, $zero, 3
		0xC0, 0x05, 0x04, 0x08, // j      MtcExec
		0xC0, 0xEF, 0x84, 0x24, // addiu  $a0, $a0, -0x1040
		0x00, 0x00, 0x00, 0x00  // nop (To be replaced with a jump)
	};

	u8 loopPatch[] = /* Thread entry point - loops until $a0 is NOT zero */
	{
		0x00, 0x00, 0x00, 0x00, // nop   <----------
		0x00, 0x00, 0x00, 0x00, // nop             |
		0x00, 0x00, 0x00, 0x00, // nop             |
		0x00, 0x00, 0x00, 0x00, // nop             |
		0x00, 0x00, 0x00, 0x00, // nop             |
		0xF1, 0xFF, 0x80, 0x10, // beqz  $a0, 0 ----
		0x00, 0x00, 0x00, 0x00, // nop
		0x44, 0x06, 0x04, 0x08, // j     MtcExit
		0x00, 0x00, 0x00, 0x00, // nop
	};

	vtlb_memSafeWriteBytes(0x00104E90, &patch,     sizeof(patch));     // Patch 'PackIntDecode'
	vtlb_memSafeWriteBytes(0x0011EFC0, &loopPatch, sizeof(loopPatch)); // Patch 'xtrView'
}

void ReloadEmuPatches()
{
	CreateAsyncFunc();
	PTR2AspectRatioSet();
}
