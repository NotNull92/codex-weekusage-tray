@echo off
setlocal
set "ROOT=%~dp0"
set "CXX=C:\Users\PC\msys64\ucrt64\bin\g++.exe"
set "OUT=%ROOT%out"

if not exist "%OUT%" mkdir "%OUT%"

if /I "%~1"=="tests" (
  "%CXX%" -std=c++17 -Wall -Wextra -Werror -municode -I"%ROOT%src" "%ROOT%tests\native_tests.cpp" "%ROOT%src\core.cpp" "%ROOT%src\codex_client.cpp" -lwinhttp -o "%OUT%\CodexWeekUsageTrayTests.exe"
  exit /b %errorlevel%
)

"%CXX%" -std=c++17 -Os -s -static -municode -mwindows -I"%ROOT%src" "%ROOT%src\main.cpp" "%ROOT%src\core.cpp" "%ROOT%src\codex_client.cpp" -lgdiplus -lshell32 -lcomctl32 -lole32 -lwinhttp -o "%OUT%\CodexWeekUsageTray.exe"
