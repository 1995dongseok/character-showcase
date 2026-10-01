@echo off
rem RunPlayDemo.bat - run the play demo (LV_PlayDemo, third-person walk/run) in its own window.
rem   RunPlayDemo.bat [--check] [--no-pause] [--720p]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem     --720p      1280x720 window instead of 1920x1080
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
rem Note: the packaged build (PackageViewer.bat) does not include this map (MapsToCook = LV_Portfolio only).
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

set "DEMO_MAP=%PROJECT_ROOT%\Content\PlayDemo\Maps\LV_PlayDemo.umap"
if not exist "%DEMO_MAP%" goto demo_missing
for %%F in ("%DEMO_MAP%") do set "DEMO_SIZE=%%~zF"
if %DEMO_SIZE% LSS 1024 goto demo_pointer
if "%MODULE_STATE%"=="missing" goto module_missing

set "RES_X=1920"
set "RES_Y=1080"
if "%RES720%"=="1" set "RES_X=1280"
if "%RES720%"=="1" set "RES_Y=720"
set CMDLINE="%EDITOR_EXE%" "%UPROJECT%" /Game/PlayDemo/Maps/LV_PlayDemo -game -windowed -ResX=%RES_X% -ResY=%RES_Y%
if "%CHECK%"=="1" goto check

echo Starting the play demo (%RES_X%x%RES_Y% window).
echo Keys: W/A/S/D move, Left Shift run, mouse look, wheel zoom, R reset camera,
echo       Backspace back to start, Esc show/hide cursor, Alt+F4 quit.
start "" %CMDLINE%
goto end

:check
echo [check] map file : %DEMO_MAP% (%DEMO_SIZE% bytes)
echo [check] would run: start "" %CMDLINE%
goto end

:demo_missing
echo [ERROR] Play demo map not found: %DEMO_MAP%
goto failed
:demo_pointer
echo [ERROR] %DEMO_MAP% is a Git LFS pointer (%DEMO_SIZE% bytes). Run:  git lfs install  then  git lfs pull
goto failed
:module_missing
echo [ERROR] The C++ module is not built yet (Binaries\Win64\UnrealEditor-CharacterShowcase.dll).
echo         Run Tools\BuildEditor.bat once, or open Tools\OpenEditor.bat and answer Yes to rebuild.
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
