#include <common/FileSystem.h>
#include <pcsx2/Config.h>

#include <fstream>
#include <common/Path.h>

#include "Memory.h"

#include "PTR2Common.h"
#include <common/StringUtil.h>
#include <pcsx2/mods/ActiveMods.h>
#include <pcsx2/mods/PriorityList.h>

using namespace PTR2;

bool files_to_delete;
#pragma pack(push, 1)

struct p2m_header
{
	char p2m_magic[4];
	u16 version;
	u16 file_count;
	char reserved[8]; 
	u32 meta_offset; 
	u32 meta_size;
	u32 path_offset;
	u32 path_size;
	u32 type_offset;
	u32 type_size;
	u32 size_offset;
	u32 size_size;
	u32 body_offset;
	u32 body_size;
	char reserved2[8]; 
};

#pragma pack(pop)

struct STDAT_DAT
{
	u32 id; //i think
	char section_name[4];
	float bpm;
	u32 null;
	u32 olm_addr1;
	u32 olm_addr2;
	u32 olm_addr3;
	FILE_STR file1; //e.g int, if file = vs08vs0.int , end
	FILE_STR file2; //e.g c wp2
	FILE_STR file3; //e.g g wp2
	FILE_STR file4; //e.g ba wp2
	u32 olm_addr4; //null for cutscenes/boxy 
};

struct olm_struct
{
	FILE_STR file; //if ST00 end
	u32 unk;
	u32 unk_olm_addr;
	u32 stage_name_pos; //if TITLE end
};


/*
file map
0007ff70 RAM: 0017EF70
FILE_STR logo int
u32 null
FILE_STR stmenu int
u32 null
FILE_STR EXT00 - EXT09 wp2

*/

static_assert(sizeof(p2m_header) == 64, "p2m_header struct not packed to 64 bytes");
static_assert(sizeof(sceCdlFILE) == 0x24, "sceCdlFILE struct not packed to 0x24 bytes");
static_assert(sizeof(FILE_STR) == 0x2C, "FILE_STR struct not packed to 0x2C bytes");
static_assert(sizeof(STDAT_DAT) == 0xD0, "STDAT_DAT struct not packed to 0xD0 bytes");
static_assert(sizeof(olm_struct) == 0x38, "olm_struct struct not packed to 0x38 bytes");

enum ModType : u16
{
	isoFile = 0,
	intAsset = 1,
	pcsx2Tex = 2
};
struct mod_file
{
	std::string path;
	ModType type;
	u32 size;
	u32 pos;
	bool tmp;
};

bool copyStream(FILE* from, FILE* to, int size)
{
	//streams must be set up at correct positions

	const int buf_size = 4096;
	u8 buf[buf_size];
	int total_copied = buf_size;
	while (size > total_copied)
	{

		std::fread(&buf, sizeof(buf[0]), buf_size, from);
		std::fwrite(&buf, sizeof(buf[0]), buf_size, to);
		total_copied += buf_size;
	}
	std::fread(&buf, size + buf_size - total_copied, 1, from);
	std::fwrite(&buf, size + buf_size - total_copied, 1, to);

	return true;
}
bool readBytes(u32& mem, void* dst, u32 size)
{
	if (vtlb_memSafeReadBytes(mem, dst, size))
	{
		mem += size;
		return true;
	}
	return false;
}

static std::string GetPTR2ModDirectory()
{
	return Path::Combine(EmuFolders::PTR2, "/MOD");
}
static std::string GetModFilePath(std::string path)
{
	size_t slash = path.find('\\');
	path.erase(0, slash);
	std::string mod_dir = GetPTR2ModDirectory();
	std::string mod_file = Path::Combine(mod_dir, path);
	return mod_file;
}
std::string GetPathFromModName(std::string modname)
{
	return Path::Combine(EmuFolders::PTR2Mods, modname);
}

mod_file GetP2MFile(FILE* stream, p2m_header& hd, int index)
{
	mod_file file;

	//read
	int off = 0; //ftell(stream);
	std::string path;
	for (int i = 0; i < index + 1; i++)
	{
		char buf[50];
		std::fseek(stream, hd.path_offset + off, SEEK_SET);
		fgets(buf, sizeof(buf), stream);
		path = buf;
		//fgets puts fp to end, so keep track ourselves
		off += path.length() + 1;
	}
	file.path = path;

	//read type
	std::fseek(stream, hd.type_offset + (2 * index), SEEK_SET);
	ModType type;
	std::fread(&type, sizeof(u16), 1, stream);
	file.type = type;

	//read size/pos
	std::fseek(stream, hd.size_offset + (2 * index), SEEK_SET);
	u32 size;
	u32 pos;
	std::fread(&size, sizeof(u32), 1, stream);
	std::fread(&pos, sizeof(u32), 1, stream);
	file.size = size;
	file.pos = pos;

	file.tmp = false;

	return file;
}
std::vector<mod_file> GetP2MFiles(FILE* stream, p2m_header& hd)
{
	//make sure at beginning
	std::fseek(stream, 0, SEEK_SET);
	std::fread(&hd, 64, 1, stream);

	std::vector<mod_file> files;

	for (int i = 0; i < hd.file_count; i++)
	{
		mod_file file = GetP2MFile(stream, hd, i);
		files.push_back(file);
	}
	return files;
}
std::vector<mod_file> GetP2MFiles(FILE* stream, std::string modpath)
{
	const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");
	p2m_header hd;
	return GetP2MFiles(stream, hd);
}
std::vector<mod_file> GetP2MFiles(std::string modpath)
{
	const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");
	p2m_header hd;
	return GetP2MFiles(fp.get(), hd);
}
std::vector<mod_file> GetP2MFilesByModName(std::string modname)
{
	std::string filename = GetPathFromModName(modname);
	return GetP2MFiles(filename);
}

mod_file GetP2MFile(FILE* stream, p2m_header& hd, std::string path)
{
	std::fseek(stream, hd.path_offset, SEEK_SET);

	long off = ftell(stream);
	int i = 0;
	for (i; i < hd.file_count; i++)
	{
		char buf[50];
		if (fgets(buf, sizeof(buf), stream) != nullptr)
		{
			std::string found_path = buf;
			if (StringUtil::compareNoCase(found_path, path))
			{
				break;
			}
			//fgets puts fp to end, so reset it;
			off += path.length() + 1;
			std::fseek(stream, off, SEEK_SET);
		}
	}

	return GetP2MFile(stream, hd, i);
}
mod_file GetP2MFileByEntry(std::pair<std::string, std::string> entry)
{
	std::string filename = GetPathFromModName(entry.second);
	const auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	p2m_header hd;
	return GetP2MFile(fp.get(), hd, entry.first);
}

bool ELFfilenameFound(u32 mem, std::string filename)
{
	char dst[24];
	vtlb_memSafeReadBytes(mem, dst, sizeof(dst));
	std::string str(StringUtil::toUpper(dst));

	filename = StringUtil::toUpper(filename);

	if (str.find(filename) != std::string::npos)
	{
		return true;
	}

	return false;
}
bool ELFpathFound(int& path_off, int& file_off, std::string filename)
{
	u32 mem_pointer = 0x0017EF70;
	//Console.WriteLn("Mem pointer: %u", mem_pointer);
	FILE_STR file;
	//Console.WriteLn("file size: %u", sizeof(file));
	for (int i = 0; i < 2; i++) //loop through logo/stmenu structs
	{
		readBytes(mem_pointer, &file, sizeof(file));
		if (ELFfilenameFound(file.fname_p, filename))
		{
			path_off = file.fname_p;
			file_off = mem_pointer - sizeof(file);
			return true;
		}

		mem_pointer += 4;
	}
	
	for (int i = 0; i < 10; i++) //loop through ext0X structs
	{
		readBytes(mem_pointer, &file, sizeof(file));
		if (ELFfilenameFound(file.fname_p, filename))
		{
			path_off = file.fname_p;
			file_off = mem_pointer - sizeof(file);
			return true;
		}
	}
	
	STDAT_DAT stdat;
	do //loop through stdata structs
	{
		readBytes(mem_pointer, &stdat, sizeof(stdat));
		
		//loop through file_strs inside stdat
		u32 offsets[4] = {stdat.file1.fname_p, stdat.file2.fname_p, stdat.file3.fname_p, stdat.file4.fname_p};
		for (int i = 0; i < 4; i++)
		{
			if (offsets[i] != 0 && ELFfilenameFound(offsets[i], filename))
			{
				path_off = offsets[i];
				file_off = mem_pointer - 4 - sizeof(file) * (5 - (i + 1));
				return true;
			}

		}

	} while (!ELFfilenameFound(stdat.file1.fname_p, "VS08VS0.INT"));
	olm_struct olm;

	do // Loop through OLM structs
	{
		readBytes(mem_pointer, &olm, sizeof(olm));
		if (ELFfilenameFound(olm.file.fname_p, filename))
		{
			path_off = olm.file.fname_p;
			file_off = mem_pointer - sizeof(olm);
			return true;
		}

	} while (!ELFfilenameFound(olm.file.fname_p, "STG00.OLM") || !ELFfilenameFound(olm.stage_name_pos, "TITLE"));

	return false;
}

bool PatchELFPath(std::string path, bool unpatch, bool tmp)
{
	std::string filename = Path::GetFileName(path).data();
	int off, file_off = 0;
	if (ELFpathFound(off, file_off, filename))
	{
		char patch[24];
		std::string strpath;
		if (unpatch)
		{
			strpath = "host:\\" + StringUtil::toUpper(path);
		}
		else if (tmp)
		{
			strpath = "host:\\TMP\\" + filename;
		}
		else
		{
			strpath = "host:\\MOD\\" + filename;
		}
		strncpy(patch, strpath.c_str(), sizeof(patch));
		vtlb_memSafeWriteBytes(off, patch, sizeof(patch));

		//patch search value
		char search[1] = {0};
		vtlb_memSafeWriteBytes(file_off + 3, search, 1);
		return true;
	}
	return false;
}
bool PatchELFPath(mod_file file)
{
	return PatchELFPath(file.path, false, file.tmp);
}
bool PatchELFPath(std::string path, bool tmp)
{
	return PatchELFPath(path, false, tmp);
}
bool UnPatchELFPath(mod_file file)
{
	return PatchELFPath(file.path, true, false);
}
bool UnPatchELFPath(std::string path)
{
	return PatchELFPath(path, true, false);
}

/*
bool isIntAsset(std::string path)
{
	
	std::string extension = Path::GetExtension(path.data()).data();
	transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
	if (extension.find("WP2") == std::string::npos && extension.find("INT") == std::string::npos && extension.find("XTR") == std::string::npos && extension.find("XTR") == std::string::npos)
		return true;
	else
		return false;
	
}
*/
bool ApplyModFile(mod_file file)
{
	//dont patch files from unpacked INT/XTR files as they aren't in the ISO paths
	//they are loaded using PTR2Hooks, which will use the activemod cache file
	if (file.type == isoFile)
	{
		if (!PatchELFPath(file))
		{
			return false;
		}
	}
}
bool StartUpApplyActiveMods()
{
	std::vector<std::pair<std::string, std::string>> activeModCache = ActiveMods::GetAll();
	int file_count = activeModCache.size();

	for (std::pair<std::string, std::string> entry : activeModCache)
	{
		mod_file file = GetP2MFileByEntry(entry);
		if (!ApplyModFile(file))
		{
			return false;
		}
	}
	return true;
}



bool isInDeleteCache(std::string path)
{
	const std::string deletecache_filename(Path::Combine(EmuFolders::Cache, "deletefile.cache"));

	const auto fp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	for (int i = 0; i < file_count; i++)
	{
		std::string delete_path;

		long off = ftell(fp.get());

		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			return false;
		delete_path = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += delete_path.length() + 1;
		std::fseek(fp.get(), off, SEEK_SET);

		if (StringUtil::compareNoCase(delete_path, path))
		{
			return true;
		}
	}
	return false;
}
bool addDeleteEntry(std::string path)
{
	const std::string deletecache_filename(Path::Combine(EmuFolders::Cache, "deletefile.cache"));

	const auto fp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "rb+");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	std::fseek(fp.get(), 0, SEEK_END);
	fputs(path.c_str(), fp.get());
	fputc('\0', fp.get());

	file_count++;
	std::fseek(fp.get(), 0, SEEK_SET);
	std::fwrite(&file_count, sizeof(file_count), 1, fp.get());

	return true;
}

/* unused
bool removeDeleteEntry(std::string path)
{
	const std::string deletecache_filename(Path::Combine(EmuFolders::Cache, "deletefile.cache"));

	auto fp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	std::vector<std::string> new_paths;
	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;

		long off = ftell(fp.get());

		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			return false;
		active_path = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += active_path.length() + 1;
		std::fseek(fp.get(), off, SEEK_SET);

		if (path != active_path)
		{
			new_paths.push_back(active_path);
		}
	}
	fp.reset();
	FileSystem::DeleteFilePath(deletecache_filename.c_str());
	const auto newfp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "wb");
	u16 new_filecount = new_paths.size();

	std::fwrite(&new_filecount, sizeof(new_filecount), 1, newfp.get());
	for (int i = 0; i < new_filecount; i++)
	{
		fputs(new_paths[i].c_str(), newfp.get());
		fputc('\0', newfp.get());
	}
	return true;
}*/


//sometimes loading a new mod isnt possible because the current active file is in use by the game
//instead of stopping the user, we can put the new mod in a TMP folder and tell the game it's there
//the current active file path is put in a deletecache file similar to activemodscache, and will be deleted
//as soon as the game has stopped using it (tries every frame), then the new mod is copied from TMP to the MOD folder
//if the file in TMP is locked up by the game, just extract it again from the p2m into MOD, and put the TMP
//file in the delete cache - this is a long workaround but should make the user experience more smooth
bool TryDeleteFiles()
{
	const std::string deletecache_filename(Path::Combine(EmuFolders::Cache, "deletefile.cache"));

	auto fp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	std::vector<std::string> paths;
	//get filepaths in deletecache
	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;

		long off = ftell(fp.get());

		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			return false;
		active_path = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += active_path.length() + 1;
		std::fseek(fp.get(), off, SEEK_SET);

		paths.push_back(active_path);
	}
	//for each file in deletecache
	std::vector<std::string> newpaths;

	for (std::string path : paths)
	{
		std::string filepath = Path::Combine(EmuFolders::PTR2, path);
		//first, try to delete
		if (!FileSystem::FileExists(filepath.c_str()))
		{
			continue;
		}
		if (!FileSystem::DeleteFilePath(filepath.c_str()))
		{
			//file must be still in use, keep in deletecache
			newpaths.push_back(path);
			continue;
		}

		//if deleted file was in MOD, check for replacement file in TMP
		std::string_view folder = Path::GetDirectory(path);
		std::string_view filename = Path::GetFileName(path);
		std::string TMP = Path::Combine(EmuFolders::PTR2, "/TMP");
		std::string MOD = GetPTR2ModDirectory();
		std::string pathTMP = Path::Combine(TMP, filename);
		std::string pathMOD = Path::Combine(MOD, filename);
		if (folder == "MOD")
		{
			if (FileSystem::FileExists(pathTMP.c_str()))
			{
				if (FileSystem::CopyFilePath(pathTMP.c_str(), pathMOD.c_str(), false))
				{
					if (!FileSystem::DeleteFilePath(pathTMP.c_str()))
					{
						//file was able to be copied from, but cannot be deleted
						//unsure if this would ever actually happen, but handle it anyway
						newpaths.push_back(pathTMP); //add to delete cache
					}
					//could make quicker by storing file type in delete cache file
					//so I can just check if its an ISO file in here and call PatcHElfPath directly
					std::string mod;
					ActiveMods::GetMod(path, mod);
					std::pair<std::string, std::string> entry(path, mod);
					mod_file file = GetP2MFileByEntry(entry);

					ApplyModFile(file);
				}
				else
				{
					//file is still in use, extract it from p2m file again instead
					std::string mod;
					ActiveMods::GetMod(path, mod);
					std::string modpath = Path::Combine(EmuFolders::PTR2Mods, mod);

					std::pair<std::string, std::string> entry(path, mod);
					mod_file file = GetP2MFileByEntry(entry);

					const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");
					if (!fp)
						return false;

					std::fseek(fp.get(), file.pos, SEEK_SET);

					const auto newfp = FileSystem::OpenManagedCFile(pathMOD.c_str(), "w+b");
					if (!newfp)
						return false;
					copyStream(fp.get(), newfp.get(), file.size);

					newpaths.push_back(pathTMP); //add to delete cache

					ApplyModFile(file);
				}
			}
		}
		//std::string name = Path::GetFileName(path);
	}
	fp.reset();
	FileSystem::DeleteFilePath(deletecache_filename.c_str());
	const auto newfp = FileSystem::OpenManagedCFile(deletecache_filename.c_str(), "wb");
	u16 new_filecount = newpaths.size();

	std::fwrite(&new_filecount, sizeof(new_filecount), 1, newfp.get());
	for (int i = 0; i < new_filecount; i++)
	{
		fputs(newpaths[i].c_str(), newfp.get());
		fputc('\0', newfp.get());
	}
	if (new_filecount == 0)
	{
		//flag bool 
		files_to_delete = false;
	}
	return true;
}

bool refreshPriorityList()
{
	std::vector<std::string> list = PriorityList::Get();
	for (std::string mod : list)
	{
		if (!ActiveMods::isModActive(mod))
		{
			PriorityList::Remove(mod);
		}
	}
	return true;
}




bool disableMod(std::string modname)
{
	//get files of mod
	std::vector<mod_file> files = GetP2MFilesByModName(modname);

	//delete files from MOD folder
	for (mod_file file : files)
	{
		std::string full_path = GetModFilePath(file.path);
		
		if (!FileSystem::DeleteFilePath(full_path.c_str()))
		{
			addDeleteEntry(Path::MakeRelative(file.path, EmuFolders::PTR2));
			files_to_delete = true;
		}

		if (file.type == isoFile)
		{
			UnPatchELFPath(file.path);
		}
	}

	//remove from activemods
	ActiveMods::RemoveMod(modname);
	//removeActiveModEntry(modname);

	//remove from prioritylist
	PriorityList::Remove(modname);

	return true;
}


bool MoveModFile(FILE* stream, mod_file file)
{
	std::string real_path;
	if (file.tmp)
		real_path = Path::Combine(Path::Combine(EmuFolders::PTR2, "/TMP"), Path::GetFileName(file.path));
	else
		real_path = GetModFilePath(file.path);

	std::string real_path_folder = std::string(Path::GetDirectory(real_path));


	std::fseek(stream, file.pos, SEEK_SET);

	FileSystem::EnsureDirectoryExists(real_path_folder.c_str(), true);
	const auto newfp = FileSystem::OpenManagedCFile(real_path.c_str(), "w+b");
	const auto new_stream = newfp.get();

	return copyStream(stream, new_stream, file.size);
}

bool moveAllModFiles(FILE* stream, std::vector<mod_file> files)
{
	for (mod_file file : files)
	{
		MoveModFile(stream, file);
	}
	return true;
}

bool disableModEntry(std::string path)
{
	ActiveMods::RemoveEntry(path);
	refreshPriorityList();

	std::string mod_file = GetModFilePath(path);

	if (!FileSystem::DeleteFilePath(mod_file.c_str()))
	{
		addDeleteEntry(path);
		files_to_delete = true;
	}
	//could be unpatching unecessarily if its an int asset file
	//but proooobably still cheaper than fetching the actual file from p2m to check
	UnPatchELFPath(path); 
	return true;
}

bool enableMod(std::string filename)
{
	const auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	//get files of mod
	std::vector<mod_file> files = GetP2MFiles(fp.get(), filename);

	//check activemods
		//if files exist, disable mods associated
	std::string mod = Path::GetFileName(filename).data();
	for (mod_file file : files)
	{
		std::string existing_mod;
		if (ActiveMods::GetMod(file.path, existing_mod) == true)
		{

			//disableMod(existing_mod);
			disableModEntry(file.path);
			
			//if file in delete cache, mark it to be put in TMP
			if (isInDeleteCache(file.path) && file.type != intAsset) //dont count int asset as TMP workaround isnt necessary
			{
				file.tmp = true;
				continue;
			}
		}

		//move file to MOD folder
		MoveModFile(fp.get(), file);

		ApplyModFile(file);

		std::pair<std::string, std::string> entry(file.path, mod);
		ActiveMods::Add(entry);
	}

	//add to activemods
	
	/* uncomment this for less file read/writes than the current call in the loop
	std::vector<std::pair<std::string, std::string>> entries;
	for (std::string path : paths)
	{
		std::pair<std::string, std::string> entry(path, mod);
		entries.push_back(entry);
	}
	ActiveModCacheAdd(entries);
	*/

	//add to prioritylist
	PriorityList::Push(mod);

	return true;
}



bool ApplySingleModEntry(std::string path, std::string modname)
{
	std::string modpath = GetPathFromModName(modname);

	const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");
	//get files of mod
	p2m_header hd;
	if (std::fread(&hd, 64, 1, fp.get()) != 1)
		return false;

	mod_file file = GetP2MFile(fp.get(), hd, path);

	//if file in delete cache, mark it to be put in TMP
	if (isInDeleteCache(path))
	{
		file.tmp = true;
	}

	MoveModFile(fp.get(), file);

	ApplyModFile(file);

	return true;
	
}

bool RefreshMods() 
{
	std::vector<std::string> priorities = PriorityList::Get();
	int mod_count = priorities.size();
	
	//get list of mod entries after priorities
	std::map<std::string, std::string> entries; //we dont need order but map is probably better than unordered anyway
	for (int i = mod_count - 1; i > -1; i--) //iterate lowest priority first
	{
		std::string modname = priorities[i];
		std::vector<mod_file> files = GetP2MFilesByModName(modname);
		for (mod_file file : files)
		{
			entries.insert_or_assign(file.path, modname);
			//entries.push_back(entry);
		}
	}

	//remove entries already applied
	std::vector<std::pair<std::string, std::string>> cache = ActiveMods::GetAll();
	for (std::pair<std::string, std::string> entry : cache)
	{
		if ( entries[entry.first] == entry.second )
		{
			entries.erase(entry.first);
		}
		else //remove entries we are going to replace
			ActiveMods::RemoveEntry(entry.first);
	}
	refreshPriorityList();

	//convert map to vector of pairs, so we can access both key and value
	std::vector<std::pair<std::string, std::string>> new_cache;
	new_cache.assign(entries.begin(), entries.end());

	//apply our new entries
	for (std::pair<std::string, std::string> entry : new_cache)
	{
		ApplySingleModEntry(entry.first, entry.second);
		ActiveMods::Add(entry);
	}
	return true;
}

bool AdjustModPriority(std::string modname, int newIndex)
{
	PriorityList::Remove(modname);
	PriorityList::Add(modname, newIndex);
	
	return RefreshMods();
}

bool IsP2M(const char* filename, std::string& title, std::string& author, std::string& description, bool& enabled)
{
	const auto fp = FileSystem::OpenManagedCFile(filename, "rb");
	if (!fp)
		return false;

	char p2m_magic[4] = {0x50, 0x32, 0x4D, 0x11};

	p2m_header hd;

	if (std::fread(&hd, 64, 1, fp.get()) != 1)
		return false;
	if (std::strncmp(hd.p2m_magic, p2m_magic, 4) != 0)
		return false; //if magic not found, return false

	//read title, author, description

	//change this to fgets probably
	std::ifstream file;
	file.open(filename);
	if (!file.is_open()) return false;
	file.seekg(hd.meta_offset, file.beg);

	for (int i = 1; i <= 3; i++)
	{
		char letter;
		while (file.get(letter) && letter != '\0')
		{
			switch (i)
			{
				case 1:
					title += letter;
					break;
				case 2:
					author += letter;
					break;
				case 3:
					description += letter;
					break;
			}
		}
	}
	
	return true;
}