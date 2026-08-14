@REM scripts/build-cli-windows.cmd
@echo off
set "configuration=%~1"
if "%configuration%"=="" set "configuration=Release"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Configuration "%configuration%" -Mode Cli
exit /b %ERRORLEVEL%
