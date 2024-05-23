#pragma once

#include <string>
#include <vector>

extern bool files_to_delete;
extern bool TryDeleteFiles();
extern bool isModActive(const std::string mod);
extern bool GetActiveModFromPath(const std::string path, std::string& mod);
extern bool findActiveModPaths(const std::string mod, std::vector<std::string>& paths);

extern bool removeActiveModEntry(std::string mod);
extern bool addActiveModEntry(std::string filename, std::vector<std::string> paths);

extern std::vector<std::string> GetModsPriorityList();
extern bool PriorityListAdjust(std::string modname, int newIndex);

extern bool PatchActiveMods();
extern bool disableMod(std::string modname);
extern bool enableMod(std::string filename);

extern bool IsP2M(const char* filename, std::string& title, std::string& author, std::string& description, bool& enabled);