#pragma once
namespace PriorityList
{
	static std::string GetFilename();
	std::vector<std::string> Get();
	bool GetPriority(std::string modname, int& priority);
	bool GetModName(int priority, std::string& modname);
	bool Save(std::vector<std::string> priority_list);
	bool Add(std::string modname, int index);
	bool Push(std::string modname);
	bool Remove(std::string modname);
	bool Save(std::vector<std::string> priority_list);
	bool ContainsMod(std::string modname)

}