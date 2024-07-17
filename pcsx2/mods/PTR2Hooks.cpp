#include "PrecompiledHeader.h"

#include "PTR2Hooks.h"

#include "mods/PTR2Common.h"

#include <Common.h>
#include <VMManager.h>

#include <common/Path.h>
#include <common/FileSystem.h>

#include "x86/iR5900.h"
#include <pcsx2/mods/P2mTools.h>
#include <pcsx2/mods/ActiveMods.h>
#include <pcsx2/DebugTools/MipsAssembler.h>

extern void iBranchTest(u32 newpc);

using namespace x86Emitter;
using namespace PTR2;

bool                  g_hasHooked      = false;
std::atomic<bool>     g_isHookOnLoop   = false;

std::atomic<bool>     g_threadWork     = false;
std::atomic<u32>      g_threadWorkType = HookType::None;

std::atomic<u32>      g_retAddr;
GPRregs				  g_cpuRegs;

static void CopyRegisters()
{
	g_cpuRegs = cpuRegs.GPR;
}

PrHookManager* PrHookMgr()
{
	static PrHookManager hookMgr;
	return &hookMgr;
}

void PrHookManager::InitHooks()
{
	m_gameHash = VMManager::GetCurrentCRC();

	switch (m_gameHash)
	{
	case 0x38E1D1E3: /* Patched PTR2 NTSC-J */
		m_hookMap.insert( { 0x00105AD8, CdctrlMemIntgDecode } );
		m_returnMap.insert( { 0x00105AD8, 0x00105AEC } );
		m_hooksInit = true;
		break;
	}

	if (!m_threadInit)
	{
		m_hookThread = std::thread(&HookThread::WorkerThread);
		m_threadInit = true;
	}
}

void PrHookManager::CdctrlMemIntgDecode()
{
#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Cyan, "[PTR2+] CdctrlMemIntgDecode hook called");
#endif

	CopyRegisters();
	HookThread::JumpToThreadWait();

	// Init work thread
	g_threadWorkType = HookType::CdctrlMemIntgDecode;
	g_threadWork     = true;

#if 0
	while (g_threadWork == true)
	{
	}
#endif

#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Cyan, "[PTR2+] Thread started");
#endif
}

void PrHookManager::intReadSub()
{
#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Green, "[PTR2+] intReadSub hook called");
#endif

	CopyRegisters();
	HookThread::JumpToThreadWait();

	// Init work thread
	g_threadWorkType = HookType::intReadSub;
	g_threadWork     = true;

#if 0
	while (g_threadWork == true)
	{
	}
#endif

#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Cyan, "Hook over!");
#endif
}

bool PrHookManager::RunHooks(const u32 curPC)
{
	if (!m_hooksInit)
		return false;

	// Find the functions to hook
	auto hook  = m_hookMap.find(curPC);
	auto ret  = m_returnMap.find(curPC);

	if (hook != m_hookMap.end() || g_hasHooked)
	{
		g_retAddr = ret->second;

		if (!g_hasHooked)
		{
			// Execute the hook!
			if (CHECK_EEREC)
				recCall(hook->second);
			else
				hook->second();

			g_hasHooked = true;
		}

		// We must not return while the hook is still running!
		if (!g_threadWork)
		{
			Console.WriteLn(Color_Cyan, "[PTR2+] Returning!");

			if (ret != m_returnMap.end())
			{
				// We're done, leave the game alone for now
				if (CHECK_EEREC)
				{
					g_branch = 1;

					iFlushCall(FLUSH_EVERYTHING);
					xMOV(ptr32[&cpuRegs.pc], ret->second);
					iBranchTest(ret->second);
				}
				else
				{
					cpuRegs.pc = ret->second;
				}

				g_hasHooked = false;
				g_isHookOnLoop = false;
				return true;
			}
			else
			{
				// We don't know what to set the program counter to...
				Console.WriteLn(Color_Red, "[PTR2+] Couldn't find return address!");
			}
		}
	}

	return false;
}

/* =============================================== */
/* ==================== HOOKS ==================== */
/* =============================================== */

/* Our loop is here */
/* while(1) { MtcWait(1); } */
#define WAIT_ADDR (0x00105A94) /* Hardcoded to patched NTSC-J ELF */

void HookThread::JumpToThreadWait()
{
	if (!g_isHookOnLoop)
	{
		Console.WriteLn(Color_Red, "[PTR2+] Jumping to loop");

		// Crashing here.
		// u8 jumpPatch[] = { 0xFD, 0xFF, 0x00, 0x10 };
		// vtlb_memSafeWriteBytes(JUMP_ADDR, &jumpPatch, sizeof(jumpPatch));

		Console.WriteLn(Color_Red, "CPU PC before jumping: 0x%08x", cpuRegs.pc);

		if (CHECK_EEREC)
		{
			g_branch = 1;

			iFlushCall(FLUSH_EVERYTHING);
			xMOV(ptr32[&cpuRegs.pc], WAIT_ADDR);
			iBranchTest(WAIT_ADDR);
		}
		else
		{
			cpuRegs.pc = WAIT_ADDR;
		}

		g_isHookOnLoop = true;
	}
}

/* CdctrlMemIntgDecode */
#define DECODE_ADDR (0x00105AD8) /* Hardcoded to patched NTSC-J ELF */

void HookThread::JumpBack()
{
	Console.WriteLn(Color_Red, "[PTR2+] Jumping back to 0x%08x", DECODE_ADDR);
	Console.WriteLn(Color_Red, "[PTR2+] CPU PC before jump: 0x%08x", cpuRegs.pc);

	if (CHECK_EEREC)
	{
		g_branch = 1;

		iFlushCall(FLUSH_EVERYTHING);
		xMOV(ptr32[&cpuRegs.pc], DECODE_ADDR);
		iBranchTest(DECODE_ADDR);
	}
	else
	{
		cpuRegs.pc = DECODE_ADDR;
	}
}

void HookThread::WorkerThread()
{
	Threading::SetNameOfCurrentThread("[PTR2+] Hook worker thread");

	while (true)
	{
		if (g_threadWork)
		{
#if defined(PCSX2_DEVBUILD)
			Console.WriteLn(Color_Cyan, "[PTR2+] In thread");
#endif

			switch (g_threadWorkType)
			{
			case HookType::CdctrlMemIntgDecode:
				CdctrlMemIntgDecode();
				break;
			case HookType::intReadSub:
				intReadSub();
				break;
			case HookType::None:
				break;
			}


#if defined(PCSX2_DEVBUILD)
			Console.WriteLn(Color_Cyan, "[PTR2+] Thread over");
#endif

			HookThread::JumpBack();
			g_threadWork = false;
		}
	}
}

void HookThread::CdctrlMemIntgDecode()
{
#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Cyan, "[PTR2+] In thread CdctrlMemIntgDecode");
#endif

	int memOff = 0;
	// Find FILE_STR on sp and get int name pointer

	int FILE_STR_pp;
	int int_name_pp;

	vtlb_memSafeReadBytes(g_cpuRegs.n.sp.UD[0] + 0x10, &FILE_STR_pp, 0x04);
	vtlb_memSafeReadBytes(FILE_STR_pp + 0x04, &int_name_pp, 0x04);

	//get int path to check if correct pointer or not
	char buf2[24] = {};
	vtlb_memSafeReadBytes(int_name_pp, buf2, sizeof(buf2));
	std::string int_path = buf2;
	if (int_path.find("INT") == std::string::npos) //if bad FILE_STR_pp
	{
		// it's probably a boxy HKO INT which has the pointer at a different place
		vtlb_memSafeReadBytes(g_cpuRegs.n.sp.UD[0], &FILE_STR_pp, 0x04);
		FILE_STR_pp += 0x1C; //gotta do this for boxy
		vtlb_memSafeReadBytes(FILE_STR_pp + 0x04, &int_name_pp, 0x04);
	}

	// Get current int header off using a0 - header_size - name_size
	// Probably a better way of finding this (stack pointer maybe)

	int head_size;
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0] + 0x0c, &head_size, 0x04);
	int name_size;
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0] + 0x10, &name_size, 0x04);

	int int_head_pp = g_cpuRegs.n.a0.UD[0] - head_size - name_size; //cpuRegs.GPR.n.s0.UD[0];

	PACKINT_FILE_STR packFile;
	vtlb_memSafeReadBytes(int_head_pp, &packFile, sizeof(packFile));

	//save the name chunk now, before the packed int gets overwritten by unpacked files
	//in the memory pool - the game doesn't care because it doesn't use the name chunk and it
	//already copied the header chunk elsewhere

	char name_chunk[80000]; //biggest name_chunk the game has is probably st8.int with ~41,000 - i havent checked XTR
	vtlb_memSafeReadBytes(int_head_pp + head_size, &name_chunk, name_size);

	std::string folder;
	switch (packFile.ftype)
	{
	case FT_VRAM:
		folder = "VRAM";
		break;
	case FT_R1:
		folder = "R1";
		break;
	case FT_R2:
		folder = "R2";
		break;
	case FT_R3:
		folder = "R3";
		break;
	case FT_R4:
		folder = "R4";
		break;
	case FT_SND:
		folder = "SND";
		break;
	case FT_ONMEM:
		folder = "ONMEM";
		break;
	}

	// Get cached address
	int write_pp = g_cpuRegs.n.a1.UD[0] + 0x20000000;

#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Gray, "Writing " + folder + " to: " + fmt::format("{:#08x}", (write_pp - 0x20000000) + memOff));
#endif
	int strings_off = (8 * packFile.fnum);

	for (int i = 0; i < packFile.fnum; i++)
	{
		//get file size and name pointer
		int size = 0;
		memcpy(&size, name_chunk + 4 + (8 * i), 4);

		int name_pp = 0;
		memcpy(&name_pp, name_chunk + (8 * i), 4);

		//calculate size of name (null terminated)
		int filename_size = 0;
		while (name_chunk[strings_off + name_pp + filename_size] != '\0')
		{
			filename_size++;
		}
		char buf[500];
		memcpy(&buf, name_chunk + strings_off + name_pp, filename_size);
		buf[filename_size] = 0;
		std::string name = buf;

#if defined(PCSX2_DEVBUILD)
		Console.WriteLn(Color_Gray, "Writing " + name + " to: " + fmt::format("{:#08x}", (write_pp - 0x20000000) + memOff));
#endif

		//get int file name
		char buf2[24];
		vtlb_memSafeReadBytes(int_name_pp, buf2, sizeof(buf2));
		std::string int_path = buf2;

		//remove ".INT"
		std::string int_dir = int_path.substr(0, int_path.size() - 4);

		std::string int_title(Path::GetFileTitle(int_path));

		// Remove "host:\"
		std::string path = int_dir;
		path = path.substr(6, path.size());

		path += "\\" + folder + "\\" + name;


		//replace path with modded file if active mod
		std::string mod;
		if (ActiveMods::GetMod(path, mod))
		{
			path = "MOD\\" + int_title + "\\" + folder + "\\" + name;
#if defined(PCSX2_DEVBUILD)
			Console.WriteLn(Color_Cyan, "Using " + name + " from " + mod + " instead.");
#endif
		}
		std::string final_path = Path::Combine(EmuFolders::PTR2, path);

		// Write bytes from file to memory
		const auto fp = FileSystem::OpenManagedCFile(final_path.c_str(), "rb");

		const int buf_size = 4096;
		u8 buf3[buf_size];
		int total_copied = buf_size;

		while (size > total_copied)
		{
			std::fread(&buf3, sizeof(buf3[0]), buf_size, fp.get());
			vtlb_memSafeWriteBytes(write_pp + memOff, &buf3, buf_size);
			total_copied += buf_size;
			memOff += buf_size;
		}
		std::fread(&buf3, size + buf_size - total_copied, 1, fp.get());
		vtlb_memSafeWriteBytes(write_pp + memOff, &buf3, size + buf_size - total_copied);
		memOff += size + buf_size - total_copied;

		// Make sure the offset is 0x10 aligned
		while (memOff % 0x10 != 0)
		{
			memWrite8(write_pp + memOff, 0);
			memOff++;
		}
	}

#if defined(PCSX2_DEVBUILD)
	Console.WriteLn(Color_Cyan, "[PTR2+] CdctrlMemIntgDecode over!");
#endif
}

void HookThread::intReadSub()
{
	int memOff = 0;

	// Find FILE_STR on sp and get int name pointer
	int FILE_STR_pp;
	int int_name_pp;

	vtlb_memSafeReadBytes(g_cpuRegs.n.s0.UD[0] + 0x10, &FILE_STR_pp, 0x04);
	vtlb_memSafeReadBytes(FILE_STR_pp + 0x04, &int_name_pp, 0x04);

	// Get current int header off using a0 - header_size - name_size
	// Probably a better way of finding this (stack pointer maybe)

	int head_size;
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0] + 0x0c, &head_size, 0x04);
	int name_size;
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0] + 0x10, &name_size, 0x04);

	//int int_head_pp = //cpuRegs.GPR.n.a0.UD[0] - head_size - name_size; //cpuRegs.GPR.n.s0.UD[0];

	PACKINT_FILE_STR packFile;
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0], &packFile, sizeof(packFile));

	//save the name chunk now, before the packed int gets overwritten by unpacked files
	//in the memory pool - the game doesn't care because it doesn't use the name chunk and it
	//already copied the header chunk elsewhere

	char name_chunk[80000]; //biggest name_chunk the game has is probably st8.int with ~41,000 - i havent checked XTR
	vtlb_memSafeReadBytes(g_cpuRegs.n.s4.UD[0] + head_size, &name_chunk, name_size);

	std::string folder;
	switch (packFile.ftype)
	{
	case FT_VRAM:
		folder = "VRAM";
		break;
	case FT_R1:
		folder = "R1";
		break;
	case FT_R2:
		folder = "R2";
		break;
	case FT_R3:
		folder = "R3";
		break;
	case FT_R4:
		folder = "R4";
		break;
	case FT_SND:
		folder = "SND";
		break;
	case FT_ONMEM:
		folder = "ONMEM";
		break;
	}

	// Get cached address
	int write_pp = g_cpuRegs.n.a1.UD[0] + 0x20000000;

#if defined(PCSX2_DEVBUILD)
	Console.WriteLn("Writing " + folder + " to: " + fmt::format("{:#08x}", (write_pp - 0x20000000) + memOff));
#endif
	int strings_off = (8 * packFile.fnum);

	for (int i = 0; i < packFile.fnum; i++)
	{
		//get file size and name pointer
		int size = 0;
		memcpy(&size, name_chunk + 4 + (8 * i), 4);

		int name_pp = 0;
		memcpy(&name_pp, name_chunk + (8 * i), 4);

		//calculate size of name (null terminated)
		int filename_size = 0;
		while (name_chunk[strings_off + name_pp + filename_size] != '\0')
		{
			filename_size++;
		}
		char buf[500];
		memcpy(&buf, name_chunk + strings_off + name_pp, filename_size);
		buf[filename_size] = 0;
		std::string name = buf;

#if defined(PCSX2_DEVBUILD)
		Console.WriteLn("Writing " + name + " to: " + fmt::format("{:#08x}", (write_pp - 0x20000000) + memOff));
#endif

		//get int file name
		char buf2[24];
		vtlb_memSafeReadBytes(int_name_pp, buf2, sizeof(buf2));
		std::string int_path = buf2;

		//remove ".INT"
		std::string int_dir = int_path.substr(0, int_path.size() - 4);

		std::string int_title(Path::GetFileTitle(int_path));

		// Remove "host:\"
		std::string path = int_dir;
		path = path.substr(6, path.size());

		path += "\\" + folder + "\\" + name;


		//replace path with modded file if active mod
		std::string mod;
		if (ActiveMods::GetMod(path, mod))
		{
			path = "MOD\\" + int_title + "\\" + folder + "\\" + name;
#if defined(PCSX2_DEVBUILD)
			Console.WriteLn(Color_Cyan, "Using " + name + " from " + mod + " instead.");
#endif
		}
		std::string final_path = Path::Combine(EmuFolders::PTR2, path);

		// Write bytes from file to memory
		const auto fp = FileSystem::OpenManagedCFile(final_path.c_str(), "rb");

		const int buf_size = 4096;
		u8 buf3[buf_size];
		int total_copied = buf_size;

		while (size > total_copied)
		{
			std::fread(&buf3, sizeof(buf3[0]), buf_size, fp.get());
			vtlb_memSafeWriteBytes(write_pp + memOff, &buf3, buf_size);
			total_copied += buf_size;
			memOff += buf_size;
		}
		std::fread(&buf3, size + buf_size - total_copied, 1, fp.get());
		vtlb_memSafeWriteBytes(write_pp + memOff, &buf3, size + buf_size - total_copied);
		memOff += size + buf_size - total_copied;

		// Make sure the offset is 0x10 aligned
		while (memOff % 0x10 != 0)
		{
			memWrite8(write_pp + memOff, 0);
			memOff++;
		}
	}
}
