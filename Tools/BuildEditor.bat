@echo off
rem BuildEditor.bat - compile the project's C++ for the Editor (CharacterShowcaseEditor Win64 Development).
rem Needed once after cloning, and after pulling C++ changes. Close the Unreal Editor first
rem (Live Coding in a running Editor blocks this build).
rem   BuildEditor.bat [--check] [--no-pause]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

if not exist "%BUILD_BAT%" goto no_build_bat
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_STATE=found"
if not exist "%VSWHERE%" set "VS_STATE=not found (install Visual Studio 2022 with the Game development with C++ workload)"
echo Visual Studio installer: %VS_STATE%

set CMDLINE="%BUILD_BAT%" CharacterShowcaseEditor Win64 Development -Project="%UPROJECT%" -WaitMutex
if "%CHECK%"=="1" goto check

echo Building CharacterShowcaseEditor (Win64 Development)...
call %CMDLINE%
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" echo [OK] Build succeeded. Next: Tools\OpenEditor.bat or Tools\RunViewer.bat
if not "%RC%"=="0" echo [ERROR] Build failed with exit code %RC%. Look for "error" lines above.
goto end

:check
echo [check] would run: call %CMDLINE%
goto end

:no_build_bat
echo [ERROR] Build.bat not found: %BUILD_BAT%
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
