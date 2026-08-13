# Codex Login and System Tray Design

## Goal

Replace the taskbar-covering counter with a standard Windows notification-area icon, and let a user start the official Codex ChatGPT login flow when their CLI session is unavailable.

## Decisions

- Use Windows Forms `NotifyIcon`; do not place a topmost window over the taskbar clock or date.
- Respect Windows' **Other system tray icons** setting. The app registers a normal notification icon; Windows and the user control whether it is shown directly or in the overflow area.
- Render a compact, color-coded `W` icon. Show the exact weekly percentage in the tray tooltip and popup because Windows notification-area icons do not support persistent arbitrary text.
- Own the message loop with `ApplicationContext`, not a visible main form. The tray context menu provides status, refresh, login, and exit actions.
- Read `account/read` after App Server initialization. Treat only a `chatgpt` account as able to provide this app's ChatGPT weekly quota.
- When no ChatGPT account is available, send `account/login/start` with `type: "chatgpt"`, open only the returned HTTPS authorization URL in the user's default browser, and wait for `account/login/completed` before refreshing usage.
- The app never receives, persists, prints, or transmits credentials. Codex App Server owns OAuth token storage and refresh.

## UI states

| State | Tray tooltip | Popup |
|---|---|---|
| Loading | `W -- · Codex 사용량 확인 중` | Usage lookup state |
| Login required | `W -- · Codex 로그인 필요` | Explanation and **로그인** button |
| Weekly quota ready | `W N% · 7일 잔여` | Remaining, used, reset, countdown, refresh |
| Read failure | Last available tray state | Error and refresh; login is offered when no ChatGPT session exists |

## Data flow

1. `TrayApplicationContext` starts `CodexUsageClient`.
2. The client initializes App Server, then `account/read` determines ChatGPT authentication.
3. An authenticated session reads `account/rateLimits/read`, consumes pushed updates, and falls back to a five-second refresh.
4. A login action invokes `account/login/start`; App Server returns the authorization URL, which opens in the default browser.
5. `account/login/completed` triggers a fresh account check and usage refresh.

## Constraints

- Windows 10/11, .NET 10 Windows Forms, no third-party packages.
- No account email, API key, access token, or actual quota values in source, logs, or repository files.
- A public release must be rebuilt after this change; do not alter the already-published v1.0.0 asset.

## Verification

- Unit checks parse unauthenticated and ChatGPT-authenticated `account/read` replies.
- Existing weekly-window and countdown checks remain green.
- Manual Windows QA verifies a notification-area icon, popup, login-required state, and no overlap with the clock/date.
