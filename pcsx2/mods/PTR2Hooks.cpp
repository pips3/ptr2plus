#include "PTR2Hooks.h"

namespace PTR2
{
	namespace hook
	{
		void CdctrlMemIntgDecode()
		{
			Console.WriteLn("[PTR2] CdctrlMemIntgDecode hook called");
			int memOff = 0;

			//find FILE_STR on sp and get int name pointer
			int FILE_STR_pp;
			vtlb_memSafeReadBytes(cpuRegs.GPR.n.sp.UD[0] + 0x10, &FILE_STR_pp, 0x04);
			int int_name_pp;
			vtlb_memSafeReadBytes(FILE_STR_pp + 0x04, &int_name_pp, 0x04);

			//get current int header off using a0 - header_size - name_size
			//probably a better way of finding this (stack pointer maybe

			int head_size;
			vtlb_memSafeReadBytes(cpuRegs.GPR.n.s4.UD[0] + 0x0c, &head_size, 0x04);
			int name_size;
			vtlb_memSafeReadBytes(cpuRegs.GPR.n.s4.UD[0] + 0x10, &name_size, 0x04);

			int int_head_pp = cpuRegs.GPR.n.a0.UD[0] - head_size - name_size;  //cpuRegs.GPR.n.s0.UD[0];

			PACKINT_FILE_STR int_file;
			vtlb_memSafeReadBytes(int_head_pp, &int_file, sizeof(int_file));

			std::string folder;
			switch (int_file.ftype)
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

			//get mempool address to write to
			int write_pp = cpuRegs.GPR.n.a1.UD[0] + 0x20000000;

			//Console.WriteLn("write_pp = " + std::to_string(write_pp) + ", mem_off = " + std::to_string(mem_off));
			Console.WriteLn("Writing " + folder + " to: " + std::to_string(write_pp + mem_off));

			for (int i = 0; i < int_file.fnum; i++)
			{
				//get file size and name pointer
				int size = 0;
				vtlb_memSafeReadBytes(int_head_pp + int_file.head_size + 0x04 + (8 * i), &size, 0x04);
				int name_pp = 0;
				vtlb_memSafeReadBytes(int_head_pp + int_file.head_size + (8 * i), &name_pp, 0x04);

				int strings_off = int_head_pp + int_file.head_size + (8 * int_file.fnum);

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

				Console.WriteLn("Writing " + name + " to: " + std::to_string(write_pp + mem_off));

				//get int file name
				char buf2[24];
				vtlb_memSafeReadBytes(int_name_pp, buf2, sizeof(buf2));
				std::string int_path = buf2;

				//remove ".INT"
				std::string int_dir = int_path.substr(0, int_path.size() - 4);

				std::string path = int_dir + "\\" + folder + "\\" + name;

				//remove "host:\"
				std::string rel_path = path.substr(6, path.size());

				std::string final_path = Path::Combine(EmuFolders::PTR2, rel_path);


				//write bytes from file to memory
				const auto fp = FileSystem::OpenManagedCFile(final_path.c_str(), "rb");

				const int buf_size = 4096;
				u8 buf3[buf_size];
				int total_copied = buf_size;

				while (size > total_copied)
				{
					std::fread(&buf3, sizeof(buf3[0]), buf_size, fp.get());
					vtlb_memSafeWriteBytes(write_pp + mem_off, &buf3, buf_size);
					total_copied += buf_size;
					mem_off += buf_size;
				}
				std::fread(&buf3, size + buf_size - total_copied, 1, fp.get());
				vtlb_memSafeWriteBytes(write_pp + mem_off, &buf3, size + buf_size - total_copied);
				mem_off += size + buf_size - total_copied;

				// make sure 0x10 aligned
				while (mem_off % 0x10 != 0)
				{
					memWrite8(write_pp + mem_off, 0);
					mem_off++;
				}
			}
		}
	}
}