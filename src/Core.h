#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace launcher
{
struct Entry
{
    std::string id;
    std::string name;
    std::string group = "Solutions";
    std::string branch;
    bool branchFromGit = false; // Show the checked-out Git branch instead of the manual label.
    std::string path;
    std::string color = "#2563EB";
    std::string icon;
    std::string visualStudio;
    bool enabled = true;
};

struct Settings
{
    std::string visualStudio;
    std::vector<Entry> entries;
};

Settings ParseSettings(const std::string &json);
std::string SerializeSettings(const Settings &settings);
std::string DisplayName(const Entry &entry);
std::string DisplayName(const std::string &name, const std::string &branch);
// Branch name from the contents of a .git/HEAD file; a short hash when detached, empty when unrecognised.
std::string BranchFromHead(const std::string &head);
bool IsValidId(const std::string &id);
bool IsValidColor(const std::string &color);
bool IsSolutionPath(const std::string &path);
// Quote exactly one Windows argv argument, including trailing backslashes.
std::wstring QuoteArgument(const std::wstring &argument);
} // namespace launcher
