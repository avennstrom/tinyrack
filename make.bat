@echo off
setlocal enabledelayedexpansion


set DEBUG=0
if "%1" == "debug" set DEBUG=1

set "VCVARS="
for %%e in (Enterprise Professional Community) do (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat"
        goto :found_vs
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\%%e\VC\Auxiliary\Build\vcvars64.bat"
        goto :found_vs
    )
    if exist "C:\Program Files\Microsoft Visual Studio\2019\%%e\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\%%e\VC\Auxiliary\Build\vcvars64.bat"
        goto :found_vs
    )
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%e\VC\Auxiliary\Build\vcvars64.bat" (
        set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\%%e\VC\Auxiliary\Build\vcvars64.bat"
        goto :found_vs
)
)

echo ERROR: Could not find Visual Studio installation
echo Please install Visual Studio 2019 or 2022 with C++ development tools
pause
exit /b 1

:found_vs
echo Found Visual Studio: %VCVARS%
call "%VCVARS%"

if not exist "obj" mkdir obj

:: Find all .c files in ./src
set "sources="
for /r src %%f in (*.c) do (
    set "sources=!sources! %%f"
)

if "!sources!"=="" (
    echo No sources
    exit /b 1
)

echo Building...

cl.exe /nologo /Zi /Od /W4 /WX /EHsc ^
    %sources% ^
    /Fe:"tinyrack.exe" ^
    /Fo:"obj\\" ^
    /MD ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /D_DEBUG ^
    /Iraylib/include ^
    user32.lib gdi32.lib kernel32.lib dsound.lib dxguid.lib Shell32.lib Winmm.lib ^
    raylib/raylib.lib

if %ERRORLEVEL% neq 0 (
    exit /b 1
)

echo Done.

:: Run game
tinyrack