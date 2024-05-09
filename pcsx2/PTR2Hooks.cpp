#include "PTR2Hooks.h"

#include "mods/PTR2Common.h"

#include <Common.h>
#include <VMManager.h>

#include <common/Path.h>
#include <common/FileSystem.h>

using namespace PTR2;

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
		m_hookMap.insert( { 0x00105AD8, CdctrlMemIntgDecode} );
		m_returnMap.insert( { 0x00105AD8, 0x00105AE0 });
		m_hooksInit = true;
		break;
	}
}

void PrHookManager::CdctrlMemIntgDecode()
{
	Console.WriteLn(Color_Green, "[PTR2] CdctrlMemIntgDecode hook called");

	int memOff = 0;

	// Find FILE_STR on sp and get int name pointer
	int FILE_STR_pp;
	int int_name_pp;

	vtlb_memSafeReadBytes(cpuRegs.GPR.n.sp.UD[0] + 0x10, &FILE_STR_pp, 0x04);
	vtlb_memSafeReadBytes(FILE_STR_pp + 0x04, &int_name_pp, 0x04);

	// Get current int header off using a0 - header_size - name_size
	// Probably a better way of finding this (stack pointer maybe)

	int head_size;
	vtlb_memSafeReadBytes(cpuRegs.GPR.n.s4.UD[0] + 0x0c, &head_size, 0x04);
	int name_size;
	vtlb_memSafeReadBytes(cpuRegs.GPR.n.s4.UD[0] + 0x10, &name_size, 0x04);

	int int_head_pp = cpuRegs.GPR.n.a0.UD[0] - head_size - name_size;  //cpuRegs.GPR.n.s0.UD[0];

	PACKINT_FILE_STR packFile;
	vtlb_memSafeReadBytes(int_head_pp, &packFile, sizeof(packFile));

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

	// get mempool address to write to
	int write_pp = cpuRegs.GPR.n.a1.UD[0] + 0x20000000;

	//Console.WriteLn("write_pp = " + std::to_string(write_pp) + ", memOff = " + std::to_string(memOff));
	Console.WriteLn("Writing " + folder + " to: " + fmt::format("0x{:x}", write_pp + memOff));

	for (int i = 0; i < packFile.fnum; i++)
	{
		//get file size and name pointer
		int size = 0;
		vtlb_memSafeReadBytes(int_head_pp + packFile.head_size + 0x04 + (8 * i), &size, 0x04);
		int name_pp = 0;
		vtlb_memSafeReadBytes(int_head_pp + packFile.head_size + (8 * i), &name_pp, 0x04);

		int strings_off = int_head_pp + packFile.head_size + (8 * packFile.fnum);

		//calculate size of name (null terminated)
		int filename_size = 0;
		while (memRead8(strings_off + name_pp + filename_size) != 0)
		{
			filename_size++;
		}
		char buf[500];
		vtlb_memSafeReadBytes(strings_off + name_pp, &buf, filename_size);
		buf[filename_size] = 0;
		std::string name = buf;

		Console.WriteLn("Writing " + name + " to: " + fmt::format("0x{:x}", write_pp + memOff));

		//get int file name
		char buf2[24];
		vtlb_memSafeReadBytes(int_name_pp, buf2, sizeof(buf2));
		std::string int_path = buf2;

		//remove ".INT"
		std::string int_dir = int_path.substr(0, int_path.size() - 4);

		std::string path = int_dir + "\\" + folder + "\\" + name;

		// Remove "host:\"
		std::string rel_path = path.substr(6, path.size());

		std::string final_path = Path::Combine(EmuFolders::PTR2, rel_path);


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

void PrHookManager::RunHooks(const u32 curPC)
{
	if (!m_hooksInit)
		return;

	// Find the functions to hook
	auto hook = m_hookMap.find(curPC);
	auto ret  = m_returnMap.find(curPC);

	if (hook != m_hookMap.end())
	{
		// Execute the hook!
		hook->second();

		if (ret != m_returnMap.end())
		{
			// We're done, leave the game alone for now
			cpuRegs.pc = ret->second;
		}
		else
		{
			// We don't know what to set the program counter to... fuck
			Console.WriteLn(Color_Red, "[PTR2] Couldn't find return address!");
		}
	}
}