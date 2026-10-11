@echo off
setlocal
set "VSLAUNCHER_MSBUILD="
set "VSLAUNCHER_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSLAUNCHER_VSWHERE%" (
  echo Visual Studio Installer was not found. Open VisualStudioLauncher.sln and build Release x64.
  exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSLAUNCHER_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find MSBuild\**\Bin\MSBuild.exe`) do set "VSLAUNCHER_MSBUILD=%%i"
if not defined VSLAUNCHER_MSBUILD (
  echo Install the Desktop development with C++ workload in Visual Studio.
  exit /b 1
)
"%VSLAUNCHER_MSBUILD%" "%~dp0VisualStudioLauncher.sln" /m /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1
echo.
echo Built: %~dp0bin\Release\VisualStudioLauncher.exe
