@echo off
rem SubRoo Rooms - Windows installer (right click -> Run as administrator)
setlocal
set HERE=%~dp0
echo Installing SubRoo Rooms...
if not exist "%CommonProgramFiles%\VST3" mkdir "%CommonProgramFiles%\VST3"
if not exist "%CommonProgramFiles%\CLAP" mkdir "%CommonProgramFiles%\CLAP"
xcopy /E /I /Y "%HERE%SubRoo Rooms.vst3" "%CommonProgramFiles%\VST3\SubRoo Rooms.vst3" >nul
copy /Y "%HERE%SubRoo Rooms.clap" "%CommonProgramFiles%\CLAP\SubRoo Rooms.clap" >nul
if errorlevel 1 (
  echo.
  echo Install failed. Right click install_windows.bat and choose "Run as administrator".
  pause
  exit /b 1
)
if not exist "%USERPROFILE%\Documents\SubRoo Rooms\Packs" mkdir "%USERPROFILE%\Documents\SubRoo Rooms\Packs"
echo.
echo Done. VST3:  %CommonProgramFiles%\VST3\SubRoo Rooms.vst3
echo       CLAP:  %CommonProgramFiles%\CLAP\SubRoo Rooms.clap
echo       Packs: %USERPROFILE%\Documents\SubRoo Rooms\Packs
echo Rescan plug-ins in your DAW.
pause
