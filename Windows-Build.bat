@echo off
REM Windows counterpart to Unix-Build.sh.
REM
REM GENERATOR: Ninja. Ninja and Makefiles are the only generators that emit
REM compile_commands.json, which clangd needs for go-to-definition and
REM diagnostics in both VS Code and Emacs. The Visual Studio generator does
REM NOT produce one, so switching to it silently breaks both editors.
REM
REM COMPILER: pinned, because auto-detection is wrong on this machine. A bare
REM CMake configure picks C:\Program Files\LLVM\bin\clang++.exe (18.1.8),
REM which is both older than the Clang 20 that VS 18's MSVC 14.51 STL requires
REM   error STL1000: Unexpected compiler version, expected Clang 20 or newer
REM and a GNU-style driver rather than the MSVC-style clang-cl this project's
REM if(MSVC) flags assume. VS 18 ships clang 22, which pairs correctly.
REM
REM Keep this in sync with "cmake.configureSettings" in .vscode/settings.json,
REM or VS Code and this script will produce incompatible caches in build/.
REM
REM Windows-only by design: Unix-Build.sh pins nothing, so Linux keeps
REM auto-detecting its system compiler.
REM
REM Usage:  Windows-Build.bat [Debug|Release]     (default Debug)

setlocal

set build_target=Debug
if not "%~1"=="" set build_target=%~1

set "CLANG_CL=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"
if not exist "%CLANG_CL%" (
    echo ERROR: clang-cl not found at:
    echo   %CLANG_CL%
    echo Edit CLANG_CL in this script to match your Visual Studio install.
    exit /b 1
)

REM The Ninja generator needs mt.exe (the manifest tool) from the Windows SDK.
REM VS Code's CMake kit puts it on PATH; a plain shell does not, so CMake fails
REM with CMAKE_MT-NOTFOUND. Find the newest SDK rather than hardcoding a
REM version -- `for /d` enumerates in order, so the last match wins.
set "MT_EXE="
for /d %%D in ("C:\Program Files (x86)\Windows Kits\10\bin\10.*") do (
    if exist "%%D\x64\mt.exe" set "MT_EXE=%%D\x64\mt.exe"
)
if not defined MT_EXE (
    echo ERROR: mt.exe not found under the Windows 10 SDK.
    echo Install the Windows SDK via the Visual Studio Installer.
    exit /b 1
)

cmake -S . -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=%build_target% ^
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
      -DCMAKE_C_COMPILER="%CLANG_CL%" ^
      -DCMAKE_CXX_COMPILER="%CLANG_CL%" ^
      -DCMAKE_MT="%MT_EXE%"
if errorlevel 1 exit /b 1

REM Ninja is single-config, so the build type came from CMAKE_BUILD_TYPE above
REM and --config would be meaningless here.
cmake --build build --parallel
exit /b %errorlevel%
