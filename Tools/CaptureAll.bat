@echo off
rem CaptureAll.bat - shoot the whole portfolio image set in one go, unattended.
rem Opens the viewer (UnrealEditor.exe -game, uncooked content), runs the console command
rem   Viewer.CaptureAll profile=<current|all> [expressions=1] [poses=1] quit=1
rem (every camera preset x material variant as a UI-less screenshot, like F12), waits until
rem the window closes by itself, then prints the new Batch_* folder(s) and their file counts.
rem   CaptureAll.bat [--check] [--no-pause] [--720p] [--all] [--expressions] [--poses]
rem     --all          every profile in the GameMode's Profile Library (default: only the start profile)
rem     --expressions  also one set per Expression (only for profiles with more than Neutral)
rem     --poses        plus one shot per pose entry (Animations with Is Pose)
rem     --720p         1280x720 window instead of 1920x1080 (shots are window size x 2)
rem     --check        print what would run and exit; starts nothing
rem     --no-pause     do not wait for a key at the end
rem Output: Saved\Screenshots\Portfolio\Batch_<Profile>_<yyyyMMdd-HHmmss>\<Profile>_<Preset>_<Variant>.png
rem Do not touch the window while it runs; Esc in the window cancels the batch.
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
rem Capture this folder before SHIFT below changes argument 0.
set "SELF_DIR=%~dp0"
set "PROFILE_ARG=current"
set "EXPRESSIONS_ARG=0"
set "POSES_ARG=0"
set "COMMON_ARGS="

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--all" (
    set "PROFILE_ARG=all"
    goto next
)
if /i "%~1"=="--expressions" (
    set "EXPRESSIONS_ARG=1"
    goto next
)
if /i "%~1"=="--poses" (
    set "POSES_ARG=1"
    goto next
)
set COMMON_ARGS=%COMMON_ARGS% %1
:next
shift
goto parse
:parsed

call "%SELF_DIR%_Common.bat" %COMMON_ARGS%
if errorlevel 1 goto usage

if "%CONTENT_STATE%" NEQ "ok" goto content_bad
if "%MODULE_STATE%"=="missing" goto module_missing

set "RES_X=1920"
set "RES_Y=1080"
if "%RES720%"=="1" set "RES_X=1280"
if "%RES720%"=="1" set "RES_Y=720"
set "PORTFOLIO_DIR=%PROJECT_ROOT%\Saved\Screenshots\Portfolio"
set "LOG_FILE=%PROJECT_ROOT%\Saved\Logs\CaptureAll.log"
set "CAPTURE_CMD=Viewer.CaptureAll profile=%PROFILE_ARG% expressions=%EXPRESSIONS_ARG% poses=%POSES_ARG% quit=1"
set CMDLINE="%EDITOR_EXE%" "%UPROJECT%" /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=%RES_X% -ResY=%RES_Y% -WinX=0 -WinY=0 "-ExecCmds=%CAPTURE_CMD%" -log "-abslog=%LOG_FILE%"
if "%CHECK%"=="1" goto check

rem Remember the Batch_* folders that already exist, so only the new ones are reported.
set "BEFORE_LIST=%TEMP%\CaptureAll_before_%RANDOM%%RANDOM%.txt"
type nul > "%BEFORE_LIST%"
if exist "%PORTFOLIO_DIR%" dir /b /ad "%PORTFOLIO_DIR%\Batch_*" > "%BEFORE_LIST%" 2>nul

echo Capturing (%RES_X%x%RES_Y% window, profile=%PROFILE_ARG%, expressions=%EXPRESSIONS_ARG%, poses=%POSES_ARG%).
echo The viewer window opens, takes the shots and closes by itself. The first run may show
echo "Preparing Shaders" for a while; the batch waits for shaders before the first shot.
echo Do not click into the window; Esc there cancels the batch. Waiting for it to close...
start "" /wait %CMDLINE%
set "GAME_RC=%ERRORLEVEL%"

echo.
echo ===== Result =====
set "NEW_FOLDERS=0"
set "NEW_FILES=0"
for /f "delims=" %%D in ('dir /b /ad "%PORTFOLIO_DIR%\Batch_*" 2^>nul') do (
    findstr /x /l /c:"%%D" "%BEFORE_LIST%" >nul 2>nul || call :report_folder "%%D"
)
del "%BEFORE_LIST%" >nul 2>nul
echo New folders: %NEW_FOLDERS%, new files: %NEW_FILES% (viewer exit code %GAME_RC%)
if not exist "%LOG_FILE%" goto after_log
echo.
echo ----- [CharacterViewerBatch] lines from %LOG_FILE% -----
findstr /l /c:"[CharacterViewerBatch]" "%LOG_FILE%"
:after_log
if "%NEW_FILES%"=="0" goto no_files
goto end

:report_folder
set "COUNT=0"
for %%F in ("%PORTFOLIO_DIR%\%~1\*.png") do set /a COUNT+=1
echo   %PORTFOLIO_DIR%\%~1   (%COUNT% files)
set /a NEW_FOLDERS+=1
set /a NEW_FILES+=COUNT
exit /b 0

:check
echo [check] console : %CAPTURE_CMD%
echo [check] would run: start "" /wait %CMDLINE%
echo [check] then list: new "%PORTFOLIO_DIR%\Batch_*" folders and their *.png counts
echo [check] then show: findstr /l /c:"[CharacterViewerBatch]" "%LOG_FILE%"
goto end

:no_files
echo [ERROR] No new screenshots were written. Read the [CharacterViewerBatch] lines above and
echo         %LOG_FILE%  (search for "CaptureAll" / "CharacterViewerBatch").
goto failed

:usage
echo         CaptureAll.bat also takes: --all, --expressions, --poses (see the top of this file)
goto failed

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
