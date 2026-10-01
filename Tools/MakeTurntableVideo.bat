@echo off
rem MakeTurntableVideo.bat - turn a Shift+F12 turntable folder (frame_000.png ...) into
rem turntable.mp4 (H.264, yuv420p, CRF 18) and, with --gif, a 640 px wide turntable.gif.
rem   MakeTurntableVideo.bat [folder] [--gif] [--fps 12] [--check] [--no-pause]
rem     folder      a Turntable_* folder; default = the newest one in Saved\Screenshots\Portfolio
rem     --gif       also make turntable.gif (two-pass palette, 640 px wide, loops forever)
rem     --fps N     frames per second (default 12 = one turn of 36 frames in 3 seconds)
rem     --check     print what would run and exit; makes nothing
rem     --no-pause  do not wait for a key at the end
rem Needs ffmpeg (not part of Unreal). Looked up in this order: PATH, the winget install folder
rem %LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*, then Tools\ffmpeg\ffmpeg.exe.
rem If none is found it prints the install command and exits with code 2.
rem Unreal Engine is not needed for this file (UE_ROOT is not used).
setlocal EnableExtensions
set "RC=0"
rem Capture this folder before SHIFT below changes argument 0.
set "SELF_DIR=%~dp0"
for %%I in ("%SELF_DIR%..") do set "PROJECT_ROOT=%%~fI"
set "PORTFOLIO_DIR=%PROJECT_ROOT%\Saved\Screenshots\Portfolio"
set "CHECK=0"
set "NOPAUSE=0"
set "GIF=0"
set "FPS=12"
set "FOLDER="

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--check" (
    set "CHECK=1"
    goto next
)
if /i "%~1"=="--no-pause" (
    set "NOPAUSE=1"
    goto next
)
if /i "%~1"=="--gif" (
    set "GIF=1"
    goto next
)
if /i "%~1"=="--fps" (
    set "FPS=%~2"
    shift
    goto next
)
set "ARG=%~1"
if "%ARG:~0,2%"=="--" goto bad_option
if defined FOLDER goto bad_option
for %%I in ("%~1") do set "FOLDER=%%~fI"
:next
shift
goto parse
:parsed

echo Project : %PROJECT_ROOT%
if "%CHECK%"=="1" echo Mode    : --check (prints the commands only, makes nothing)

echo %FPS%| findstr /r /x "[1-9][0-9]*" >nul
if errorlevel 1 goto bad_fps

if defined FOLDER goto have_folder
for /f "delims=" %%D in ('dir /b /ad /o-d "%PORTFOLIO_DIR%\Turntable_*" 2^>nul') do (
    set "FOLDER=%PORTFOLIO_DIR%\%%D"
    goto have_folder
)
goto no_turntable
:have_folder
echo Frames  : %FOLDER%
if not exist "%FOLDER%\frame_000.png" goto no_frames
set "FRAME_COUNT=0"
for %%F in ("%FOLDER%\frame_*.png") do set /a FRAME_COUNT+=1
echo           %FRAME_COUNT% frame(s) at %FPS% fps

rem --- find ffmpeg ---
set "FFMPEG="
for /f "delims=" %%F in ('where ffmpeg.exe 2^>nul') do (
    if not defined FFMPEG set "FFMPEG=%%F"
)
if defined FFMPEG goto have_ffmpeg
for /d %%P in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*") do (
    for /r "%%P" %%F in (ffmpeg.exe) do (
        if not defined FFMPEG if exist "%%F" set "FFMPEG=%%F"
    )
)
if defined FFMPEG goto have_ffmpeg
if exist "%PROJECT_ROOT%\Tools\ffmpeg\ffmpeg.exe" set "FFMPEG=%PROJECT_ROOT%\Tools\ffmpeg\ffmpeg.exe"
if defined FFMPEG goto have_ffmpeg
goto no_ffmpeg
:have_ffmpeg
echo ffmpeg  : %FFMPEG%
echo.

set "MP4=%FOLDER%\turntable.mp4"
set "GIF_FILE=%FOLDER%\turntable.gif"
set "PALETTE=%FOLDER%\turntable_palette.png"
set MP4_CMD="%FFMPEG%" -hide_banner -loglevel warning -y -framerate %FPS% -i "%FOLDER%\frame_%%03d.png" -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -crf 18 "%MP4%"
set PALETTE_CMD="%FFMPEG%" -hide_banner -loglevel warning -y -framerate %FPS% -i "%FOLDER%\frame_%%03d.png" -vf "scale=640:-1:flags=lanczos,palettegen" -update 1 "%PALETTE%"
set GIF_CMD="%FFMPEG%" -hide_banner -loglevel warning -y -framerate %FPS% -i "%FOLDER%\frame_%%03d.png" -i "%PALETTE%" -lavfi "scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse" -loop 0 "%GIF_FILE%"
if "%CHECK%"=="1" goto check

echo Making %MP4% ...
%MP4_CMD%
if errorlevel 1 goto ffmpeg_failed
call :report "%MP4%"
if "%GIF%"=="0" goto end

echo Making %GIF_FILE% (pass 1: palette, pass 2: GIF) ...
%PALETTE_CMD%
if errorlevel 1 goto ffmpeg_failed
%GIF_CMD%
if errorlevel 1 goto ffmpeg_failed
del "%PALETTE%" >nul 2>nul
call :report "%GIF_FILE%"
goto end

:report
for %%F in ("%~1") do echo Saved   : %%~fF  (%%~zF bytes)
exit /b 0

:check
echo [check] would run: %MP4_CMD%
if "%GIF%"=="0" goto end
echo [check] would run: %PALETTE_CMD%
echo [check] would run: %GIF_CMD%
echo [check] then delete: "%PALETTE%"
goto end

:no_ffmpeg
echo [ERROR] ffmpeg was not found (PATH, %LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*, Tools\ffmpeg\ffmpeg.exe).
echo         Install it for your user only (no administrator rights needed), then open a NEW command window:
echo             winget install --id Gyan.FFmpeg --scope user --accept-package-agreements --accept-source-agreements
echo         Or download a Windows build from https://www.gyan.dev/ffmpeg/builds/ and put ffmpeg.exe in Tools\ffmpeg\.
set "RC=2"
goto end

:no_turntable
echo [ERROR] No Turntable_* folder in %PORTFOLIO_DIR%.
echo         Take a turntable first: Tools\RunViewer.bat, then Shift+F12 in the viewer (36 frames).
echo         Or pass a folder:  MakeTurntableVideo.bat "D:\path\to\Turntable_DA_Hero_20261001-120000"
goto failed

:no_frames
echo [ERROR] %FOLDER%\frame_000.png not found. Pass a Turntable_* folder made by Shift+F12.
goto failed

:bad_fps
echo [ERROR] --fps needs a whole number greater than 0 (got "%FPS%").
goto failed

:bad_option
echo [ERROR] Unknown option or second folder: %1
echo         Options: [folder] --gif --fps N --check --no-pause
goto failed

:ffmpeg_failed
echo [ERROR] ffmpeg failed (exit code %ERRORLEVEL%). See the messages above.
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
