@rem _Common.bat - shared setup for the Tools\*.bat launchers. Do not run it directly.
@rem
@rem Called from another Tools\*.bat as:   call "%~dp0_Common.bat" %*
@rem It has no setlocal on purpose: the variables below are set in the caller's
@rem (already setlocal'ed) environment.
@rem
@rem   CHECK, NOPAUSE, RES720, ZIPONLY        option flags, 0 or 1
@rem   PROJECT_ROOT, UPROJECT                 project folder and .uproject
@rem   UE_ROOT                                engine folder (env override, default below)
@rem   EDITOR_EXE, EDITOR_CMD_EXE, BUILD_BAT, UAT_BAT
@rem   MODULE_STATE                           built / missing  (editor C++ module DLL)
@rem   CONTENT_STATE                          ok / missing / lfs-pointer  (LV_Portfolio.umap)
@rem
@rem Returns errorlevel 1 (after printing why) for an unknown option, a missing
@rem .uproject or a missing engine. Everything else is only reported.

@rem Capture this folder before SHIFT below changes argument 0.
set "TOOLS_DIR=%~dp0"
set "CHECK=0"
set "NOPAUSE=0"
set "RES720=0"
set "ZIPONLY=0"

:common_parse
if "%~1"=="" goto common_parsed
if /i "%~1"=="--check" (
    set "CHECK=1"
    goto common_next
)
if /i "%~1"=="--no-pause" (
    set "NOPAUSE=1"
    goto common_next
)
if /i "%~1"=="--720p" (
    set "RES720=1"
    goto common_next
)
if /i "%~1"=="--zip-only" (
    set "ZIPONLY=1"
    goto common_next
)
echo [ERROR] Unknown option: %1
echo         Options: --check (print only, start nothing), --no-pause,
echo                  --720p (RunViewer/RunPlayDemo), --zip-only (PackageViewer)
exit /b 1
:common_next
shift
goto common_parse
:common_parsed

for %%I in ("%TOOLS_DIR%..") do set "PROJECT_ROOT=%%~fI"
set "UPROJECT=%PROJECT_ROOT%\CharacterShowcase.uproject"

if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
set "UE_ROOT=%UE_ROOT:"=%"
if "%UE_ROOT:~-1%"=="\" set "UE_ROOT=%UE_ROOT:~0,-1%"
set "EDITOR_EXE=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
set "EDITOR_CMD_EXE=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "BUILD_BAT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "UAT_BAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

echo Project : %UPROJECT%
echo Engine  : %UE_ROOT%
if "%CHECK%"=="1" echo Mode    : --check (prints the command only, starts nothing)

if not exist "%UPROJECT%" goto common_no_project
if not exist "%EDITOR_EXE%" goto common_no_engine

set "MODULE_STATE=built"
if not exist "%PROJECT_ROOT%\Binaries\Win64\UnrealEditor-CharacterShowcase.dll" set "MODULE_STATE=missing"
echo C++     : editor module %MODULE_STATE%

set "CONTENT_STATE=ok"
set "PORTFOLIO_MAP=%PROJECT_ROOT%\Content\Portfolio\Maps\LV_Portfolio.umap"
if not exist "%PORTFOLIO_MAP%" goto common_map_missing
for %%F in ("%PORTFOLIO_MAP%") do set "MAP_SIZE=%%~zF"
if %MAP_SIZE% LSS 1024 set "CONTENT_STATE=lfs-pointer"
goto common_content_done
:common_map_missing
set "CONTENT_STATE=missing"
:common_content_done
echo Content : %CONTENT_STATE%
if "%CONTENT_STATE%"=="lfs-pointer" echo [WARN] Content files are Git LFS pointers (a few hundred bytes), not real assets. Run:  git lfs install  then  git lfs pull
if "%CONTENT_STATE%"=="missing" echo [WARN] Content\Portfolio\Maps\LV_Portfolio.umap is missing. Is this a complete clone of the repository?
echo.
exit /b 0

:common_no_project
echo [ERROR] Project file not found: %UPROJECT%
echo         Keep this Tools folder inside the project folder (next to CharacterShowcase.uproject).
exit /b 1

:common_no_engine
echo [ERROR] Unreal Engine 5.6 not found: %EDITOR_EXE%
echo         Install UE 5.6.1 with the Epic Games Launcher, or point UE_ROOT at your install, e.g.
echo             set UE_ROOT=D:\Epic Games\UE_5.6
echo         and run this file again from the same command window.
exit /b 1
