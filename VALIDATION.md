# Validation

Package version: 1.0.0

## Checks completed in this session

- All C++ translation units compiled for Windows x64 with MinGW GCC 13 in C++17 mode, using `-Wall -Wextra -Wpedantic -Werror`.
- Windows resource script and embedded manifest compiled; executable linked with a static GCC/C++ runtime.
- PE inspection identified a 64-bit Windows GUI application and only Windows DLL imports: COMCTL32, comdlg32, KERNEL32, msvcrt, ole32, SHELL32, SHLWAPI, USER32.
- Portable CMake Release build and CTest: **1/1 passed**.
- Windows x64 CMake Release cross-build: **passed**, including native resource and manifest compilation.
- Portable core checks also passed with AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer was disabled because this container cannot inspect process tasks.
- Settings round-trip preserved Unicode display names, quotes, spaces, UNC paths, ordering, visibility, executable paths and stable IDs.
- Settings parser rejected duplicate IDs, command-like IDs, invalid icon colors, unsupported versions, invalid Boolean values, invalid surrogate escapes, duplicate JSON keys, truncation, trailing commas and trailing garbage.
- Windows argument quoting checks covered empty strings, spaces, embedded quotes and trailing backslashes.
- The actual C++ icon generator produced blue, green and purple icons. Pillow decoded all five sizes (16, 24, 32, 48, 64), with transparent corners and opaque centers.
- Visual Studio project and application manifest XML parsed successfully.
- All MSBuild source, header, resource and auxiliary input paths resolve within the packaged project.
- Source formatting checked with clang-format 18 using the included Allman brace style.

## Checks that need your Windows PC

This session has no access to your Windows desktop or `D:` drive. The executable is cross-compiled; the MSVC project is included but has not been built using MSVC here.

1. Run `Install.cmd`, pin the created Start shortcut, and confirm left-click opens the manager.
2. Add two solutions with the same filename, different paths, different names and colors. Save and confirm their right-click entries are distinguishable.
3. Open both entries and confirm Visual Studio opens the correct solution. Set an explicit `devenv.exe` if needed.
4. Rename, recolor, hide, duplicate and reorder entries, saving between changes; check the taskbar menu updates.
5. Close the manager, then open a solution from the Jump List while the launcher is not running.
6. Check save/discard behavior and reopen to confirm persistence.
7. Check the dialog on your monitor's DPI scale and check custom category availability under your Windows privacy policies.

Windows controls category availability, pinned destination order and the number of visible Jump List entries. A cross-build cannot verify Explorer's live behavior.
