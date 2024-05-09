#pragma once

namespace PTR2
{
	enum FREAD_MODE : u8
	{
		FRMODE_PC = 0,
		FRMODE_CD = 1,
		FRMODE_MAX = 2
	};

	enum FTYPE_MODE : u8
	{
		FTMODE_INTG = 0,
		FTMODE_WP2 = 1,
		FTMODE_ETC = 2,
		FTMODE_XTR = 3,
		FTMODE_MAX = 4
	};

	struct sceCdlFILE
	{
		u32 lsn;		/* File location */
		u32 size;		/* File size	 */
		s8  name[16];	/* File name	 */
		u8  date[8];	/* Date			 */
		u32 flag;		/* Flags		 */
	};

	static_assert(sizeof(sceCdlFILE) == 0x24, "sceCdlFILE struct is not 0x24 bytes");

	#pragma pack(push, 1)

	struct FILE_STR
	{
		/* 0x00 */ FREAD_MODE frmode; /* File read mode */
		/* 0x01 */ FTYPE_MODE ftmode; /* File type */
		/* 0x02 */ u8  mchan;
		/* 0x03 */ u8  search;
		/* 0x04 */ u32 fname_p;
		/* 0x08 */ sceCdlFILE fpCd;
	};

	#pragma pack(pop)

	// static_assert(sizeof(FILE_STR) == 0x2C, "FILE_STR struct is not 0x2C bytes");
}