@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo           Compiling AutoTrainer with MSVC
echo ===================================================
echo.

REM Locate Visual Studio vcvars64.bat
if defined VCINSTALLDIR (
    goto :COMPILE
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo [ERROR] Visual Studio 2022 x64 build environment not found.
    pause
    exit /b 1
)

:COMPILE
echo.
echo [1/3] Compiling Windows Resources (version.rc)...
rc /fo version.res version.rc >nul

echo [2/3] Compiling AutoTrainer.exe (C++20, Direct3D 11, WinRT Media OCR, ImGui)...
cl /nologo /O2 /Oi /Ot /MP /utf-8 /permissive- /await:strict /std:c++20 /EHsc ^
    /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DNDEBUG ^
    /Isrc /Iexternal\imgui /Iexternal\imgui\backends ^
    src\main.cpp ^
    src\App.cpp ^
    src\ProcessManager.cpp ^
    src\ScreenCapture.cpp ^
    src\OcrEngine.cpp ^
    src\MemoryScanner.cpp ^
    src\TrainerController.cpp ^
    src\RegionSelectorOverlay.cpp ^
    src\Utils.cpp ^
    external\imgui\imgui.cpp ^
    external\imgui\imgui_demo.cpp ^
    external\imgui\imgui_draw.cpp ^
    external\imgui\imgui_tables.cpp ^
    external\imgui\imgui_widgets.cpp ^
    external\imgui\backends\imgui_impl_win32.cpp ^
    external\imgui\backends\imgui_impl_dx11.cpp ^
    version.res ^
    /FeAutoTrainer.exe ^
    /link /SUBSYSTEM:WINDOWS ^
    d3d11.lib d3dcompiler.lib dxgi.lib dwmapi.lib gdi32.lib gdiplus.lib user32.lib advapi32.lib windowsapp.lib

if errorlevel 1 (
    echo.
    echo [ERROR] AutoTrainer compilation failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Compiling MockGame.exe (Test RPG Game)...
cl /nologo /O2 /utf-8 /std:c++20 /EHsc /DUNICODE /D_UNICODE ^
    src\MockGame.cpp ^
    /FeMockGame.exe ^
    /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib

if errorlevel 1 (
    echo.
    echo [ERROR] MockGame compilation failed!
    pause
    exit /b 1
)

REM Cleanup intermediate .obj and .res files
del /q *.obj version.res 2>nul

echo.
echo ===================================================
echo           Build Completed Successfully!
echo ===================================================
echo Output:
echo   - AutoTrainer.exe
echo   - MockGame.exe
echo.
