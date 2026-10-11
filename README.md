# Visual Studio Launcher

A small native C++17 Windows application for a **manually maintained**, color-coded Visual Studio solution Jump List.

- **Right-click the pinned launcher icon:** choose a named solution with a colored icon.
- **Left-click the icon:** open the manager dialog, or activate it if it is already open.
- Choosing a solution starts Visual Studio through `devenv.exe` or the Windows solution file association. The launcher exits after handing off the launch.
- No repository scans, branch switching, automatic recent-file imports, tray service, or background monitoring.

## Install in your requested folder

After extracting the ZIP, you can run the top-level `InstallToProjects.ps1` from PowerShell to copy the project into `D:\_Projects\VisualStudioLauncher` and start installation. It refuses to overwrite a nonempty destination. Alternatively, use the manual steps below.

1. Extract the contents of the `VisualStudioLauncher` folder in the ZIP into:

   ```text
   D:\_Projects\VisualStudioLauncher
   ```

   `Install.cmd` and `VisualStudioLauncher.sln` should be directly inside that folder; avoid an extra nested `VisualStudioLauncher` directory.

2. Run **Install.cmd**. It uses the included executable if present. Otherwise it calls `Build.cmd`, which requires Visual Studio's **Desktop development with C++** workload and the **MSVC v143** tools.
3. In Windows Start, search for **Visual Studio Launcher**. Right-click the result, choose **Pin to taskbar**, and drag it beside your existing Visual Studio icon.
4. In the manager, use **Add...** to choose a `.sln` or `.slnx`. Set a useful display name, group, optional branch label, and icon color.
5. Click **Save / update menu**. Right-click the launcher taskbar icon to use the list. You can close the manager; the Jump List remains available.

The program creates only its own Start-menu shortcut and per-user settings. It does not need administrator rights.

## Manage your list

| Control | Purpose |
| --- | --- |
| Add... | Choose a solution file and create an entry. |
| Duplicate | Copy an entry with a new stable ID; change its name/path for another branch or worktree. |
| Remove | Remove the entry from the configuration. No solution or repository files are deleted. |
| Move up / Move down | Set the manual order. Category order follows the first entry in each group; entries keep their relative order within each category. |
| Display name | Your label, independent of the solution filename. |
| Group | A native Jump List category, such as CTrack, Robotics, or Utilities. |
| Branch label | A manually entered suffix, displayed as `CTrack [develop]`. It does not query or change Git. |
| Icon color | Choose any RGB color. Generates an antialiased colored icon with a white code symbol. Choosing a color clears the custom icon override. |
| Custom .ico | Use an existing icon; the launcher saves a managed copy when you save. |
| Use color | Clear the custom icon and use the colored built-in symbol again. |
| Show in Jump List | Hide an entry from the taskbar menu while keeping it in the manager. |
| VS override | Optional full path to `devenv.exe` for this entry. |
| Default Visual Studio | Optional `devenv.exe` used for entries without an override. |
| Open solution | Test or launch the selected entry without closing the manager. Double-click a row also opens it. |
| Create Start shortcut | Recreate the shortcut after relocating the executable. |
| Settings folder | Open the folder containing configuration, backup, and managed icons. |

All field edits are kept in memory until you save. Closing with unsaved changes offers Save, Discard, or Cancel.

### Choose your Visual Studio version

For predictable launches, use **Browse...** beside **Default Visual Studio** and select the `devenv.exe` of your preferred installation. Common locations include:

```text
C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe
C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe
```

Other versions or editions may have different installation paths. Select their actual executable. If both the entry override and default are blank, Windows opens the solution using its current file association, usually Visual Studio or Visual Studio Version Selector. Choose an explicit executable if that association points elsewhere.

## Windows Jump List behavior

- The menu uses the native Windows style. Color is supplied through icons; Windows controls text color and layout.
- Windows decides how many destinations fit. The launcher respects the limit returned by Windows and reports truncation in the manager. All entries are still available through the manager's Open button.
- Items you remove through Windows' **Remove from this list** remain hidden during subsequent menu updates. Duplicate the entry in the manager to create a new identity if you want to restore it.
- Some Windows privacy or application-registration policies block custom categories. For the known access/registration failures, the launcher puts the colored commands under **Tasks**, prefixed with the group name, and reports this in the status line. Other failures display an error; the settings remain saved so you can retry.
- The fallback Tasks section has Windows' own menu-size restrictions. The manager always retains the complete configured list.
- Windows may show a Pinned section if you pin individual destinations. Its order is controlled by Windows.
- Right-click behavior is provided by the Windows taskbar; this application does not intercept taskbar mouse clicks.
- If you move the executable, unpin the old shortcut, start it from its new location, save the list, and pin the new Start shortcut.

## Settings and recovery

Settings and icons are stored at:

```text
%LOCALAPPDATA%\CTech\VisualStudioLauncher\
  settings.json
  settings.previous.json
  icons\
```

Each save writes and flushes a temporary file, then replaces the configuration. The previous version is backed up as `settings.previous.json`. A malformed or unsupported configuration produces an error and is **not** silently overwritten with an empty list. To recover, close the launcher, copy `settings.previous.json` over `settings.json`, then reopen it.

Missing solution files are allowed in saved configurations so an unavailable network share does not prevent editing the list. Launching checks the solution and selected executable and displays a useful error if either is unavailable. Names, paths, and icon files support Unicode.

Settings example:

```json
{
  "version": 1,
  "visualStudio": "",
  "entries": [
    {
      "id": "ctrack-main",
      "name": "CTrack - Main",
      "group": "CTrack",
      "branch": "main",
      "path": "D:\\_Projects\\CTrackMain\\CTrack_Work.sln",
      "color": "#16A34A",
      "icon": "",
      "visualStudio": "",
      "enabled": true
    }
  ]
}
```

The shipped configuration starts empty. Example paths are not added to your list.

## Build and modify

### Visual Studio

Open `VisualStudioLauncher.sln`, select **Release | x64**, then **Build Solution**. Output:

```text
D:\_Projects\VisualStudioLauncher\bin\Release\VisualStudioLauncher.exe
```

`Build.cmd` locates MSBuild through the Visual Studio Installer's `vswhere.exe`. The project targets **v143** and any installed Windows 10/11 SDK. If you use a newer Visual Studio without v143, install the v143 toolset or retarget the project through its properties. The MSVC build uses the static C++ runtime; MFC, .NET, vcpkg, and third-party libraries are not required.

### CMake (optional)

```powershell
cmake -S . -B build/cmake -A x64
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

On Linux, the CMake project builds only the portable settings/argument tests. A Windows cross-toolchain can also build the executable.

### Source map

| File | Responsibility |
| --- | --- |
| `src/Main.cpp` | Native dialog, editing, file/color pickers, single manager instance, command-line modes. |
| `src/Platform.cpp` | Unicode conversion, durable configuration writes, Shell launch, Jump List, Start shortcut. |
| `src/Icons.cpp` | Multiresolution 32-bit `.ico` generation with alpha and AND masks. |
| `src/Core.cpp` | Settings JSON, validation helpers, display names, Windows argument quoting. |
| `res/Launcher.rc` | Dialog layout, app icon and executable metadata. |
| `res/Launcher.manifest` | Common controls, DPI awareness and normal user privileges. |
| `tests/CoreTests.cpp` | Persistence, Unicode, malformed settings and argument-handling checks. |

Command-line modes:

```text
VisualStudioLauncher.exe                  Open manager
VisualStudioLauncher.exe --manage         Open or activate manager
VisualStudioLauncher.exe --open ENTRY_ID  Launch a configured solution
VisualStudioLauncher.exe --refresh        Rebuild the Jump List from saved settings
VisualStudioLauncher.exe --install        Create Start shortcut and rebuild the Jump List
```

## Validation and limits

See `VALIDATION.md` for the checks run on this package. Native Windows taskbar behavior must be verified on your PC; a cross-build cannot exercise Explorer's live Jump List UI.

To remove the launcher, unpin its taskbar icon, delete its Start shortcut, and delete the extracted project folder. Delete the settings directory separately only if you also want to erase your configured list.

## Windows API references

- [ICustomDestinationList](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-icustomdestinationlist)
- [AppendCategory and privacy/registration behavior](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icustomdestinationlist-appendcategory)
- [Application User Model IDs and shortcuts](https://learn.microsoft.com/en-us/windows/win32/shell/appids)
