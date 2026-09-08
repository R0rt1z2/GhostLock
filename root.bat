@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0root.ps1" %*
set "GLRC=%ERRORLEVEL%"
echo.
pause
exit /b %GLRC%
