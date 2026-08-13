# Native Win32 Codex WeekUsage Tray Design

## Goal

Replace the .NET WinForms tray process with one x64 C++17 Win32 EXE while preserving every shipped user-facing behavior: weekly remaining number in the system tray, neon Codex panel, five-second fallback refresh, official browser login, and safe tray-settings cleanup.

## Investigated constraints

- Windows 10/11 x64 is the only target.
- The installed UCRT64 MinGW `g++` 16.1.0 toolchain includes the Windows, Shell, common-controls, and GDI+ headers and link libraries required for a native tray app.
- `codex app-server --stdio` uses newline-delimited JSON-RPC on standard input/output. It requires an `initialize` request followed by `initialized`, provides `account/rateLimits/read` and `account/rateLimits/updated`, and owns ChatGPT browser OAuth, token persistence, callback handling, and login completion notifications.
- The current app's steady-state private memory is about 60 MiB plus about 24 MiB for the required Codex App Server child. The native host must retain the child server to preserve five-second refresh and pushed limit changes.

## Architecture

The native EXE owns one Win32 message loop, one `Shell_NotifyIcon` icon, and one borderless popup window. It starts only `%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe` with `app-server --stdio`, using inherited anonymous pipes for JSONL requests, replies, and notifications. A reader thread parses incoming messages and posts UI work to the message loop; the UI thread owns every window, GDI+ object, tray change, and popup interaction.

The app uses no network client, browser control, .NET runtime, or credential store. For ChatGPT login it asks App Server for `account/login/start` with `type: "chatgpt"`, accepts only absolute HTTPS `chatgpt.com` or `auth.openai.com` URLs, and calls `ShellExecuteW` to open the returned URL. The App Server, not this EXE, owns credentials and the localhost OAuth callback.

## Source layout

- `native/build.cmd` — reproducible UCRT64 MinGW x64 release and native self-test builds using `-std=c++17 -Os -s -static`.
- `native/src/main.cpp` — application entry, message loop, tray icon, popup window, rendering, menu, actions, and `--self-test` dispatch.
- `native/src/codex_client.h` / `native/src/codex_client.cpp` — fixed Codex path resolution, child process/pipes, JSON-RPC lifecycle, request correlation, login state, refresh timer, and event delivery.
- `native/third_party/jsmn.h` — the small MIT-licensed jsmn JSON tokenizer, vendored as source with no runtime or package-manager dependency.
- `native/src/json.h` / `native/src/json.cpp` — the exact JSON field readers/writers and string escaping built on the tokenizer.
- `native/tests/native_tests.cpp` — native self-tests for JSON escaping, secure login hosts, weekly-window selection, remaining percentage, and fixed Codex path selection.
- GDI+, Shell32, Ole32, and Comctl32 remain Windows system libraries.

## UI and behavior parity

- Standard Windows notification-area icon, controlled by Windows **Other system tray icons** settings. It renders `--` before data and the full remaining percentage when present, using the existing lavender-to-blue Codex colors.
- Left click toggles the 368-by-282 DPI-aware neon `CODEX / WEEKLY LIMIT` popup. The popup retains the current English copy, used/reset/time-left rows, `Sign in`/`Check` or `Refresh`, and right-most `Close` button.
- Right-click menu retains **Show panel**, **Refresh**, **Sign in to Codex**, and **Quit**.
- App Server notifications update immediately. A five-second timer refetches as a fallback; one request is in flight at a time.
- A missing or non-ChatGPT account shows `--` and offers login. Restarting sign-in cancels the prior `loginId`. Completion is accepted only when its `loginId` matches the pending attempt.
- The popup's one-second countdown starts only while it is visible.

## Security and privacy

- Never execute `codex.exe` from the current directory or `PATH`.
- Reject login URLs that are not absolute HTTPS or whose exact host is not `chatgpt.com` or `auth.openai.com`.
- Do not request API-key login, read refresh tokens, write account data, log JSON payloads, or register startup persistence.
- `--uninstall` and `--uninstall-dry-run` remove only current-user `NotifyIconSettings` entries whose `ExecutablePath` base name is exactly `CodexWeekUsageTray.exe`; matching running processes are stopped only when their full executable path is one of the selected entries. EXE files are never deleted.
- The release remains unsigned until a trusted Authenticode certificate is supplied; publish a SHA-256 manifest with every release.

## Build and release

`native/build.cmd` produces one `CodexWeekUsageTray.exe` with no PDB or .NET dependency. It must pass `--self-test`, the native test executable, native Windows UI QA, packaged-EXE self-test, ESET scan, and checksum verification before replacing the current GitHub release.

The native EXE replaces the existing C# release only after parity QA. Then remove `src/`, `tests/CodexWeekUsageTray.Tests/`, and the PowerShell/CMD uninstaller as one cleanup commit; Git history preserves the prior implementation.

## Acceptance criteria

1. A Windows user can run the release EXE without .NET installed.
2. The app shows the number or `--` as a normal notification-area icon and never overlays the taskbar clock.
3. Browser login opens the official Codex URL and, after successful login, refreshes the weekly rate-limit display.
4. The panel, menu, popup close behavior, weekly calculation, English copy, colors, and five-second fallback match the current release.
5. The native host uses materially less private memory than the current roughly 60 MiB .NET host; report both native-host and Codex-child measurements separately.
6. The release contains no credentials, PDB, managed .NET runtime, or unexpected files.
