@echo off
rem ValidateProfiles.bat - check every CharacterProfileData asset without opening the Editor UI.
rem Runs Scripts\ValidateProfiles.py headless (UnrealEditor-Cmd, -NullRHI). Expected summary lines:
rem   [ValidateProfiles] <asset>: E=0 W=.. I=..     (E = errors, W = warnings, I = info)
rem   ValidateProfiles.bat [--check] [--no-pause]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

set "SCRIPT=%PROJECT_ROOT%\Scripts\ValidateProfiles.py"
set "LOG_FILE=%PROJECT_ROOT%\Saved\Logs\ValidateProfiles.log"
set CMDLINE="%EDITOR_CMD_EXE%" "%UPROJECT%" "-ExecutePythonScript=%SCRIPT%" -NullRHI -unattended -nosplash -nop4 "-abslog=%LOG_FILE%"

if not exist "%SCRIPT%" goto no_script
if "%CONTENT_STATE%" NEQ "ok" goto content_bad
if "%MODULE_STATE%"=="missing" goto module_missing
if "%CHECK%"=="1" goto check

echo Validating profiles. This loads the project headless and takes a few minutes; no window opens.
call %CMDLINE%
set "RC=%ERRORLEVEL%"
echo.
echo ===== Summary (from %LOG_FILE%) =====
if exist "%LOG_FILE%" findstr /l /c:"[ValidateProfiles]" "%LOG_FILE%"
echo Exit code: %RC%
if not "%RC%"=="0" echo [ERROR] Validation did not finish cleanly. Read the lines above and the log file.
goto end

:check
echo [check] script   : %SCRIPT%
echo [check] would run: %CMDLINE%
echo [check] then show: findstr /l /c:"[ValidateProfiles]" "%LOG_FILE%"
goto end

:no_script
echo [ValidateProfiles] Scripts\ValidateProfiles.py is not present yet - nothing to validate.
echo                    It is added by a separate change; pull the latest main and run this file again.
if "%CHECK%"=="1" echo [check] would run once the script exists: %CMDLINE%
goto end

:module_missing
echo [ERROR] The C++ module is not built yet (Binaries\Win64\UnrealEditor-CharacterShowcase.dll).
echo         Run Tools\BuildEditor.bat once first.
goto failed
:content_bad
echo [ERROR] Project content is not usable (%CONTENT_STATE%). See Docs\ARTIST_QUICKSTART.md step 7 (Git LFS).
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
