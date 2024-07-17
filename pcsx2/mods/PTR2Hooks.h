#pragma once

#include "Common.h"
#include <unordered_set>

typedef void (*hookFunc_t)();
typedef u32 u_adr;

namespace PTR2
{
	enum FILE_TYPE_ENUM
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
	};

	struct PACKINT_FILE_STR
	{
		/* 0x00 */ s32 id;
		/* 0x04 */ s32 fnum;
		/* 0x08 */ FILE_TYPE_ENUM ftype;
		/* 0x0c */ s32 head_size;
		/* 0x10 */ s32 name_size;
		/* 0x14 */ s32 data_size;
		/* 0x18 */ s32 pad[2];
		/* 0x20 */ u_adr adr[0];
	};  /* 0x20 */

	static_assert(sizeof(PACKINT_FILE_STR) == 0x20, "PACKINT_FILE_STR struct is not 0x20 bytes");
}

enum HookType
{
	None,
	CdctrlMemIntgDecode,
	intReadSub
};

namespace HookThread
{
	void WorkerThread();

	void JumpToThreadWait();
	void JumpBack();

	void CdctrlMemIntgDecode();
	void intReadSub();
}

class PrHookManager
{
	DeclareNoncopyableObject(PrHookManager);

public:
	PrHookManager()  = default;
	~PrHookManager() = default;

	void InitHooks();
	bool RunHooks(const u32 curPC);

	/* Hooks */
	static void CdctrlMemIntgDecode();
	static void intReadSub();

private:
	u32  m_gameHash;
	bool m_hooksInit;

	std::unordered_map<u32, hookFunc_t> m_hookMap;
	std::unordered_map<u32, u32>        m_returnMap;

	std::thread m_hookThread;
	bool        m_threadInit;
};

PrHookManager* PrHookMgr();