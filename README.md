# Codex WeekUsage Tray

A small Windows system-tray app that shows your Codex weekly limit. Click the `--` or remaining-number tray icon to see what is left, when it resets, and the time left.

The app is a standard Windows tray icon. It does not cover the taskbar clock. Choose whether to show it in **Settings > Personalization > Taskbar > Other system tray icons**. The icon shows only a large bold number or `--`, using the lavender-to-blue colors from the Codex mark. Hover it to read a clear label such as `73% left this week`.

## What you need

- Windows 10 or Windows 11
- The [Codex CLI](https://developers.openai.com/codex/cli/) installed
- .NET 10 SDK when running from source

If Codex is not signed in with a ChatGPT account, choose **Sign in** in the tray panel or **Sign in to Codex** from the right-click menu. The app opens the HTTPS page provided by Codex in your default browser. It never asks for or saves your password, API key, or token. A Codex CLI session using only an API key needs a ChatGPT sign-in to read this limit.

## Run

```powershell
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj
```

Find the `--` icon in the Windows system tray. If it is hidden, turn on **Codex WeekUsage Tray** in **Settings > Personalization > Taskbar > Other system tray icons**. When the weekly limit is not available, the panel still offers **Sign in** and **Check**.

## How it works

- Updates right away after an `account/rateLimits/updated` notice.
- Checks at most once every five seconds as a fallback, not every minute.
- Shows `--` before it can read the limit, then a large remaining number such as `73`.
- Opens a neon `CODEX / WEEKLY LIMIT` panel with `Used`, `Resets`, and `Time left` when you click the tray icon.
- Uses **Refresh** to check now and **Close** to hide only the panel. The app keeps running in the tray.
- Starts the Codex browser sign-in with **Sign in**. Choosing it again cancels the old attempt and opens a new page.
- The right-click menu offers **Show panel**, **Refresh**, **Sign in to Codex**, and **Quit**.

## Release build

Create a folder that other Windows x64 users can run without installing .NET:

```powershell
dotnet publish src/CodexWeekUsageTray/CodexWeekUsageTray.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -o artifacts/win-x64
```

Run `artifacts/win-x64/CodexWeekUsageTray.exe`. Embedded assemblies are compressed to keep the standalone EXE smaller; the first launch can take slightly longer. Each user still needs the Codex CLI. If the user is not signed in, the app can open the browser sign-in page.

## Security

- The app runs Codex only from its standard per-user OpenAI install and never resolves `codex.exe` from the current folder or `PATH`.
- It opens sign-in only on HTTPS `chatgpt.com` or `auth.openai.com` URLs returned by Codex App Server.
- It does not store a password, API key, token, or usage history. Trust only a Codex CLI installed from OpenAI.
- Release binaries are not Authenticode-signed yet. Verify the published SHA-256 checksum before running a downloaded EXE.

## Remove old tray entries

Run `uninstall.cmd` from the release folder to remove saved Codex WeekUsage Tray entries from **Other system tray icons** right away. Use `uninstall.cmd -DryRun` to only view the entries.

It stops matching Codex WeekUsage Tray processes and removes only their saved Windows tray settings. It does not delete EXE files or change settings for other apps.

## Privacy

This repository contains no account names, API keys, access tokens, or real usage data. The app only displays the current session limit returned by Codex App Server and does not save it to a file or a remote server.

## Checks

```powershell
dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj
dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -- --self-test
```
