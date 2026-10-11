#include "Platform.h"
#include <shellapi.h>
#include <shlobj.h>
#include <propkey.h>
#include <propvarutil.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <algorithm>

namespace launcher
{
void Check(HRESULT result, const char *context)
{
    if (FAILED(result))
    {
        std::ostringstream out;
        out << context << " (0x" << std::hex << std::uppercase << static_cast<unsigned long>(result) << ')';
        throw std::runtime_error(out.str());
    }
}

std::wstring Wide(const std::string &text)
{
    if (text.empty())
    {
        return {};
    }
    const auto length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!length)
    {
        throw std::runtime_error("Invalid UTF-8 text in settings");
    }
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                        length);
    return result;
}

std::string Utf8(const std::wstring &text)
{
    if (text.empty())
    {
        return {};
    }
    const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                            nullptr, 0, nullptr, nullptr);
    if (!length)
    {
        throw std::runtime_error("Invalid Unicode text");
    }
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                        length, nullptr, nullptr);
    return result;
}

std::filesystem::path ExecutablePath()
{
    std::wstring buffer(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!size || size >= buffer.size())
    {
        throw std::runtime_error("Cannot locate launcher executable");
    }
    buffer.resize(size);
    return buffer;
}

namespace
{
std::filesystem::path KnownFolder(REFKNOWNFOLDERID id)
{
    PWSTR raw = nullptr;
    Check(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw), "Locate Windows user folder");
    std::filesystem::path result(raw);
    CoTaskMemFree(raw);
    return result;
}
} // namespace

std::filesystem::path DataDirectory()
{
    const auto path = KnownFolder(FOLDERID_LocalAppData) / L"CTech" / L"VisualStudioLauncher";
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path SettingsPath()
{
    return DataDirectory() / L"settings.json";
}

Settings LoadSettings()
{
    const auto path = SettingsPath();
    if (!std::filesystem::exists(path))
    {
        return {};
    }
    if (std::filesystem::file_size(path) > 4 * 1024 * 1024)
    {
        throw std::runtime_error("Settings file exceeds 4 MiB; the file was not changed");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot read settings.json");
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad())
    {
        throw std::runtime_error("Error reading settings.json");
    }
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0)
    {
        text.erase(0, 3);
    }
    Wide(text); // Validate Unicode before accepting the file.
    return ParseSettings(text);
}

void SaveSettings(const Settings &settings)
{
    const auto path = SettingsPath();
    // A unique temporary file prevents concurrent --refresh processes colliding.
    const auto temporary = path.parent_path() / (L"settings-" + Wide(NewId()) + L".tmp");
    const auto backup = path.parent_path() / L"settings.previous.json";
    const auto bytes = SerializeSettings(settings);
    const HANDLE file =
        CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot create settings temporary file");
    }
    DWORD written = 0;
    const bool okay = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
                      written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!okay)
    {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Cannot write settings; existing settings were preserved");
    }
    bool replaced = false;
    if (std::filesystem::exists(path))
    {
        replaced = ReplaceFileW(path.c_str(), temporary.c_str(), backup.c_str(), 0, nullptr, nullptr) != FALSE;
    }
    else
    {
        replaced = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!replaced)
    {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Cannot replace settings file; close other applications using it and try again");
    }
}

std::string NewId()
{
    GUID guid{};
    Check(CoCreateGuid(&guid), "Generate entry ID");
    wchar_t text[40]{};
    StringFromGUID2(guid, text, 40);
    return Utf8(std::wstring(text + 1, 36));
}

std::filesystem::path EntryIcon(const Entry &entry)
{
    if (!entry.icon.empty())
    {
        const std::filesystem::path source(Wide(entry.icon));
        if (!std::filesystem::is_regular_file(source))
        {
            throw std::runtime_error("Custom icon does not exist: " + entry.icon);
        }
        // Retain a managed copy: moving the original icon will not break the taskbar.
        const auto directory = DataDirectory() / L"icons";
        std::filesystem::create_directories(directory);
        std::ifstream input(source, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Cannot read custom icon: " + entry.icon);
        }
        std::uint64_t hash = 14695981039346656037ull;
        char value = 0;
        while (input.get(value))
        {
            hash ^= static_cast<unsigned char>(value);
            hash *= 1099511628211ull;
        }
        if (input.bad())
        {
            throw std::runtime_error("Error reading custom icon: " + entry.icon);
        }
        std::ostringstream suffix;
        suffix << std::hex << hash;
        const auto destination = directory / (Wide(entry.id + "-" + suffix.str()) + L".ico");
        if (source != destination)
        {
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
        }
        return destination;
    }
    const auto directory = DataDirectory() / L"icons";
    std::filesystem::create_directories(directory);
    const auto path = directory / (Wide(entry.color.substr(1)) + L".ico");
    if (!std::filesystem::exists(path))
    {
        WriteColorIcon(path, entry.color);
    }
    return path;
}

void OpenSolution(const Settings &settings, const Entry &entry, HWND owner)
{
    const std::filesystem::path solution(Wide(entry.path));
    if (!IsSolutionPath(entry.path) || !solution.is_absolute() || !std::filesystem::is_regular_file(solution))
    {
        throw std::runtime_error("Solution not found:\n" + entry.path + "\n\nCorrect the path in the launcher.");
    }
    const auto studio = entry.visualStudio.empty() ? settings.visualStudio : entry.visualStudio;
    std::wstring executable;
    std::wstring arguments;
    if (!studio.empty())
    {
        const std::filesystem::path path(Wide(studio));
        if (!path.is_absolute() || !std::filesystem::is_regular_file(path))
        {
            throw std::runtime_error("Visual Studio executable not found:\n" + studio);
        }
        executable = path.wstring();
        arguments = QuoteArgument(solution.wstring());
    }
    else
    {
        executable = solution.wstring();
    }
    const auto directory = solution.parent_path().wstring();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    info.hwnd = owner;
    info.lpVerb = L"open";
    info.lpFile = executable.c_str();
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = directory.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info))
    {
        throw std::runtime_error(
            "Cannot open the solution (Windows error " + std::to_string(GetLastError()) +
            ").\nChoose a devenv.exe in the launcher if the solution file association is missing.");
    }
}

namespace
{
void SetTextProperty(IPropertyStore *store, REFPROPERTYKEY key, const std::wstring &text)
{
    PROPVARIANT value{};
    Check(InitPropVariantFromString(text.c_str(), &value), "Create shell property");
    const auto result = store->SetValue(key, value);
    PropVariantClear(&value);
    Check(result, "Set shell property");
}

ComPtr<IShellLinkW> Link(const std::wstring &title, const std::wstring &arguments, const std::filesystem::path &icon,
                         const std::wstring &description)
{
    ComPtr<IShellLinkW> link;
    Check(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.Put())),
          "Create shell link");
    const auto exe = ExecutablePath();
    Check(link->SetPath(exe.c_str()), "Set shell link target");
    Check(link->SetArguments(arguments.c_str()), "Set shell link arguments");
    Check(link->SetWorkingDirectory(exe.parent_path().c_str()), "Set shell link directory");
    Check(link->SetIconLocation(icon.c_str(), 0), "Set shell link icon");
    Check(link->SetDescription(description.substr(0, 250).c_str()), "Set shell link description");
    ComPtr<IPropertyStore> store;
    Check(link->QueryInterface(IID_PPV_ARGS(store.Put())), "Get shell link properties");
    SetTextProperty(store.Get(), PKEY_Title, title);
    SetTextProperty(store.Get(), PKEY_AppUserModel_ID, AppId);
    Check(store->Commit(), "Commit shell link properties");
    return link;
}

ComPtr<IObjectCollection> Collection()
{
    ComPtr<IObjectCollection> result;
    Check(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(result.Put())),
          "Create shell link collection");
    return result;
}

std::set<std::wstring> RemovedArguments(IObjectArray *array)
{
    std::set<std::wstring> result;
    UINT count = 0;
    Check(array->GetCount(&count), "Read removed Jump List items");
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IShellLinkW> link;
        if (SUCCEEDED(array->GetAt(i, IID_PPV_ARGS(link.Put()))))
        {
            wchar_t buffer[32768]{};
            if (SUCCEEDED(link->GetArguments(buffer, 32768)))
            {
                result.insert(buffer);
            }
        }
    }
    return result;
}

class JumpTransaction
{
  public:
    explicit JumpTransaction(ICustomDestinationList *list) : list_(list)
    {
    }
    ~JumpTransaction()
    {
        if (!committed_)
        {
            list_->AbortList();
        }
    }
    void Commit()
    {
        Check(list_->CommitList(), "Publish Jump List");
        committed_ = true;
    }

  private:
    ICustomDestinationList *list_;
    bool committed_ = false;
};
} // namespace

std::wstring PublishJumpList(const Settings &settings)
{
    ComPtr<ICustomDestinationList> list;
    Check(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(list.Put())),
          "Create Jump List");
    Check(list->SetAppID(AppId), "Identify Jump List");
    UINT available = 0;
    ComPtr<IObjectArray> removed;
    Check(list->BeginList(&available, IID_PPV_ARGS(removed.Put())), "Begin Jump List");
    JumpTransaction transaction(list.Get());
    const auto hidden = RemovedArguments(removed.Get());
    auto tasks = Collection();
    auto manage =
        Link(L"Manage solutions...", L"--manage", ExecutablePath(), L"Edit the manually configured solution list");
    Check(tasks->AddObject(manage.Get()), "Add manager task");

    std::vector<std::string> groups;
    size_t visible = 0;
    for (const auto &entry : settings.entries)
    {
        const auto group = entry.group.empty() ? "Solutions" : entry.group;
        if (entry.enabled && std::find(groups.begin(), groups.end(), group) == groups.end())
        {
            groups.push_back(group);
        }
    }
    bool fallback = false;
    bool truncated = false;
    for (const auto &group : groups)
    {
        auto items = Collection();
        std::vector<const Entry *> included;
        for (const auto &entry : settings.entries)
        {
            if (!entry.enabled || (entry.group.empty() ? "Solutions" : entry.group) != group)
            {
                continue;
            }
            const auto arguments = L"--open " + Wide(entry.id);
            if (hidden.count(arguments))
            {
                continue;
            } // Honor removal through the native menu.
            if (visible >= available)
            {
                truncated = true;
                continue;
            }
            auto link = Link(Wide(DisplayName(entry)), arguments, EntryIcon(entry), Wide(entry.path));
            Check(items->AddObject(link.Get()), "Add solution to Jump List");
            included.push_back(&entry);
            ++visible;
        }
        if (included.empty())
        {
            continue;
        }
        ComPtr<IObjectArray> array;
        Check(items->QueryInterface(IID_PPV_ARGS(array.Put())), "Get category items");
        const auto result = list->AppendCategory(Wide(group).c_str(), array.Get());
        if (result == E_ACCESSDENIED || result == static_cast<HRESULT>(0x80040F03L))
        {
            fallback = true;
            // Windows permits user tasks when custom categories are blocked.
            // Keep the manual launch commands with their icons and group labels.
            for (const auto *entry : included)
            {
                auto link = Link(Wide(group + " - " + DisplayName(*entry)), L"--open " + Wide(entry->id),
                                 EntryIcon(*entry), Wide(entry->path));
                Check(tasks->AddObject(link.Get()), "Add fallback launch command");
            }
        }
        else
        {
            Check(result, "Add Jump List category");
        }
    }
    ComPtr<IObjectArray> taskArray;
    Check(tasks->QueryInterface(IID_PPV_ARGS(taskArray.Put())), "Get Jump List tasks");
    Check(list->AddUserTasks(taskArray.Get()), "Add Jump List tasks");
    transaction.Commit();
    std::wstring status = L"Saved. Right-click the pinned launcher icon to open a solution.";
    if (fallback)
    {
        status = L"Saved. Windows blocked categories; solutions appear under Tasks with colored icons.";
    }
    if (truncated)
    {
        status += L" Windows' menu limit hides some entries; all remain available here.";
    }
    if (!hidden.empty())
    {
        status += L" Entries removed through Windows remain hidden; duplicate an entry to restore it.";
    }
    return status;
}

std::filesystem::path CreateStartMenuShortcut()
{
    auto link = Link(L"Visual Studio Launcher", L"--manage", ExecutablePath(),
                     L"Manage and launch your Visual Studio solutions");
    ComPtr<IPersistFile> file;
    Check(link->QueryInterface(IID_PPV_ARGS(file.Put())), "Save launcher shortcut");
    const auto shortcut = KnownFolder(FOLDERID_Programs) / L"Visual Studio Launcher.lnk";
    Check(file->Save(shortcut.c_str(), TRUE), "Create Start menu shortcut");
    return shortcut;
}
} // namespace launcher
