#include <common/FileSystem.h>
#include <pcsx2/Config.h>

#include <fstream>
#include <common/Path.h>

/* void ListMods(){
	//FileSystem::FindFiles(EmuFolders::ptr2mods.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &results);

}*/

static constexpr u32 P2M_HEADER_SIZE = 64;

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

static_assert(sizeof(p2m_header) == P2M_HEADER_SIZE, "p2m_header struct not packed to 64 bytes");

/*
struct metadata
{
	std::string title;
	std::string author;
	std::string desc;
};
*/
/*
static std::string GetModCacheFilename()
{
	return Path::Combine(EmuFolders::Cache, "activemods.cache");
}

bool disableMod(const char* filename)
{
	//get files of mod
	//delete files from MOD
	
	//remove from activemods

}

bool enableMod(const char* filename)
{
	//get files of mod
	const auto fp = FileSystem::OpenManagedCFile(filename, "rb");
	if (!fp)
		return false;

	p2m_header hd;
	if (std::fread(&hd, 64, 1, fp.get()) != 1)
		return false;
	std::fseek(fp.get(), hd.path_offset, SEEK_SET);

	std::vector<std::string> paths;
	for (int i = 1; i <= hd.file_count; i++)
	{
		char buf[50];
		if (fgets(buf, sizeof(buf), fp.get()) != nullptr)
		{
			std::string path = buf;
			paths.push_back(path);
		}
		//paths.push_back();
	}

	const std::string activemods_filename(GetModCacheFilename());


	//check activemods
		//if files exist, disable mods associated

	//move files to MOD

	//add to activemods
}

bool findActiveMod*/

bool IsP2M(const char* filename, std::string& title, std::string& author, std::string& description)
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