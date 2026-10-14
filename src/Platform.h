#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <filesystem>
#include <stdexcept>
#include "Core.h"

namespace launcher
{
inline constexpr wchar_t AppId[] = L"CTech.VisualStudioLauncher";

template <class T> class ComPtr
{
  public:
    ComPtr() = default;
    ~ComPtr()
    {
        if (value_)
        {
            value_->Release();
        }
    }
    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;
    ComPtr(ComPtr &&other) noexcept : value_(other.value_)
    {
        other.value_ = nullptr;
    }
    T *operator->() const
    {
        return value_;
    }
    T *Get() const
    {
        return value_;
    }
    T **Put()
    {
        if (value_)
        {
            value_->Release();
            value_ = nullptr;
        }
        return &value_;
    }

  private:
    T *value_ = nullptr;
};

void Check(HRESULT result, const char *context);
std::wstring Wide(const std::string &text);
std::string Utf8(const std::wstring &text);
std::filesystem::path ExecutablePath();
std::filesystem::path DataDirectory();
std::filesystem::path SettingsPath();
Settings LoadSettings();
void SaveSettings(const Settings &settings);
std::string NewId();
std::filesystem::path EntryIcon(const Entry &entry);
// Branch checked out in the Git repository containing the solution; empty when there is none.
std::string GitBranch(const std::string &solutionPath);
// DisplayName with the branch taken from Git when the entry asks for it.
std::string EntryDisplayName(const Entry &entry);
void OpenSolution(const Settings &settings, const Entry &entry, HWND owner);
// Visual Studio's recent solutions, merged across installed (non-experimental) instances, newest first.
std::vector<RecentSolution> LoadRecentSolutions();
std::wstring PublishJumpList(const Settings &settings);
std::filesystem::path CreateStartMenuShortcut();
void WriteColorIcon(const std::filesystem::path &path, const std::string &color);
} // namespace launcher
