@echo off
if not exist "%~dp0CodexWeekUsageTray.exe" (
  echo CodexWeekUsageTray.exe was not found next to this script.
  exit /b 1
)
"%~dp0CodexWeekUsageTray.exe" --uninstall
exit /b %errorlevel%
