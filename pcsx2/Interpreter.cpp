/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "Elfheader.h"

#include "DebugTools/Breakpoints.h"

#include "common/FastJmp.h"

#include <common/Path.h>
#include <common/FileSystem.h>

#include <float.h>

using namespace R5900;		// for OPCODE and OpcodeImpl

extern int vu0branch, vu1branch;

static int branch2 = 0;
static u32 cpuBlockCycles = 0;		// 3 bit fixed point version of cycle count
static std::string disOut;
static bool intExitExecution = false;
static fastjmp_buf intJmpBuf;

static void intEventTest();

// These macros are used to assemble the repassembler functions

void intBreakpoint(bool memcheck)
{
	const u32 pc = cpuRegs.pc;
 	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
		return;

	if (!memcheck)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc);
		if (cond && !cond->Evaluate())
			return;
	}

	CBreakPoints::SetBreakpointTriggered(true);
	VMManager::SetPaused(true);
	Cpu->ExitExecution();
}

void intMemcheck(u32 op, u32 bits, bool store)
{
	// compute accessed address
	u32 start = cpuRegs.GPR.r[(op >> 21) & 0x1F].UD[0];
	if (static_cast<s16>(op) != 0)
		start += static_cast<s16>(op);
	if (bits == 128)
		start &= ~0x0F;

	start = standardizeBreakpointAddress(start);
	const u32 end = start + bits/8;

	auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_EE);
	for (size_t i = 0; i < checks.size(); i++)
	{
		auto& check = checks[i];

		if (check.result == 0)
			continue;
		if ((check.cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((check.cond & MEMCHECK_READ) == 0 && !store)
			continue;

		if (start < check.end && check.start < end)
			intBreakpoint(true);
	}
}

void intCheckMemcheck()
{
	const u32 pc = cpuRegs.pc;
	const int needed = isMemcheckNeeded(pc);
	if (needed == 0)
		return;

	const u32 op = memRead32(needed == 2 ? pc + 4 : pc);
	const OPCODE& opcode = GetInstruction(op);

	const bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
		case MEMTYPE_BYTE:
			intMemcheck(op, 8, store);
			break;
		case MEMTYPE_HALF:
			intMemcheck(op, 16, store);
			break;
		case MEMTYPE_WORD:
			intMemcheck(op, 32, store);
			break;
		case MEMTYPE_DWORD:
			intMemcheck(op, 64, store);
			break;
		case MEMTYPE_QWORD:
			intMemcheck(op, 128, store);
			break;
	}
}

typedef enum
{
	FT_NONE = 0,
	FT_VRAM = 1,
	FT_SND = 2,
	FT_ONMEM = 3,
	FT_R1 = 4,
	FT_R2 = 5,
	FT_R3 = 6,
	FT_R4 = 7,
	FT_PAD0 = 8,
	FT_PAD1 = 9,
	FT_PAD2 = 10,
	FT_PAD3 = 11,
	FT_PAD4 = 12,
	FT_PAD5 = 13,
	FT_PAD6 = 14,
	FT_PAD7 = 15,
	FT_MAX = 16
} FILE_TYPE_ENUM;

typedef unsigned int u_adr;
typedef struct
{ // 0x20
	/* 0x00 */ int id;
	/* 0x04 */ int fnum;
	/* 0x08 */ FILE_TYPE_ENUM ftype;
	/* 0x0c */ int head_size;
	/* 0x10 */ int name_size;
	/* 0x14 */ int data_size;
	/* 0x18 */ int pad[2];
	/* 0x20 */ u_adr adr[0];
} PACKINT_FILE_STR;

static_assert(sizeof(PACKINT_FILE_STR) == 0x20, "PACKINT_FILE_STR struct not packed to 0x20 bytes");



static void hook_CdCtrlMemIntGDecode() 
{
	Console.WriteLn("CdCtrlMemIntGDecode Hook Called");
	int mem_off = 0;

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
		
		//make sure 0x10 aligned
		while (mem_off % 0x10 != 0)
		{
			memWrite8(write_pp + mem_off, 0);
			mem_off++;
		}

	}

	

}
static void execI()
{
	// execI is called for every instruction so it must remains as light as possible.
	// If you enable the next define, Interpreter will be much slower (around
	// ~4fps on 3.9GHz Haswell vs ~8fps (even 10fps on dev build))
	// Extra note: due to some cycle count issue PCSX2's internal debugger is
	// not yet usable with the interpreter
//#define EXTRA_DEBUG
#if defined(EXTRA_DEBUG) || defined(PCSX2_DEVBUILD)
	// check if any breakpoints or memchecks are triggered by this instruction
	if (isBreakpointNeeded(cpuRegs.pc))
		intBreakpoint(false);

	intCheckMemcheck();
#endif

	const u32 pc = cpuRegs.pc;

	//ptr2 hook
	if (cpuRegs.pc == 0x105AD8) //jal
	{
		hook_CdCtrlMemIntGDecode();
		cpuRegs.pc = 0x105AE0;
	}

	// We need to increase the pc before executing the memRead32. An exception could appears
	// and it expects the PC counter to be pre-incremented
	cpuRegs.pc += 4;

	// interprete instruction
	cpuRegs.code = memRead32( pc );

	const OPCODE& opcode = GetCurrentInstruction();
#if 0
	static long int runs = 0;
	//use this to find out what opcodes your game uses. very slow! (rama)
	runs++;
	 //leave some time to startup the testgame
	if (runs > 1599999999)
	{
		 //find all opcodes beginning with "L"
		if (opcode.Name[0] == 'L')
		{
			Console.WriteLn ("Load %s", opcode.Name);
		}
	}
#endif

#if 0
	static long int print_me = 0;
	// Based on cycle
	// if( cpuRegs.cycle > 0x4f24d714 )
	// Or dump from a particular PC (useful to debug handler/syscall)
	if (pc == 0x80000000)
	{
		print_me = 2000;
	}
	if (print_me)
	{
		print_me--;
		disOut.clear();
		disR5900Fasm(disOut, cpuRegs.code, pc);
		CPU_LOG( disOut.c_str() );
	}
#endif


	cpuBlockCycles += opcode.cycles;

	opcode.interpret();
}

static __fi void _doBranch_shared(u32 tar)
{
	branch2 = cpuRegs.branch = 1;
	execI();

	// branch being 0 means an exception was thrown, since only the exception
	// handler should ever clear it.

	if( cpuRegs.branch != 0 )
	{
		cpuRegs.pc = tar;
		cpuRegs.branch = 0;
	}
}

static void doBranch( u32 target )
{
	_doBranch_shared( target );
	cpuRegs.cycle += cpuBlockCycles >> 3;
	cpuBlockCycles &= (1<<3)-1;
	intEventTest();
}

void intDoBranch(u32 target)
{
	//Console.WriteLn("Interpreter Branch ");
	_doBranch_shared( target );

	if( Cpu == &intCpu )
	{
		cpuRegs.cycle += cpuBlockCycles >> 3;
		cpuBlockCycles &= (1<<3)-1;
		intEventTest();
	}
}

void intSetBranch()
{
	branch2 = /*cpuRegs.branch =*/ 1;
}

////////////////////////////////////////////////////////////////////
// R5900 Branching Instructions!
// These are the interpreter versions of the branch instructions.  Unlike other
// types of interpreter instructions which can be called safely from the recompilers,
// these instructions are not "recSafe" because they may not invoke the
// necessary branch test logic that the recs need to maintain sync with the
// cpuRegs.pc and delaySlot instruction and such.

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
// fixme: looking at the other branching code, shouldn't those _SetLinks in BGEZAL and such only be set
// if the condition is true? --arcum42

void J()
{
	doBranch(_JumpTarget_);
}

void JAL()
{
	// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		if (_JumpTarget_ == 0x3563b8)
			GoemonUnloadTlb(cpuRegs.GPR.n.a0.UL[0]);
	}
	_SetLink(31);
	doBranch(_JumpTarget_);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/

void BEQ()  // Branch if Rs == Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

void BNE()  // Branch if Rs != Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void BGEZ()    // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BGEZAL() // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BGTZ()    // Branch if Rs >  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] > 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLEZ()   // Branch if Rs <= 0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLTZ()    // Branch if Rs <  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLTZAL()  // Branch if Rs <  0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
}

/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/


void BEQL()    // Branch if Rs == Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BNEL()     // Branch if Rs != Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLEZL()    // Branch if Rs <= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGTZL()     // Branch if Rs >  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] > 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZL()     // Branch if Rs <  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZL()     // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZALL()   // Branch if Rs <  0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZALL()   // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void JR()
{
	// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		const u32 add = cpuRegs.GPR.r[_Rs_].UL[0];
		if (add == 0x33ad48 || add == 0x35060c)
			GoemonPreloadTlb();
	}
	doBranch(cpuRegs.GPR.r[_Rs_].UL[0]);
}

void JALR()
{
	const u32 temp = cpuRegs.GPR.r[_Rs_].UL[0];

	if (_Rd_)  _SetLink(_Rd_);

	doBranch(temp);
}

} } }		// end namespace R5900::Interpreter::OpcodeImpl


// --------------------------------------------------------------------------------------
//  R5900cpu/intCpu interface (implementations)
// --------------------------------------------------------------------------------------

static void intReserve()
{
	// fixme : detect cpu for use the optimize asm code
}

static void intReset()
{
	cpuRegs.branch = 0;
	branch2 = 0;
}

static void intEventTest()
{
	// Perform counters, ints, and IOP updates:
	_cpuEventTest_Shared();

	if (intExitExecution)
	{
		intExitExecution = false;
		fastjmp_jmp(&intJmpBuf, 1);
	}
}

static void intSafeExitExecution()
{
	// If we're currently processing events, we can't safely jump out of the interpreter here, because we'll
	// leave things in an inconsistent state. So instead, we flag it for exiting once cpuEventTest() returns.
	if (eeEventTestIsActive)
		intExitExecution = true;
	else
		fastjmp_jmp(&intJmpBuf, 1);
}

static void intCancelInstruction()
{
	// See execute function.
	fastjmp_jmp(&intJmpBuf, 0);
}

static void intExecute()
{
	// This will come back as zero the first time it runs, or on instruction cancel.
	// It will come back as nonzero when we exit execution.
	if (fastjmp_set(&intJmpBuf) != 0)
		return;

	for (;;)
	{
		if (!VMManager::Internal::HasBootedELF())
		{
			// Avoid reloading every instruction.
			u32 elf_entry_point = VMManager::Internal::GetCurrentELFEntryPoint();
			u32 eeload_main = g_eeloadMain;
			u32 eeload_exec = g_eeloadExec;

			while (true)
			{
				execI();

				if (cpuRegs.pc == EELOAD_START)
				{
					// The EELOAD _start function is the same across all BIOS versions afaik
					const u32 mainjump = memRead32(EELOAD_START + 0x9c);
					if (mainjump >> 26 == 3) // JAL
						g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);

					eeload_main = g_eeloadMain;
				}
				else if (cpuRegs.pc == eeload_main)
				{
					eeloadHook();
					if (VMManager::Internal::IsFastBootInProgress())
					{
						// See comments on this code in iR5900-32.cpp's recRecompile()
						const u32 typeAexecjump = memRead32(EELOAD_START + 0x470);
						const u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0);
						const u32 typeCexecjump = memRead32(EELOAD_START + 0x618);
						const u32 typeDexecjump = memRead32(EELOAD_START + 0x600);
						if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
							g_eeloadExec = EELOAD_START + 0x2B8;
						else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
							g_eeloadExec = EELOAD_START + 0x170;
						else
							Console.WriteLn("intExecute: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");

						eeload_exec = g_eeloadExec;
					}

					elf_entry_point = VMManager::Internal::GetCurrentELFEntryPoint();
				}
				else if (cpuRegs.pc == eeload_exec)
				{
					eeloadHook2();
				}
				else if (cpuRegs.pc == elf_entry_point)
				{
					VMManager::Internal::EntryPointCompilingOnCPUThread();
					break;
				}
			}
		}
		else
		{
			while (true)
				execI();
		}
	}
}

static void intStep()
{
	execI();
}

static void intClear(u32 Addr, u32 Size)
{
}

static void intShutdown() {
}

R5900cpu intCpu =
{
	intReserve,
	intShutdown,

	intReset,
	intStep,
	intExecute,

	intSafeExitExecution,
	intCancelInstruction,

	intClear
};
