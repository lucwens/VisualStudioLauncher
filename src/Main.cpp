#include "Platform.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <memory>
#include <cstdio>

namespace
{
using namespace launcher;
constexpr wchar_t ManagerMutex[] = L"Local\\CTech.VisualStudioLauncher.Manager";
constexpr wchar_t ManagerProperty[] = L"CTech.VisualStudioLauncher.Manager.Window";

struct Manager
{
    Settings settings;
    int selected = -1;
    bool loading = false;
    bool dirty = false;
    HIMAGELIST images = nullptr;
};

Manager &State(HWND dialog)
{
    return *reinterpret_cast<Manager *>(GetWindowLongPtrW(dialog, DWLP_USER));
}

void Error(HWND owner, const std::exception &error)
{
    std::wstring message;
    try
    {
        message = Wide(error.what());
    }
    catch (...)
    {
        message = L"A Windows operation failed.";
    }
    MessageBoxW(owner, message.c_str(), L"Visual Studio Launcher", MB_OK | MB_ICONERROR);
}

std::wstring Text(HWND dialog, int id)
{
    const HWND control = GetDlgItem(dialog, id);
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(length);
    return value;
}

void Text(HWND dialog, int id, const std::string &value)
{
    SetDlgItemTextW(dialog, id, Wide(value).c_str());
}

void Status(HWND dialog, const std::wstring &text)
{
    SetDlgItemTextW(dialog, IDC_STATUS, text.c_str());
}

Entry *Selected(HWND dialog)
{
    auto &state = State(dialog);
    if (state.selected < 0 || state.selected >= static_cast<int>(state.settings.entries.size()))
    {
        return nullptr;
    }
    return &state.settings.entries[state.selected];
}

void Column(HWND list, int row, int column, const std::string &text)
{
    auto value = Wide(text);
    ListView_SetItemText(list, row, column, value.data());
}

void UpdateRow(HWND dialog, int row)
{
    const auto &entry = State(dialog).settings.entries[row];
    const HWND list = GetDlgItem(dialog, IDC_LIST);
    Column(list, row, 0, EntryDisplayName(entry));
    Column(list, row, 1, entry.group.empty() ? "Solutions" : entry.group);
    Column(list, row, 2, entry.path);
    Column(list, row, 3, entry.enabled ? "Yes" : "No");
}

// Show the manual label, or the detected Git branch read-only.
void ShowBranch(HWND dialog, const Entry *entry)
{
    auto &state = State(dialog);
    const bool fromGit = entry && entry->branchFromGit;
    const bool loading = state.loading;
    state.loading = true;
    SendDlgItemMessageW(dialog, IDC_BRANCH_SOURCE, CB_SETCURSEL, fromGit ? 1 : 0, 0);
    Text(dialog, IDC_BRANCH, !entry ? "" : (fromGit ? GitBranch(entry->path) : entry->branch));
    SendDlgItemMessageW(dialog, IDC_BRANCH, EM_SETREADONLY, fromGit, 0);
    state.loading = loading;
}

void ReadEditor(HWND dialog)
{
    auto &state = State(dialog);
    if (state.loading)
    {
        return;
    }
    if (auto *entry = Selected(dialog))
    {
        entry->name = Utf8(Text(dialog, IDC_NAME));
        entry->group = Utf8(Text(dialog, IDC_GROUP));
        const auto path = Utf8(Text(dialog, IDC_PATH));
        if (!entry->branchFromGit)
        {
            entry->branch = Utf8(Text(dialog, IDC_BRANCH));
        }
        else if (path != entry->path)
        {
            entry->path = path;
            ShowBranch(dialog, entry);
        }
        entry->path = path;
        entry->icon = Utf8(Text(dialog, IDC_CUSTOM_ICON));
        entry->visualStudio = Utf8(Text(dialog, IDC_STUDIO));
        entry->enabled = IsDlgButtonChecked(dialog, IDC_ENABLED) == BST_CHECKED;
        UpdateRow(dialog, state.selected);
    }
    state.settings.visualStudio = Utf8(Text(dialog, IDC_DEFAULT_STUDIO));
    state.dirty = true;
    Status(dialog, L"Unsaved changes. Save / update menu publishes the taskbar list. Blank default VS uses the Windows "
                   L"file association.");
}

void PopulateEditor(HWND dialog)
{
    auto &state = State(dialog);
    state.loading = true;
    const auto *entry = Selected(dialog);
    const Entry empty;
    const auto &value = entry ? *entry : empty;
    Text(dialog, IDC_NAME, entry ? value.name : "");
    Text(dialog, IDC_GROUP, entry ? value.group : "");
    ShowBranch(dialog, entry);
    Text(dialog, IDC_PATH, value.path);
    Text(dialog, IDC_CUSTOM_ICON, value.icon);
    Text(dialog, IDC_STUDIO, value.visualStudio);
    Text(dialog, IDC_COLOR, value.color);
    CheckDlgButton(dialog, IDC_ENABLED, value.enabled && entry ? BST_CHECKED : BST_UNCHECKED);
    for (const auto id :
         {IDC_NAME, IDC_GROUP, IDC_BRANCH_SOURCE, IDC_BRANCH, IDC_PATH, IDC_CUSTOM_ICON, IDC_STUDIO, IDC_COLOR,
          IDC_ENABLED, IDC_BROWSE_PATH, IDC_BROWSE_ICON, IDC_USE_COLOR, IDC_BROWSE_STUDIO, IDC_DUPLICATE, IDC_REMOVE, IDC_OPEN})
    {
        EnableWindow(GetDlgItem(dialog, id), entry != nullptr);
    }
    EnableWindow(GetDlgItem(dialog, IDC_UP), entry && state.selected > 0);
    EnableWindow(GetDlgItem(dialog, IDC_DOWN),
                 entry && state.selected + 1 < static_cast<int>(state.settings.entries.size()));
    state.loading = false;
}

void RefreshList(HWND dialog)
{
    auto &state = State(dialog);
    const HWND list = GetDlgItem(dialog, IDC_LIST);
    state.loading = true;
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    const auto oldImages = state.images;
    const UINT dpi = GetDpiForWindow(dialog);
    const int size = MulDiv(20, dpi, 96);
    state.images = ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 8, 8);
    ListView_SetImageList(list, state.images, LVSIL_SMALL);
    if (oldImages)
    {
        ImageList_Destroy(oldImages);
    }
    for (size_t i = 0; i < state.settings.entries.size(); ++i)
    {
        const auto &entry = state.settings.entries[i];
        HICON icon = nullptr;
        try
        {
            const auto path = EntryIcon(entry);
            icon = static_cast<HICON>(LoadImageW(nullptr, path.c_str(), IMAGE_ICON, size, size, LR_LOADFROMFILE));
        }
        catch (...)
        {
        } // Incomplete editor paths must not interrupt typing.
        if (!icon)
        {
            icon = static_cast<HICON>(
                LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, size, size, 0));
        }
        const int image = icon ? ImageList_AddIcon(state.images, icon) : -1;
        if (icon)
        {
            DestroyIcon(icon);
        }
        auto name = Wide(EntryDisplayName(entry));
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_IMAGE;
        item.iItem = static_cast<int>(i);
        item.pszText = name.data();
        item.iImage = image;
        ListView_InsertItem(list, &item);
        UpdateRow(dialog, static_cast<int>(i));
    }
    if (state.selected >= 0 && state.selected < static_cast<int>(state.settings.entries.size()))
    {
        ListView_SetItemState(list, state.selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list, state.selected, FALSE);
    }
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
    state.loading = false;
    PopulateEditor(dialog);
}

std::wstring Browse(HWND owner, int kind)
{
    ComPtr<IFileOpenDialog> picker;
    Check(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(picker.Put())),
          "Open file picker");
    DWORD options = 0;
    Check(picker->GetOptions(&options), "Read file picker options");
    Check(
        picker->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT | FOS_FORCEFILESYSTEM),
        "Set file picker options");
    const COMDLG_FILTERSPEC solutions[] = {{L"Visual Studio solutions (*.sln; *.slnx)", L"*.sln;*.slnx"}};
    const COMDLG_FILTERSPEC icons[] = {{L"Windows icons (*.ico)", L"*.ico"}};
    const COMDLG_FILTERSPEC executables[] = {{L"Visual Studio executable (devenv.exe)", L"devenv.exe"},
                                             {L"Executables (*.exe)", L"*.exe"}};
    if (kind == 0)
    {
        Check(picker->SetFileTypes(1, solutions), "Filter solutions");
        picker->SetTitle(L"Choose a Visual Studio solution");
    }
    else if (kind == 1)
    {
        Check(picker->SetFileTypes(1, icons), "Filter icons");
        picker->SetTitle(L"Choose a custom colored icon");
    }
    else
    {
        Check(picker->SetFileTypes(2, executables), "Filter executables");
        picker->SetTitle(L"Choose Visual Studio's devenv.exe");
    }
    const auto result = picker->Show(owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        return {};
    }
    Check(result, "Choose file");
    ComPtr<IShellItem> item;
    Check(picker->GetResult(item.Put()), "Get selected file");
    PWSTR raw = nullptr;
    Check(item->GetDisplayName(SIGDN_FILESYSPATH, &raw), "Get selected path");
    const std::wstring path(raw);
    CoTaskMemFree(raw);
    return path;
}

bool NonBlank(const std::string &value)
{
    return value.find_first_not_of(" \t\r\n") != std::string::npos;
}

void Validate(const Settings &settings)
{
    for (const auto &entry : settings.entries)
    {
        if (!NonBlank(entry.name))
        {
            throw std::runtime_error("Every solution needs a display name.");
        }
        if (!IsSolutionPath(entry.path) || !std::filesystem::path(Wide(entry.path)).is_absolute())
        {
            throw std::runtime_error("Choose an absolute .sln or .slnx path for: " + entry.name);
        }
        if (!entry.icon.empty())
        {
            const std::filesystem::path icon(Wide(entry.icon));
            if (!icon.is_absolute())
            {
                throw std::runtime_error("Use an absolute custom icon path for: " + entry.name);
            }
            HICON loaded = static_cast<HICON>(LoadImageW(nullptr, icon.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
            if (!loaded)
            {
                throw std::runtime_error("Cannot load custom .ico for: " + entry.name);
            }
            DestroyIcon(loaded);
        }
        for (const auto *text : {&entry.name, &entry.group, &entry.branch})
        {
            if (text->find_first_of("\r\n") != std::string::npos)
            {
                throw std::runtime_error("Names, groups and branch labels must be single lines.");
            }
        }
    }
    const auto checkStudio = [](const std::string &path)
    {
        if (path.empty())
        {
            return;
        }
        const std::filesystem::path exe(Wide(path));
        if (!exe.is_absolute() || _wcsicmp(exe.filename().c_str(), L"devenv.exe") != 0)
        {
            throw std::runtime_error("Select an absolute path to Visual Studio's devenv.exe.");
        }
    };
    checkStudio(settings.visualStudio);
    for (const auto &entry : settings.entries)
    {
        checkStudio(entry.visualStudio);
    }
}

bool Save(HWND dialog)
{
    auto &state = State(dialog);
    Validate(state.settings);
    // Prepare all icons before saving; publication errors still leave recoverable settings.
    for (auto &entry : state.settings.entries)
    {
        const auto icon = EntryIcon(entry);
        if (!entry.icon.empty())
        {
            entry.icon = Utf8(icon.wstring());
        }
    }
    SaveSettings(state.settings);
    state.dirty = false;
    RefreshList(dialog);
    try
    {
        Status(dialog, PublishJumpList(state.settings));
    }
    catch (const std::exception &error)
    {
        Status(dialog, L"Settings saved, but Windows could not update the menu. Click Save to retry.");
        Error(dialog, error);
        return false;
    }
    return true;
}

void Close(HWND dialog)
{
    if (State(dialog).dirty)
    {
        const auto choice = MessageBoxW(dialog, L"Save your changes before closing?", L"Visual Studio Launcher",
                                        MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL)
        {
            return;
        }
        if (choice == IDYES && !Save(dialog))
        {
            return;
        }
    }
    EndDialog(dialog, IDOK);
}

void MarkDirty(HWND dialog)
{
    State(dialog).dirty = true;
    Status(dialog, L"Unsaved changes. Click Save / update menu to publish the taskbar list.");
}

void Command(HWND dialog, int command)
{
    auto &state = State(dialog);
    switch (command)
    {
    case IDC_ADD:
    {
        if (state.settings.entries.size() >= 500)
        {
            throw std::runtime_error("Maximum 500 solutions");
        }
        const auto path = Browse(dialog, 0);
        if (path.empty())
        {
            return;
        }
        Entry entry;
        entry.id = NewId();
        entry.path = Utf8(path);
        entry.name = Utf8(std::filesystem::path(path).stem().wstring());
        if (auto *current = Selected(dialog))
        {
            entry.group = current->group;
        }
        state.settings.entries.push_back(std::move(entry));
        state.selected = static_cast<int>(state.settings.entries.size()) - 1;
        RefreshList(dialog);
        MarkDirty(dialog);
        SetFocus(GetDlgItem(dialog, IDC_NAME));
        SendDlgItemMessageW(dialog, IDC_NAME, EM_SETSEL, 0, -1);
        break;
    }
    case IDC_DUPLICATE:
        if (auto *current = Selected(dialog))
        {
            if (state.settings.entries.size() >= 500)
            {
                throw std::runtime_error("Maximum 500 solutions");
            }
            Entry copy = *current;
            copy.id = NewId();
            copy.name += " - copy";
            state.settings.entries.insert(state.settings.entries.begin() + state.selected + 1, std::move(copy));
            ++state.selected;
            RefreshList(dialog);
            MarkDirty(dialog);
        }
        break;
    case IDC_REMOVE:
        if (Selected(dialog) && MessageBoxW(dialog, L"Remove the selected launcher entry?", L"Visual Studio Launcher",
                                            MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            state.settings.entries.erase(state.settings.entries.begin() + state.selected);
            state.selected = (std::min)(state.selected, static_cast<int>(state.settings.entries.size()) - 1);
            RefreshList(dialog);
            MarkDirty(dialog);
        }
        break;
    case IDC_UP:
    case IDC_DOWN:
    {
        const int next = state.selected + (command == IDC_UP ? -1 : 1);
        if (Selected(dialog) && next >= 0 && next < static_cast<int>(state.settings.entries.size()))
        {
            std::swap(state.settings.entries[state.selected], state.settings.entries[next]);
            state.selected = next;
            RefreshList(dialog);
            MarkDirty(dialog);
        }
        break;
    }
    case IDC_OPEN:
        if (const auto *entry = Selected(dialog))
        {
            OpenSolution(state.settings, *entry, dialog);
        }
        break;
    case IDC_COLOR:
        if (auto *entry = Selected(dialog))
        {
            static COLORREF custom[16]{};
            const unsigned rgb = static_cast<unsigned>(std::stoul(entry->color.substr(1), nullptr, 16));
            CHOOSECOLORW choose{};
            choose.lStructSize = sizeof(choose);
            choose.hwndOwner = dialog;
            choose.lpCustColors = custom;
            choose.rgbResult = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
            choose.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&choose))
            {
                char hex[8]{};
                std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", GetRValue(choose.rgbResult),
                              GetGValue(choose.rgbResult), GetBValue(choose.rgbResult));
                entry->color = hex;
                entry->icon.clear();
                RefreshList(dialog);
                MarkDirty(dialog);
            }
        }
        break;
    case IDC_USE_COLOR:
        if (auto *entry = Selected(dialog))
        {
            entry->icon.clear();
            RefreshList(dialog);
            MarkDirty(dialog);
        }
        break;
    case IDC_ENABLED:
        ReadEditor(dialog);
        break;
    case IDC_BROWSE_PATH:
    case IDC_BROWSE_ICON:
    case IDC_BROWSE_STUDIO:
    case IDC_BROWSE_DEFAULT:
    {
        const int kind = command == IDC_BROWSE_PATH ? 0 : (command == IDC_BROWSE_ICON ? 1 : 2);
        const auto path = Browse(dialog, kind);
        if (path.empty())
        {
            break;
        }
        const int control =
            command == IDC_BROWSE_PATH
                ? IDC_PATH
                : (command == IDC_BROWSE_ICON ? IDC_CUSTOM_ICON
                                              : (command == IDC_BROWSE_STUDIO ? IDC_STUDIO : IDC_DEFAULT_STUDIO));
        SetDlgItemTextW(dialog, control, path.c_str());
        if (command == IDC_BROWSE_ICON)
        {
            RefreshList(dialog);
        }
        break;
    }
    case IDC_SAVE:
        Save(dialog);
        break;
    case IDC_SHORTCUT:
    {
        const auto shortcut = CreateStartMenuShortcut();
        Status(dialog, L"Start shortcut created. Search Start for Visual Studio Launcher, right-click it and choose "
                       L"Pin to taskbar.");
        break;
    }
    case IDC_SETTINGS:
    {
        const auto folder = DataDirectory();
        const auto result =
            reinterpret_cast<INT_PTR>(ShellExecuteW(dialog, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
        {
            throw std::runtime_error("Cannot open settings folder");
        }
        break;
    }
    case IDCANCEL:
        Close(dialog);
        break;
    }
}

INT_PTR CALLBACK Dialog(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    try
    {
        if (message == WM_INITDIALOG)
        {
            auto &state = *reinterpret_cast<Manager *>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER, lParam);
            SetPropW(dialog, ManagerProperty, reinterpret_cast<HANDLE>(1));
            const auto instance = GetModuleHandleW(nullptr);
            SendMessageW(dialog, WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP))));
            SendMessageW(dialog, WM_SETICON, ICON_SMALL,
                         reinterpret_cast<LPARAM>(LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP))));
            const HWND list = GetDlgItem(dialog, IDC_LIST);
            SendMessageW(list, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), TRUE);
            ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
            const wchar_t *names[] = {L"Display name / branch", L"Group", L"Solution path", L"Menu"};
            const int widths[] = {260, 125, 540, 58};
            for (int i = 0; i < 4; ++i)
            {
                LVCOLUMNW column{};
                column.mask = LVCF_TEXT | LVCF_WIDTH;
                column.pszText = const_cast<LPWSTR>(names[i]);
                column.cx = MulDiv(widths[i], GetDpiForWindow(dialog), 96);
                ListView_InsertColumn(list, i, &column);
            }
            for (const auto id :
                 {IDC_NAME, IDC_GROUP, IDC_BRANCH, IDC_PATH, IDC_CUSTOM_ICON, IDC_STUDIO, IDC_DEFAULT_STUDIO})
            {
                SendDlgItemMessageW(dialog, id, EM_SETLIMITTEXT, 32760, 0);
            }
            SendDlgItemMessageW(dialog, IDC_BRANCH_SOURCE, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(L"Edit yourself"));
            SendDlgItemMessageW(dialog, IDC_BRANCH_SOURCE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"From Git"));
            state.loading = true;
            Text(dialog, IDC_DEFAULT_STUDIO, state.settings.visualStudio);
            state.loading = false;
            if (!state.settings.entries.empty())
            {
                state.selected = 0;
            }
            RefreshList(dialog);
            try
            {
                Status(dialog, PublishJumpList(state.settings));
            }
            catch (const std::exception &error)
            {
                Status(dialog, L"Menu update failed: " + Wide(error.what()));
            }
            return TRUE;
        }
        if (!GetWindowLongPtrW(dialog, DWLP_USER))
        {
            return FALSE;
        }
        switch (message)
        {
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE)
            {
                ReadEditor(dialog);
            }
            else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_BRANCH_SOURCE)
            {
                if (auto *entry = Selected(dialog))
                {
                    entry->branchFromGit =
                        SendDlgItemMessageW(dialog, IDC_BRANCH_SOURCE, CB_GETCURSEL, 0, 0) == 1;
                    ShowBranch(dialog, entry);
                    UpdateRow(dialog, State(dialog).selected);
                    MarkDirty(dialog);
                }
            }
            else if (HIWORD(wParam) == BN_CLICKED)
            {
                Command(dialog, LOWORD(wParam));
            }
            return TRUE;
        case WM_NOTIFY:
        {
            const auto *header = reinterpret_cast<NMHDR *>(lParam);
            if (header->idFrom != IDC_LIST || State(dialog).loading)
            {
                return FALSE;
            }
            if (header->code == LVN_ITEMCHANGED)
            {
                const auto *item = reinterpret_cast<NMLISTVIEW *>(lParam);
                if ((item->uChanged & LVIF_STATE) && (item->uNewState & LVIS_SELECTED) &&
                    !(item->uOldState & LVIS_SELECTED))
                {
                    State(dialog).selected = item->iItem;
                    PopulateEditor(dialog);
                }
            }
            else if (header->code == NM_DBLCLK)
            {
                Command(dialog, IDC_OPEN);
            }
            return FALSE;
        }
        case WM_DPICHANGED:
            // Per-monitor-v2 automatically scales the dialog and its child controls.
            RefreshList(dialog);
            return FALSE;
        case WM_CLOSE:
            Close(dialog);
            return TRUE;
        case WM_DESTROY:
            RemovePropW(dialog, ManagerProperty);
            if (State(dialog).images)
            {
                ListView_SetImageList(GetDlgItem(dialog, IDC_LIST), nullptr, LVSIL_SMALL);
                ImageList_Destroy(State(dialog).images);
                State(dialog).images = nullptr;
            }
            return TRUE;
        }
    }
    catch (const std::exception &error)
    {
        Error(dialog, error);
        if (message == WM_INITDIALOG)
        {
            EndDialog(dialog, -1);
        }
        return TRUE; // Consume failed commands; default WM_CLOSE must not discard edits.
    }
    return FALSE;
}

BOOL CALLBACK FindManager(HWND window, LPARAM lParam)
{
    if (GetPropW(window, ManagerProperty))
    {
        *reinterpret_cast<HWND *>(lParam) = window;
        return FALSE;
    }
    return TRUE;
}

class Handle
{
  public:
    explicit Handle(HANDLE value) : value_(value)
    {
    }
    ~Handle()
    {
        if (value_)
        {
            CloseHandle(value_);
        }
    }
    HANDLE Get() const
    {
        return value_;
    }

  private:
    HANDLE value_;
};

class ComSession
{
  public:
    ComSession()
    {
        Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "Initialize Windows Shell");
    }
    ~ComSession()
    {
        CoUninitialize();
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    try
    {
        ComSession com;
        Check(SetCurrentProcessExplicitAppUserModelID(AppId), "Identify taskbar application");
        int argc = 0;
        auto *raw = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!raw)
        {
            throw std::runtime_error("Cannot parse command line");
        }
        std::vector<std::wstring> argv;
        for (int i = 0; i < argc; ++i)
        {
            argv.emplace_back(raw[i]);
        }
        LocalFree(raw);
        if (argc == 3 && argv[1] == L"--open")
        {
            const auto settings = LoadSettings();
            const auto id = Utf8(argv[2]);
            const auto it = std::find_if(settings.entries.begin(), settings.entries.end(),
                                         [&](const Entry &entry) { return entry.id == id; });
            if (it == settings.entries.end())
            {
                throw std::runtime_error("This entry no longer exists. Open the manager and save to refresh the menu.");
            }
            OpenSolution(settings, *it, nullptr);
            try
            {
                PublishJumpList(settings); // Pick up branches switched since the last update.
            }
            catch (...)
            {
            }
            return 0;
        }
        if (argc == 3 && argv[1] == L"--open-path")
        {
            const auto settings = LoadSettings();
            Entry recent;
            recent.path = Utf8(argv[2]);
            OpenSolution(settings, recent, nullptr);
            try
            {
                PublishJumpList(settings); // Move the opened solution to the top of the recent list.
            }
            catch (...)
            {
            }
            return 0;
        }
        if (argc == 2 && argv[1] == L"--refresh")
        {
            PublishJumpList(LoadSettings());
            return 0;
        }
        if (argc == 2 && argv[1] == L"--install")
        {
            CreateStartMenuShortcut();
            PublishJumpList(LoadSettings());
            return 0;
        }
        if (argc > 1 && !(argc == 2 && argv[1] == L"--manage"))
        {
            throw std::runtime_error("Usage: VisualStudioLauncher.exe [--manage | --open ID | --open-path FILE | --refresh | --install]");
        }
        const HANDLE rawMutex = CreateMutexW(nullptr, FALSE, ManagerMutex);
        const DWORD mutexError = GetLastError();
        Handle mutex(rawMutex);
        if (!mutex.Get())
        {
            throw std::runtime_error("Cannot create manager mutex");
        }
        if (mutexError == ERROR_ALREADY_EXISTS)
        {
            HWND existing = nullptr;
            // The first process may still be constructing its dialog.
            for (int attempt = 0; attempt < 20 && !existing; ++attempt)
            {
                EnumWindows(FindManager, reinterpret_cast<LPARAM>(&existing));
                if (!existing)
                {
                    Sleep(50);
                }
            }
            if (existing)
            {
                ShowWindow(existing, SW_RESTORE);
                SetForegroundWindow(existing);
            }
            return 0;
        }
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
        if (!InitCommonControlsEx(&controls))
        {
            throw std::runtime_error("Cannot initialize Windows controls");
        }
        Manager manager;
        manager.settings = LoadSettings(); // Never replace malformed settings with an empty list.
        CreateStartMenuShortcut();
        const auto result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_MANAGER), nullptr, Dialog,
                                            reinterpret_cast<LPARAM>(&manager));
        if (result == -1)
        {
            throw std::runtime_error("Cannot open manager dialog");
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        Error(nullptr, error);
        return 1;
    }
}
