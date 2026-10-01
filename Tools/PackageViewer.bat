@echo off
rem PackageViewer.bat - build a standalone Windows copy of the viewer and zip it for sending.
rem   1) RunUAT BuildCookRun, Win64 Development (same flags as Docs\CHARACTER_VIEWER_SETUP.md 1.5)
rem      -> Saved\Packaged\Windows\CharacterShowcase.exe
rem   2) copy it without debug files (*.pdb) and without a stale Shipping exe, then zip:
rem      -> Saved\Packaged\CharacterShowcase-Win64-<yyyyMMdd>.zip
rem Only LV_Portfolio is cooked (Config\DefaultGame.ini MapsToCook); the play demo is not included.
rem   PackageViewer.bat [--check] [--no-pause] [--zip-only]
rem     --check     print what would run and exit; starts nothing
rem     --no-pause  do not wait for a key at the end
rem     --zip-only  skip step 1 and zip the existing Saved\Packaged\Windows
rem Engine location: C:\Program Files\Epic Games\UE_5.6, or set UE_ROOT to override.
setlocal EnableExtensions
set "RC=0"
call "%~dp0_Common.bat" %*
if errorlevel 1 goto failed

if not exist "%UAT_BAT%" goto no_uat
if "%CONTENT_STATE%" NEQ "ok" goto content_bad

set "PKG_ROOT=%PROJECT_ROOT%\Saved\Packaged"
set "PKG_DIR=%PKG_ROOT%\Windows"
set "STAMP="
for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"`) do set "STAMP=%%D"
if not defined STAMP set "STAMP=undated"
set "ZIP_NAME=CharacterShowcase-Win64-%STAMP%"
set "STAGE_DIR=%PKG_ROOT%\_zipstage\%ZIP_NAME%"
set "ZIP_PATH=%PKG_ROOT%\%ZIP_NAME%.zip"

set CMDLINE="%UAT_BAT%" BuildCookRun -project="%UPROJECT%" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%PKG_ROOT%" -unattended -noP4 -utf8output
set COPY_CMD=robocopy "%PKG_DIR%" "%STAGE_DIR%" /E /NFL /NDL /NJH /NP /XF *.pdb CharacterShowcase-Win64-Shipping.*
set ZIP_CMD=powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -LiteralPath '%STAGE_DIR%' -DestinationPath '%ZIP_PATH%' -Force"

if "%CHECK%"=="1" goto check

if "%ZIPONLY%"=="1" goto zip
echo Packaging (build + cook + pak). This takes several minutes or more; keep this window open.
call %CMDLINE%
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto package_failed

:zip
if not exist "%PKG_DIR%\CharacterShowcase.exe" goto no_package
echo Copying the package without debug files...
if exist "%PKG_ROOT%\_zipstage" rmdir /s /q "%PKG_ROOT%\_zipstage"
%COPY_CMD%
if errorlevel 8 goto copy_failed
echo Zipping to %ZIP_PATH% ...
%ZIP_CMD%
if errorlevel 1 goto zip_failed
rmdir /s /q "%PKG_ROOT%\_zipstage"
for %%Z in ("%ZIP_PATH%") do echo [OK] %%~fZ  (%%~zZ bytes)
echo Send this zip. The receiver unzips it and double-clicks %ZIP_NAME%\CharacterShowcase.exe
echo (Windows 10/11 64-bit; DirectX 12 capable GPU recommended).
set "RC=0"
goto end

:check
echo [check] step 1 package: call %CMDLINE%
echo [check] step 2 copy : %COPY_CMD%
echo [check] step 3 zip  : %ZIP_CMD%
echo [check] result      : %ZIP_PATH%
if "%ZIPONLY%"=="1" echo [check] --zip-only: step 1 would be skipped.
goto end

:package_failed
echo [ERROR] Packaging failed with exit code %RC%. Search the output above for "ERROR".
goto end
:no_package
echo [ERROR] No package found at %PKG_DIR%\CharacterShowcase.exe. Run without --zip-only first.
goto failed
:copy_failed
echo [ERROR] Copying the package to %STAGE_DIR% failed.
goto failed
:zip_failed
echo [ERROR] Compress-Archive failed. Is the zip open in another program? Is the disk full?
goto failed
:no_uat
echo [ERROR] RunUAT.bat not found: %UAT_BAT%
goto failed
:content_bad
echo [ERROR] Project content is not usable (%CONTENT_STATE%). See Docs\ARTIST_QUICKSTART.md step 7 (Git LFS).
:failed
set "RC=1"
:end
if "%CHECK%"=="0" if "%NOPAUSE%"=="0" pause
endlocal & exit /b %RC%
