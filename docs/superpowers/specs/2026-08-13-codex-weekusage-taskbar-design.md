# Codex 7-day usage taskbar counter

## Goal

Provide a Windows 10/11 desktop app that keeps the signed-in Codex account's seven-day quota remaining percentage visible at the right end of the taskbar. Clicking the percentage opens a compact popup showing the reset time and time remaining.

## Chosen approach

Windows does not provide a supported way for an ordinary app to insert arbitrary text into the Windows 11 taskbar. The app will instead use a small borderless, click-enabled overlay that is positioned over the taskbar's right end. It remains visually part of the taskbar and has no normal taskbar button.

The app will be a small .NET 8 Windows Forms executable. It will use only the .NET runtime and Windows APIs already available on the machine; no third-party package or API key is required.

## Data flow

1. Start `codex app-server` with its standard-input/output JSON-RPC transport, which reuses the local Codex ChatGPT login.
2. Request `account/rateLimits/read` at startup and locate the quota bucket whose `windowDurationMins` is 10,080 (seven days).
3. Calculate remaining quota as `100 - usedPercent` and convert the returned Unix `resetsAt` time to local time.
4. Apply `account/rateLimits/updated` notifications immediately whenever Codex reports an account change.
5. Poll the same read endpoint every five seconds only as a missed-notification safeguard. The displayed value therefore updates on server push when available and otherwise within five seconds.

The app never reads, copies, or stores credentials. If Codex is not installed, signed out, or does not return a seven-day quota bucket, the counter displays `7D --`; its popup explains the reason and offers a retry.

## Interaction and presentation

- The compact counter displays `7D 73%` on the primary taskbar. Green means 50% or more remains, amber means 20–49%, and red means under 20%.
- Clicking it opens a popup directly above the counter with remaining percentage, used percentage, the exact local reset date/time, and a countdown such as `3일 4시간 남음`.
- The popup refreshes its countdown every second without making additional network requests. Clicking outside it closes it; clicking the counter again also closes it.
- A manual refresh action in the popup fetches the newest quota value immediately.
- The overlay tracks taskbar position and size changes while the app is running. Closing the app shuts down its Codex process cleanly.

## Boundaries

- The initial release targets the primary Windows taskbar only. It does not register a legacy DeskBand, modify Explorer, install a service, or start automatically at login.
- The app shows only the seven-day quota. The shorter primary quota window and token activity history are out of scope.

## Verification

- Build the release executable with the installed .NET SDK.
- Exercise the JSON-RPC parser with a representative seven-day response and a missing-weekly-window response.
- Launch the executable on Windows, confirm the counter appears on the taskbar, click it, and confirm the popup shows a reset countdown. Confirm error-state text if the signed-in account has no weekly bucket.

## Spec review

Checked for placeholders, conflicting update intervals, and ambiguous quota selection. The seven-day quota is explicitly identified by its 10,080-minute window; server push is the primary update path and five-second polling is the fallback.
