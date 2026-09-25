@echo off
setlocal enabledelayedexpansion

rem Reformats every engine, testbed and shader source with .clang-format.
rem Skips build\ so fetched dependencies are left alone.
rem Pass --check to fail without writing, for CI or a pre-commit hook.

set "CLANG_FORMAT=clang-format"
where %CLANG_FORMAT% >nul 2>nul
if errorlevel 1 (
    set "CLANG_FORMAT=C:\Program Files\LLVM\bin\clang-format.exe"
    if not exist "!CLANG_FORMAT!" (
        echo clang-format not found on PATH or in C:\Program Files\LLVM\bin
        exit /b 1
    )
)

set "MODE=-i"
if /i "%~1"=="--check" set "MODE=--dry-run -Werror"

set COUNT=0
set FAILED=0

for %%D in (Warp\Engine Warp\TestBed Warp\EntryPoint Warp\Shaders) do (
    if exist "%%D" (
        for /r "%%D" %%F in (*.h *.hpp *.cpp *.c *.hlsl) do (
            "!CLANG_FORMAT!" %MODE% "%%F"
            if errorlevel 1 set /a FAILED+=1
            set /a COUNT+=1
        )
    )
)

if %FAILED% GTR 0 (
    echo.
    echo %FAILED% of %COUNT% files are not formatted.
    exit /b 1
)

echo Formatted %COUNT% files.
endlocal
