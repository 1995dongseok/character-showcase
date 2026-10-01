@echo off
rem RunViewer.bat - run the portfolio viewer (LV_Portfolio) in its own window,
rem without opening the Editor UI (UnrealEditor.exe -game, uncooked content).
rem   RunViewer.bat [--check] [--no-pause] [--720p]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem     --720p      1280x720 window instead of 1920x1080 (slow GPUs, small screens)
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

if "%CONTENT_STATE%" NEQ "ok" goto content_bad
if "%MODULE_STATE%"=="missing" goto module_missing

set "RES_X=1920"
set "RES_Y=1080"
if "%RES720%"=="1" set "RES_X=1280"
if "%RES720%"=="1" set "RES_Y=720"
set CMDLINE="%EDITOR_EXE%" "%UPROJECT%" /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=%RES_X% -ResY=%RES_Y%
if "%CHECK%"=="1" goto check

echo Starting the viewer (%RES_X%x%RES_Y% window). The first run may show "Preparing Shaders" for a while.
echo Keys: drag = orbit, wheel = zoom, R reset, Space turntable, H clean view, I inspection, W wireframe,
echo       F12 screenshot, Shift+F12 36-frame turntable, Esc cancel capture, Alt+F4 quit.
echo Screenshots go to: %PROJECT_ROOT%\Saved\Screenshots\Portfolio
start "" %CMDLINE%
goto end

:check
echo [check] would run: start "" %CMDLINE%
goto end

:module_missing
echo [ERROR] The C++ module is not built yet (Binaries\Win64\UnrealEditor-CharacterShowcase.dll).
echo         Run Tools\BuildEditor.bat once, or open Tools\OpenEditor.bat and answer Yes to rebuild.
goto failed
:content_bad
echo [ERROR] Project content is not usable (%CONTENT_STATE%). See Docs\ARTIST_QUICKSTART.md step 7 (Git LFS).
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
