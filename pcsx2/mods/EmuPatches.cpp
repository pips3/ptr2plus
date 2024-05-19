#include <pcsx2/Config.h>
#include <pcsx2/vtlb.h>
#include "GS/Renderers/Common/GSDevice.h"

//Reactive Aspect Ratio Patch
//This one won't be visible in UI
void PTR2AspectRatioSet(AspectRatioType ar)
{
	char buf[4] = {};
	switch (ar)
	{
		//adapted from Parotaku's widescreen patch
		case AspectRatioType::R4_3:
			//patch=1,EE,0016066c,word,3c013f40
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
				//patch=1,EE,0016066c,word,3c013f40
				char buf2[4] = {0x40, 0x3F, 0x01, 0x3C};
				vtlb_memSafeWriteBytes(0x0016066c, &buf2, 4);
				//patch=1,EE,00160678,word,44810000
				char buf3[4] = {00, 00, 0x81, 0x44};
				vtlb_memSafeWriteBytes(0x00160678, &buf3, 4);
				//patch=1,EE,0016067c,word,4600c602
				char buf4[4] = {0x02, 0xC6, 0x00, 0x46};
				vtlb_memSafeWriteBytes(0x0016067c, &buf4, 4);
			}
			break;
		case AspectRatioType::Stretch:
				u32 width = g_gs_device->GetWindowWidth();
				u32 height = g_gs_device->GetWindowHeight();

				//patch=1,EE,0016066c,word,3c013f40
				char buf[4] = {0x40, 0x3F, 0x01, 0x3C};
				vtlb_memSafeWriteBytes(0x0016066c, &buf, 4);

				//replace the "3f40" (0.75 for 16:9) with our new multiplier
				float heightReal = height * 1.071; //to get 448 to 480
				heightReal = heightReal * 1.25; //this number has no meaning its eyeballed 
				float multiplier = heightReal / float(width);
				char buf2[4] = {};
				memcpy(buf2, &multiplier, 4);
				vtlb_memSafeWriteBytes(0x0016066c, &buf2[2], 1);
				vtlb_memSafeWriteBytes(0x0016066d, &buf2[3], 1);

				//patch=1,EE,00160678,word,44810000
				char buf3[4] = {00, 00, 0x81, 0x44};
				vtlb_memSafeWriteBytes(0x00160678, &buf3, 4);
				//patch=1,EE,0016067c,word,4600c602
				char buf4[4] = {0x02, 0xC6, 0x00, 0x46};
				vtlb_memSafeWriteBytes(0x0016067c, &buf4, 4);
		break;
	}
}

void PTR2AspectRatioSet()
{
	AspectRatioType ar = EmuConfig.GS.AspectRatio;
	PTR2AspectRatioSet(ar);
}


