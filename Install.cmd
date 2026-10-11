@echo off
setlocal
set "VSLAUNCHER_EXE=%~dp0bin\Release\VisualStudioLauncher.exe"
if not exist "%VSLAUNCHER_EXE%" set "VSLAUNCHER_EXE=%~dp0bin\VisualStudioLauncher.exe"
if not exist "%VSLAUNCHER_EXE%" (
  call "%~dp0Build.cmd"
  if errorlevel 1 exit /b 1
  set "VSLAUNCHER_EXE=%~dp0bin\Release\VisualStudioLauncher.exe"
)
"%VSLAUNCHER_EXE%" --install
if errorlevel 1 exit /b 1
start "" "%VSLAUNCHER_EXE%" --manage
echo Open Start, search for Visual Studio Launcher, right-click it and select Pin to taskbar.
echo You can place that icon beside your existing Visual Studio icon.
