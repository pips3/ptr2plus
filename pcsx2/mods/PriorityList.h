#pragma once
namespace PriorityList
{
	static std::string GetFilename();
	std::vector<std::string> Get();
	bool Save(std::vector<std::string> priority_list);
	bool Add(std::string modname, int index);
	bool Push(std::string modname);
	bool Remove(std::string modname);
	bool Save(std::vector<std::string> priority_list);

}