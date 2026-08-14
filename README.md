# Codex Weekly Limit Tray

A small native Windows app that shows your Codex weekly limit in the system tray.

The tray icon shows a large bold number such as `73`, or `--` while the limit is not ready. Click it to open the Codex panel. The panel shows what is left, when it resets, and the time left.

## What you need

- Windows 10 or Windows 11 on x64
- The official [Codex CLI](https://developers.openai.com/codex/cli/)

The app starts Codex only from its normal per-user install location:

```text
%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe
```

## Run from source

```powershell
native\build.cmd
native\out\CodexWeekUsageTray.exe
```

The icon belongs to the normal Windows system tray. If it is hidden, enable **Codex WeekUsage Tray** in **Settings > Personalization > Taskbar > Other system tray icons**. It never covers the clock or date.

## Use the panel

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

When testing has left old copies in **Other system tray icons**, run:

```powershell
native\out\CodexWeekUsageTray.exe --uninstall-dry-run
native\out\CodexWeekUsageTray.exe --uninstall
```

The dry run only lists exact `CodexWeekUsageTray.exe` entries. The uninstall command immediately stops matching older app copies and removes only their current-user tray settings. It never deletes EXE files or changes another app's tray setting.

## Security and privacy

- The app does not read, print, save, or upload your password, API key, refresh token, or usage history.
- Login URLs must use HTTPS with the exact host `chatgpt.com` or `auth.openai.com` before the default browser opens them.
- The native host has no updater, downloader, startup registration, or app-owned network client. Codex CLI handles its own account traffic over a local stdio connection.
- Release EXEs are not Authenticode-signed yet. Check the published SHA-256 value before running a downloaded file.

The app stores the current limit only in memory. Windows may keep normal notification-icon settings, and Codex CLI keeps its own session data.
