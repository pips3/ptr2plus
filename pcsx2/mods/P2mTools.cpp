#include <common/FileSystem.h>
#include <pcsx2/Config.h>

#include <fstream>
#include <common/Path.h>

#include "Memory.h"

#include "PTR2Common.h"
#include <common/StringUtil.h>

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

static std::string GetActiveModsFilename()
{
	return Path::Combine(EmuFolders::Cache, "activemods.cache");
}

static std::string GetModPriorityFilename()
{
	return Path::Combine(EmuFolders::Cache, "modpriority.cache");
}

static std::string GetPTR2ModDirectory()
{
	return Path::Combine(EmuFolders::PTR2, "/MOD");
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

static std::string GetModFilePath(std::string path)
{
	size_t slash = path.find('\\');
	path.erase(0, slash);
	std::string mod_dir = GetPTR2ModDirectory();
	std::string mod_file = Path::Combine(mod_dir, path);
	return mod_file;
}

//read a single activemod file chunk (path, modname) and return
bool getActiveMod(FILE* stream, std::string& path, std::string& modname)
{
	long off = ftell(stream);

	char buf[900];
	if (fgets(buf, sizeof(buf), stream) == nullptr)
		return false;
	path = buf;

	//fgets puts file offset at end for some reason, so reset it:
	off += path.length() + 1;
	std::fseek(stream, off, SEEK_SET);

	char buf2[900];
	if (fgets(buf2, sizeof(buf2), stream) == nullptr)
		return false;
	modname = buf2;

	std::fseek(stream, off + modname.length() + 1, SEEK_SET);

	return true;
}

bool isModActive(const std::string mod)
{
	const std::string activemods_filename(GetActiveModsFilename());

	const auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	//int file_off = sizeof(file_count);
	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;
		std::string active_mod_name;
		getActiveMod(fp.get(), active_path, active_mod_name);

		if (active_mod_name == mod)
		{
			return true;
		}
	}
	return false;

}

bool GetActiveModFromPath(const std::string path, std::string& mod)
{
	const std::string activemods_filename(GetActiveModsFilename());

	const auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;
		std::string active_mod_name;
		getActiveMod(fp.get(), active_path, active_mod_name);

		/* std::string active_path_UPPER(active_path);
		transform(active_path_UPPER.begin(), active_path_UPPER.end(), active_path_UPPER.begin(), ::toupper);
		std::string path_UPPER(path);
		transform(path_UPPER.begin(), path_UPPER.end(), path_UPPER.begin(), ::toupper);

		if (active_path_UPPER == path_UPPER)
		*/
		if (StringUtil::compareNoCase(active_path, path))
		{
			mod = active_mod_name;
			return true;
		}
	}
	return false;
}

bool findActiveModPaths(const std::string mod, std::vector<std::string>& paths)
{
	const std::string activemods_filename(GetActiveModsFilename());

	const auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;
		std::string active_mod_name;
		getActiveMod(fp.get(), active_path, active_mod_name);

		if (active_mod_name == mod)
		{
			paths.push_back(active_path);
		}
	}
	return true;
}
/*
bool addActiveModEntry(std::string filename, std::vector<std::string> paths)
{
	const std::string activemods_filename(GetActiveModsFilename());

	const auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb+");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	std::fseek(fp.get(), 0, SEEK_END);
	for (std::string path : paths)
	{
		fputs(path.c_str(), fp.get());
		fputc('\0', fp.get());
		fputs(filename.c_str(), fp.get());
		fputc('\0', fp.get());

		file_count++;
	}
	
	std::fseek(fp.get(), 0, SEEK_SET);
	std::fwrite(&file_count, sizeof(file_count), 1, fp.get());

	return true;
}
*/
//path, modname
bool ActiveModCacheSave(std::vector<std::pair<std::string, std::string>> activeModCache)
{
	const std::string activemods_filename(GetActiveModsFilename());

	FileSystem::DeleteFilePath(activemods_filename.c_str());

	auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "wb");

	u16 file_count = activeModCache.size();

	std::fwrite(&file_count, sizeof(file_count), 1, fp.get());
	for (int i = 0; i < file_count; i++)
	{
		fputs(activeModCache[i].first.c_str(), fp.get());
		fputc('\0', fp.get());
		fputs(activeModCache[i].second.c_str(), fp.get());
		fputc('\0', fp.get());
	}
	return true;
}

std::vector<std::pair<std::string, std::string>> GetActiveModCache()
{
	const std::string activemods_filename(GetActiveModsFilename());
	auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb+");
	auto stream = fp.get();
	u16 file_count;

	std::fread(&file_count, sizeof(u16), 1, stream);

	std::vector<std::pair<std::string, std::string>> activeModCache;
	for (int i = 0; i < file_count; i++)
	{
		long off = ftell(stream);

		char buf[900];
		fgets(buf, sizeof(buf), stream);

		std::string path = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += path.length() + 1;
		std::fseek(stream, off, SEEK_SET);

		char buf2[900];
		fgets(buf2, sizeof(buf2), stream);
		std::string modname = buf2;

		std::fseek(stream, off + modname.length() + 1, SEEK_SET);

		std::pair<std::string, std::string> entry(path, modname);
		activeModCache.push_back(entry);
	}
	return activeModCache;
}

bool updatePriorityList();

bool ActiveModCacheRemovePath(std::string path)
{
	std::vector<std::pair<std::string, std::string>> activeModCache = GetActiveModCache();
	std::vector<std::pair<std::string, std::string>> newActiveModCache;

	int length = activeModCache.size();
	for (int i = 0; i < length; i++){
		if (!StringUtil::compareNoCase(activeModCache[i].first, path))
		{
			newActiveModCache.push_back(activeModCache[i]);
		}
	}

	ActiveModCacheSave(newActiveModCache);
	updatePriorityList();

	return true;
}

bool ActiveModCacheRemoveMod(std::string modname)
{
	std::vector<std::pair<std::string, std::string>> activeModCache = GetActiveModCache();
	std::vector<std::pair<std::string, std::string>> newActiveModCache;

	int length = activeModCache.size();
	for (int i = 0; i < length; i++)
	{
		if (!StringUtil::compareNoCase(activeModCache[i].second, modname))
		{
			newActiveModCache.push_back(activeModCache[i]);
		}
	}

	/*
	//if no more entries for that mod, remove from priority list too
	if (!isModActive(modname))
	{
		PriorityListRemove(modname);
	}
	*/

	ActiveModCacheSave(newActiveModCache);
	updatePriorityList();
	return true;
}

bool ActiveModCacheAdd(std::vector<std::pair<std::string, std::string>> entries){

	std::vector<std::pair<std::string, std::string>> activeModCache = GetActiveModCache();
	for (std::pair<std::string, std::string> entry : entries)
	{
		activeModCache.push_back(entry);
	}
	ActiveModCacheSave(activeModCache);
	return true;
}
bool ActiveModCacheAdd(std::pair<std::string, std::string> entry)
{
	std::vector<std::pair<std::string, std::string>> activeModCache = GetActiveModCache();
	activeModCache.push_back(entry);
	ActiveModCacheSave(activeModCache);
	return true;
}
/*
bool removeActiveModEntry(std::string mod)
{
	const std::string activemods_filename(GetActiveModsFilename());

	auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	std::vector<std::string> new_paths;
	std::vector<std::string> new_filenames;
	for (int i = 0; i < file_count; i++)
	{
		std::string active_path;
		std::string active_mod_name;
		getActiveMod(fp.get(), active_path, active_mod_name);
		if (mod != active_mod_name)
		{
			new_paths.push_back(active_path);
			new_filenames.push_back(active_mod_name);
		}
	}
	fp.reset();
	FileSystem::DeleteFilePath(activemods_filename.c_str());
	const auto newfp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "wb");
	u16 new_filecount = new_paths.size();

	std::fwrite(&new_filecount, sizeof(new_filecount), 1, newfp.get());
	for (int i = 0; i < new_filecount; i++)
	{
		fputs(new_paths[i].c_str(), newfp.get());
		fputc('\0', newfp.get());
		fputs(new_filenames[i].c_str(), newfp.get());
		fputc('\0', newfp.get());
	}
	return true;
}
*/
bool fileFound(u32 mem, std::string filename)
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

bool pathFound(int& path_off, int& file_off, std::string filename)
{
	u32 mem_pointer = 0x0017EF70;
	//Console.WriteLn("Mem pointer: %u", mem_pointer);
	FILE_STR file;
	//Console.WriteLn("file size: %u", sizeof(file));
	for (int i = 0; i < 2; i++) //loop through logo/stmenu structs
	{
		readBytes(mem_pointer, &file, sizeof(file));
		if (fileFound(file.fname_p, filename))
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
		if (fileFound(file.fname_p, filename))
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
			if (offsets[i] != 0 && fileFound(offsets[i], filename))
			{
				path_off = offsets[i];
				file_off = mem_pointer - 4 - sizeof(file) * (5 - (i + 1));
				return true;
			}

		}

	} while (!fileFound(stdat.file1.fname_p, "VS08VS0.INT"));
	olm_struct olm;

	do // Loop through OLM structs
	{
		readBytes(mem_pointer, &olm, sizeof(olm));
		if (fileFound(olm.file.fname_p, filename))
		{
			path_off = olm.file.fname_p;
			file_off = mem_pointer - sizeof(olm);
			return true;
		}

	} while (!fileFound(olm.file.fname_p, "STG00.OLM") || !fileFound(olm.stage_name_pos, "TITLE"));

	return false;
}

bool PatchGamePaths(std::vector<std::string> paths, bool unpatch, std::vector<bool> tmp)
{
	for (int i = 0; i < paths.size(); i++)
	{
		std::string filename = Path::GetFileName(paths[i]).data();
		int off = 0;
		int file_off = 0;
		if (pathFound(off, file_off, filename))
		{
			char patch[24];
			std::string strpath;
			if (unpatch)
			{
				std::string pathUpper(paths[i]);
				transform(pathUpper.begin(), pathUpper.end(), pathUpper.begin(), ::toupper);
				strpath = "host:\\" + pathUpper;
			}
			else if (tmp[i])
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
			continue;
		}

		return false;
	}

	return true;
}

bool PatchGamePaths(std::vector<std::string> paths, bool unpatch)
{
	for (int i = 0; i < paths.size(); i++)
	{
		std::string filename = Path::GetFileName(paths[i]).data();
		int off = 0;
		int file_off = 0;
		if (pathFound(off, file_off, filename))
		{
			char patch[24];
			std::string strpath;
			if (unpatch)
			{
				std::string pathUpper(paths[i]);
				transform(pathUpper.begin(), pathUpper.end(), pathUpper.begin(), ::toupper);
				strpath = "host:\\" + pathUpper;
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
			continue;
		}

		return false;
	}

	return true;
}

bool PatchGamePaths(std::string path, bool unpatch, bool tmp)
{
	std::string filename = Path::GetFileName(path).data();
	int off = 0;
	int file_off = 0;
	if (pathFound(off, file_off, filename))
	{
		char patch[24];
		std::string strpath;
		if (unpatch)
		{
			std::string pathUpper(path);
			transform(pathUpper.begin(), pathUpper.end(), pathUpper.begin(), ::toupper);
			strpath = "host:\\" + pathUpper;
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

bool isIntAsset(std::string path)
{
	std::string extension = Path::GetExtension(path.data()).data();
	transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
	if (extension.find("WP2") == std::string::npos && extension.find("INT") == std::string::npos && extension.find("XTR") == std::string::npos && extension.find("XTR") == std::string::npos)
		return true;
	else
		return false;
}


bool PatchActiveMods()
{
	const std::string activemods_filename(GetActiveModsFilename());

	const auto fp = FileSystem::OpenManagedCFile(activemods_filename.c_str(), "rb");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		return false;

	for (int i = 0; i < file_count; i++)
	{
		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			return false;
		std::string pathname = buf;

		if (!isIntAsset(pathname))
		{
			if (!PatchGamePaths(pathname, false, false))
			{
				return false;
			}
		}
		//necessary to get to next file 
		char buf2[900];
		//fgets puts file offset at end for some reason, so reset it:
		std::fseek(fp.get(), sizeof(file_count) + pathname.length() + 1, SEEK_SET);

		if (fgets(buf2, sizeof(buf2), fp.get()) == nullptr)
			return false;
		std::string modname = buf2;
		//fgets puts file offset at end for some reason, so reset it:
		std::fseek(fp.get(), sizeof(file_count) + pathname.length() + 1 + modname.length() + 1, SEEK_SET); 

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
		std::string active_path;

		long off = ftell(fp.get());

		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			return false;
		active_path = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += active_path.length() + 1;
		std::fseek(fp.get(), off, SEEK_SET);

		std::string active_path_UPPER(active_path);
		transform(active_path_UPPER.begin(), active_path_UPPER.end(), active_path_UPPER.begin(), ::toupper);
		std::string path_UPPER(path);
		transform(path_UPPER.begin(), path_UPPER.end(), path_UPPER.begin(), ::toupper);

		if (Path::GetFileName(active_path_UPPER) == Path::GetFileName(path_UPPER))
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
				}
				else
				{
					//file is still in use, extract it from p2m file again instead
					std::string mod;
					GetActiveModFromPath(path, mod);
					std::string modpath = Path::Combine(EmuFolders::PTR2Mods, mod);

					const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");
					if (!fp)
						return false;

					p2m_header hd;
					if (std::fread(&hd, 64, 1, fp.get()) != 1)
						return false;
					std::fseek(fp.get(), hd.path_offset, SEEK_SET);

					int i;
					bool found = false;
					for (i = 0; i < hd.file_count; i++)
					{
						char buf[50];
						if (fgets(buf, sizeof(buf), fp.get()) != nullptr)
						{
							std::string foundpath = buf;
							if (foundpath.find(filename) != std::string::npos)
							{
								found = true;
								break;
							}
						}
						//paths.push_back();
					}
					if (!found)
					{
						//should never happen
						return false;
					}

					std::fseek(fp.get(), hd.size_offset + (8 * i), SEEK_SET);
					int filesize;
					int filepos;
					std::fread(&filesize, 4, 1, fp.get());
					std::fread(&filepos, 4, 1, fp.get());

					std::fseek(fp.get(), filepos, SEEK_SET);

					const auto newfp = FileSystem::OpenManagedCFile(pathMOD.c_str(), "w+b");

					const int buf_size = 4096;
					u8 buf[buf_size];
					int total_copied = buf_size;
					while (filesize > total_copied)
					{

						std::fread(&buf, sizeof(buf[0]), buf_size, fp.get());
						std::fwrite(&buf, sizeof(buf[0]), buf_size, newfp.get());
						total_copied += buf_size;
					}
					std::fread(&buf, filesize + buf_size - total_copied, 1, fp.get());
					std::fwrite(&buf, filesize + buf_size - total_copied, 1, newfp.get());

					newpaths.push_back(pathTMP); //add to delete cache
				}
				if (!isIntAsset(pathMOD))
					PatchGamePaths(pathMOD, false, false);
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

//returns list of modnames in order from highest to lowest priority (0 first
std::vector<std::string> GetModsPriorityList()
{
	const std::string modspriority_filename(GetModPriorityFilename());

	auto fp = FileSystem::OpenManagedCFile(modspriority_filename.c_str(), "rb+");
	u16 file_count;
	if (std::fread(&file_count, 2, 1, fp.get()) != 1)
		Console.WriteLn("Error reading modspriority list: bad file count");

	std::vector<std::string> modnames_list;
	for (int i = 0; i < file_count; i++)
	{
		std::string found_mod_name;

		long off = ftell(fp.get());

		char buf[900];
		if (fgets(buf, sizeof(buf), fp.get()) == nullptr)
			Console.WriteLn("Error reading modspriority list: bad mod name");
		found_mod_name = buf;

		//fgets puts file offset at end for some reason, so reset it:
		off += found_mod_name.length() + 1;
		std::fseek(fp.get(), off, SEEK_SET);

		modnames_list.push_back(found_mod_name);
	}
	fp.reset();
	return modnames_list;
}
bool PriorityListSave(std::vector<std::string> priority_list)
{
	const std::string modspriority_filename(GetModPriorityFilename());
	FileSystem::DeleteFilePath(modspriority_filename.c_str());

	const auto fp = FileSystem::OpenManagedCFile(modspriority_filename.c_str(), "wb");
	u16 file_count = priority_list.size();
	std::fwrite(&file_count, sizeof(file_count), 1, fp.get());
	for (std::string modname : priority_list)
	{
		fputs(modname.c_str(), fp.get());
		fputc('\0', fp.get());
	}
	return true;
}
bool PriorityListAdd(std::string modname, int index)
{
	std::vector<std::string> priority_list = GetModsPriorityList();

	std::vector<std::string> new_priority_list;
	for (int i = 0; i < priority_list.size() + 1; i++)
	{
		if (i == index)
		{
			new_priority_list.push_back(modname);
		}
		else if (i > index)
		{
			new_priority_list.push_back(priority_list[i - 1]);
		}
		else //if i < index
		{
			new_priority_list.push_back(priority_list[i]);
		}
	}

	return PriorityListSave(new_priority_list);

}
//default add to list at highest priority
bool PriorityListAdd(std::string modname)
{
	return PriorityListAdd(modname, 0);
}


bool PriorityListRemove(std::string modname)
{
	std::vector<std::string> priority_list = GetModsPriorityList();

	std::vector<std::string> new_priority_list;
	for (std::string list_modname : priority_list)
	{
		if (modname != list_modname)
		{
			new_priority_list.push_back(list_modname);
		}
	}

	return PriorityListSave(new_priority_list);
}


bool updatePriorityList()
{
	std::vector<std::string> list = GetModsPriorityList();
	for (std::string mod : list)
	{
		if (!isModActive(mod))
		{
			PriorityListRemove(mod);
		}
	}
	return true;
}

std::string GetPathFromModName(std::string modname)
{
	return Path::Combine(EmuFolders::PTR2Mods, modname);
}

std::vector<std::string> GetP2MPaths(FILE* stream, std::string modpath, p2m_header& hd)
{
	//make sure at beginning
	std::fseek(stream, 0, SEEK_SET);

	std::fread(&hd, 64, 1, stream);

	std::fseek(stream, hd.path_offset, SEEK_SET);

	std::vector<std::string> paths;
	long off = ftell(stream);
	for (int i = 0; i < hd.file_count; i++)
	{
		char buf[50];
		if (fgets(buf, sizeof(buf), stream) != nullptr)
		{
			std::string path = buf;
			paths.push_back(path);
			//fgets puts fp to end, so reset it;
			off += path.length() + 1;
			std::fseek(stream, off, SEEK_SET);
		}
		//paths.push_back();
	}
	return paths;
}
std::vector<std::string> GetP2MPaths(std::string modpath)
{
	const auto fp = FileSystem::OpenManagedCFile(modpath.c_str(), "rb");

	p2m_header hd;
	std::fread(&hd, 64, 1, fp.get());
	std::fseek(fp.get(), hd.path_offset, SEEK_SET);

	std::vector<std::string> paths;
	long off = ftell(fp.get());
	for (int i = 0; i < hd.file_count; i++)
	{
		char buf[50];
		if (fgets(buf, sizeof(buf), fp.get()) != nullptr)
		{
			std::string path = buf;
			paths.push_back(path);
			//fgets puts fp to end, so reset it;
			off += path.length() + 1;
			std::fseek(fp.get(), off, SEEK_SET);
		}
		//paths.push_back();
	}
	return paths;
}
std::vector<std::string> GetP2MPathsByModName(std::string modname)
{
	std::string filename = GetPathFromModName(modname);
	return GetP2MPaths(filename);
}

bool disableMod(std::string modname)
{
	//get files of mod
	std::vector<std::string> modpaths;
	if (!findActiveModPaths(modname, modpaths))
		return false;

	//delete files from MOD folder
	for (std::string file : modpaths)
	{
		std::string mod_file = GetModFilePath(file);
		
		if (!FileSystem::DeleteFilePath(mod_file.c_str()))
		{
			addDeleteEntry(Path::MakeRelative(mod_file, EmuFolders::PTR2));
			files_to_delete = true;
		}
	}

	//remove from activemods
	ActiveModCacheRemoveMod(modname);
	//removeActiveModEntry(modname);

	//remove from prioritylist
	PriorityListRemove(modname);

	std::vector<std::string> unpatch_paths;
	for (std::string path : modpaths)
	{
		if (!isIntAsset(path))
		{
			unpatch_paths.push_back(path);
		}
	}

	//remove patched paths from game memory
	PatchGamePaths(unpatch_paths, true);

	return true;
}

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
bool moveModFile(FILE* stream, p2m_header hd, std::string path, bool tmp)
{
	//get index of the file in the p2m

	std::fseek(stream, hd.path_offset, SEEK_SET);

	std::vector<std::string> paths;
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

	std::string mod_file;
	if (tmp)
		mod_file = Path::Combine(Path::Combine(EmuFolders::PTR2, "/TMP"), Path::GetFileName(path));
	else
		mod_file = GetModFilePath(path);

	std::string mod_path = std::string(Path::GetDirectory(mod_file));

	//copy file at that index
	std::fseek(stream, hd.size_offset + (8 * i), SEEK_SET);
	int filesize, filepos;
	std::fread(&filesize, 4, 1, stream);
	std::fread(&filepos, 4, 1, stream);

	std::fseek(stream, filepos, SEEK_SET);

	FileSystem::EnsureDirectoryExists(mod_path.c_str(), true);
	const auto newfp = FileSystem::OpenManagedCFile(mod_file.c_str(), "w+b");
	const auto new_stream = newfp.get();

	return copyStream(stream, new_stream, filesize);
	
}
bool moveAllModFiles(FILE* stream, p2m_header hd, std::vector<std::string> paths, std::vector<bool> tmp)
{

	for (int i = 0; i < hd.file_count; i++)
	{
		std::fseek(stream, hd.size_offset + (8 * i), SEEK_SET);
		int filesize;
		int filepos;
		std::fread(&filesize, 4, 1, stream);
		std::fread(&filepos, 4, 1, stream);

		std::fseek(stream, filepos, SEEK_SET);

		std::string path = paths[i];
		std::string mod_file;
		if (tmp[i])
		{
			mod_file = Path::Combine(Path::Combine(EmuFolders::PTR2, "/TMP"), Path::GetFileName(path));
		}
		else
		{
			mod_file = GetModFilePath(path);
		}
		std::string mod_path = std::string(Path::GetDirectory(mod_file));
		FileSystem::EnsureDirectoryExists(mod_path.c_str(), true);
		const auto newfp = FileSystem::OpenManagedCFile(mod_file.c_str(), "w+b");
		const auto new_stream = newfp.get();

		copyStream(stream, new_stream, filesize);
	}
	return true;
}

bool disableModEntry(std::string path)
{
	ActiveModCacheRemovePath(path);

	std::string mod_file = GetModFilePath(path);

	if (!FileSystem::DeleteFilePath(mod_file.c_str()))
	{
		addDeleteEntry(Path::MakeRelative(mod_file, EmuFolders::PTR2));
		files_to_delete = true;
	}

	PatchGamePaths(path, true, false);
	return true;
}
bool enableMod(std::string filename)
{
	const auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	//get files of mod
	p2m_header hd;
	std::vector<std::string> paths = GetP2MPaths(fp.get(), filename, hd);

	//check activemods
		//if files exist, disable mods associated
	std::vector<bool> tmp; 
	for (std::string path : paths)
	{
		std::string existing_mod;
		if (GetActiveModFromPath(path, existing_mod) == true)
		{

			//disableMod(existing_mod);
			disableModEntry(path);
			
			//if file in delete cache, mark it to be put in TMP
			if (isInDeleteCache(path) && !isIntAsset(path)) //dont count int asset as TMP workaround isnt necessary
			{
				tmp.push_back(true);
				continue;
			}
		}
		tmp.push_back(false);
	}
	//move files to MOD
	moveAllModFiles(fp.get(), hd, paths, tmp);

	//dont patch files from unpacked INT/XTR files as they aren't in the ISO paths
	//they are loaded using PTR2Hooks, which will use the activemod cache file

	std::vector<std::string> patch_paths;
	std::vector<bool> patch_tmp;
	for (int i = 0; i < hd.file_count; i++)
	{
		if (!isIntAsset(paths[i]))
		{
			patch_paths.push_back(paths[i]);
			patch_tmp.push_back(tmp[i]);
		}
	}

	PatchGamePaths(patch_paths, false, patch_tmp);

	//add to activemods
	std::string mod = Path::GetFileName(filename).data();
	std::vector<std::pair<std::string, std::string>> entries;
	for (std::string path : paths)
	{
		std::pair<std::string, std::string> entry(path, mod);
		entries.push_back(entry);
	}
	ActiveModCacheAdd(entries);
	//addActiveModEntry(mod, paths);

	//add to prioritylist
	PriorityListAdd(mod);

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

	bool tmp = false;
	//if file in delete cache, mark it to be put in TMP
	if (isInDeleteCache(path) && !isIntAsset(path)) //dont count int asset as TMP workaround isnt necessary
	{
		tmp = true;
	}

	moveModFile(fp.get(), hd, path, tmp);

	if (!isIntAsset(path))
	{
		PatchGamePaths(path, false, tmp);
	}

	return true;
	
}

bool RefreshMods() 
{
	std::vector<std::string> priorities = GetModsPriorityList();
	int mod_count = priorities.size();
	
	//get list of mod entries after priorities
	std::map<std::string, std::string> entries; //we dont need order but map is probably better than unordered anyway
	for (int i = mod_count - 1; i > -1; i--) //iterate lowest priority first
	{
		std::string modname = priorities[i];
		std::vector<std::string> paths = GetP2MPathsByModName(modname);
		for (std::string path : paths)
		{
			entries.insert_or_assign(path, modname);
			//entries.push_back(entry);
		}
	}

	//remove entries already applied
	std::vector<std::pair<std::string, std::string>> cache = GetActiveModCache();
	for (std::pair<std::string, std::string> entry : cache)
	{
		if ( entries[entry.first] == entry.second )
		{
			entries.erase(entry.first);
		}
		else //remove entries we are going to replace
			ActiveModCacheRemovePath(entry.first);
	}

	//convert map to vector of pairs, so we can access both key and value
	std::vector<std::pair<std::string, std::string>> new_cache;
	new_cache.assign(entries.begin(), entries.end());

	//apply our new entries
	for (std::pair<std::string, std::string> entry : new_cache)
	{
		ApplySingleModEntry(entry.first, entry.second);
		ActiveModCacheAdd(entry);
	}
	return true;
}

bool PriorityListAdjust(std::string modname, int newIndex)
{
	PriorityListRemove(modname);
	PriorityListAdd(modname, newIndex);
	
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