@echo off
setlocal
powershell.exe -NoProfile -File "%~dp0uninstall.ps1" %*
exit /b %errorlevel%
