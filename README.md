<p align="center">
  <img src="docs/logo/logo.png" width="160" alt="Codex Weekly Limit Tray logo">
</p>

<p align="center">
  <b>English</b> | <a href="README.ko.md">한국어</a> | <a href="README.ja.md">日本語</a> | <a href="README.zh-CN.md">简体中文</a> | <a href="README.ru.md">Русский</a>
</p>

# Codex Weekly Limit Tray

A small native Windows app that shows your Codex weekly limit in the system tray.

The tray icon shows a large bold number such as `73`, or `--` while the limit is not ready. Click it to open the Codex panel. The panel shows what is left, when it resets, and the time left.

<p align="center">
  <img src="docs/screenshot/tray.png" width="220" alt="Tray icon showing the number 39 in the Windows system tray"><br>
  <sub><em>Tray icon</em></sub>
  <br><br>
  <img src="docs/screenshot/panel.png" width="320" alt="Detail panel showing 39% left, usage, reset date, and time left"><br>
  <sub><em>Detail panel</em></sub>
</p>

## What you need

- Windows 10 or Windows 11 on x64
- The official [Codex CLI](https://developers.openai.com/codex/cli/)

To build from source, install the MSYS2 UCRT64 C++ compiler. The build script checks `%USERPROFILE%\msys64\ucrt64\bin\g++.exe`, then `C:\msys64\ucrt64\bin\g++.exe`, or uses a full path supplied through the `CXX` environment variable.

The app resolves Codex through Windows' per-user Local AppData known folder. Its normal location is:

```text
%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe
```

## Run the app

For a public release, extract the ZIP and run `CodexWeekUsageTray.exe`.

To build and run from source:

```powershell
native\build.cmd
native\out\CodexWeekUsageTray.exe
```

The icon belongs to the normal Windows system tray. It never covers the clock or date.

### Make the tray icon visible

1. Run `CodexWeekUsageTray.exe`.
2. Open **Settings > Personalization > Taskbar > Other system tray icons**.
3. Find **CodexWeekUsageTray** and turn it **On**.

Windows then shows the icon in the system tray instead of leaving it hidden in the overflow list.

## Use the panel

- **Left-click the tray icon** to open or hide the detail panel.
- **Right-click the tray icon**, then choose **Show panel** to open the detail panel from the tray menu.
- **Refresh** checks your limit now.
- **Close** hides only the panel. The tray app keeps running.
- **Sign in** opens the official Codex ChatGPT sign-in page in your default browser when needed.
- **Check** checks the account again after sign-in.

The app updates right away when Codex sends a limit update. It also checks at most once every five seconds as a fallback.

## Build checks

```powershell
native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --self-test
```

## Remove saved tray entries

When testing has left old copies in **Other system tray icons**, double-click `uninstall.cmd` in the same folder as `CodexWeekUsageTray.exe`. It starts cleanup immediately; there is no word to type or confirmation prompt.

`uninstall.cmd` stops matching `CodexWeekUsageTray.exe` copies and removes only their current-user tray settings. It is useful before replacing an older release or when duplicate tray entries remain. It never deletes EXE files or changes another app's tray setting.

From a terminal in the release folder, you can also run:

```powershell
.\CodexWeekUsageTray.exe --uninstall-dry-run
.\CodexWeekUsageTray.exe --uninstall
```

The dry run only lists exact `CodexWeekUsageTray.exe` entries and changes nothing.

## Security and privacy

- The app does not read, print, save, or upload your password, API key, refresh token, or usage history.
- Login URLs must use HTTPS with the exact host `chatgpt.com` or `auth.openai.com` before the default browser opens them.
- The native host has no updater, downloader, startup registration, or app-owned network client. Codex CLI handles its own account traffic over a local stdio connection.
- Release EXEs are not Authenticode-signed yet. Check the published SHA-256 value before running a downloaded file.

v2.0.1 and later releases include the EXE, `uninstall.cmd`, and a matching `SHA256SUMS` manifest. The archive contains no .NET runtime, DLL, or PDB. v2.0.0 included only the EXE.

The app stores the current limit only in memory. Windows may keep normal notification-icon settings, and Codex CLI keeps its own session data.
