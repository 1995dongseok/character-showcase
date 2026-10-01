@echo off
rem OpenEditor.bat - open CharacterShowcase in the Unreal Editor (UE 5.6.1).
rem Double-click it, or run from a command window:
rem   OpenEditor.bat [--check] [--no-pause]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

if "%CONTENT_STATE%" NEQ "ok" goto content_bad
if "%MODULE_STATE%"=="missing" echo [NOTE] The C++ module is not built yet. The Editor will ask "Would you like to rebuild them now?" - answer Yes (needs Visual Studio 2022). To see compiler output instead, run Tools\BuildEditor.bat first.

set CMDLINE="%EDITOR_EXE%" "%UPROJECT%"
if "%CHECK%"=="1" goto check

echo Starting the Unreal Editor. The first start (C++ build + shader compile) can take a long time.
echo The level LV_Portfolio opens automatically. You can close this window.
start "" %CMDLINE%
goto end

:check
echo [check] would run: start "" %CMDLINE%
goto end

:content_bad
echo [ERROR] Project content is not usable (%CONTENT_STATE%). See Docs\ARTIST_QUICKSTART.md step 7 (Git LFS).
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
