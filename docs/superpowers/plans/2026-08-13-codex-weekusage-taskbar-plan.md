# Codex 7-day Taskbar Counter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (\`- [ ]\`) syntax for tracking.

**Goal:** Build a Windows app that always shows the logged-in Codex account's seven-day remaining quota at the taskbar edge and reveals its reset countdown when clicked.

**Architecture:** A .NET 10 Windows Forms app runs the local \`codex app-server\` over JSONL stdio and maps its rate-limit response to a \`QuotaSnapshot\`. A borderless tool window overlays the primary taskbar; a child popup shows local reset time and a local countdown.

**Tech Stack:** .NET 10, C# 14, Windows Forms, System.Text.Json, Win32 P/Invoke, Codex App Server JSON-RPC. No NuGet dependencies.

## Global Constraints

- Target Windows 10/11 and the primary taskbar only.
- Use the local signed-in Codex CLI. Never request, read, persist, log, or embed credentials or personal information.
- Use a quota only if \`windowDurationMins == 10080\`.
- Apply \`account/rateLimits/updated\` server notifications immediately; call \`account/rateLimits/read\` every five seconds only as a missed-notification fallback.
- Use a taskbar-edge overlay without Explorer modification, DeskBand registration, installer, service, or startup registration.
- Do not commit real names, email addresses, local paths, API keys, access tokens, account IDs, or captured account responses.

---

## File Structure

- \`src/CodexWeekUsageTray/CodexWeekUsageTray.csproj\`: no-dependency Windows Forms executable.
- \`src/CodexWeekUsageTray/Program.cs\`: application and \`--self-test\` entrypoints.
- \`src/CodexWeekUsageTray/QuotaSnapshot.cs\`: value object, rate-limit parser, display formatter, self-check.
- \`src/CodexWeekUsageTray/CodexUsageClient.cs\`: child process, JSON-RPC correlation, notifications, fallback loop.
- \`src/CodexWeekUsageTray/TaskbarLocator.cs\`: primary taskbar Win32 bounds.
- \`src/CodexWeekUsageTray/TaskbarOverlayForm.cs\`: always-visible counter and taskbar tracking.
- \`src/CodexWeekUsageTray/UsagePopupForm.cs\`: click popup, local reset time, countdown, manual refresh.
- \`.gitignore\`: .NET build output and release artifacts.
- \`README.md\`: public-safe build, usage, privacy, and limitation documentation.

### Task 1: Create the project and quota parser

**Files:**

- Create: \`.gitignore\`
- Create: \`src/CodexWeekUsageTray/CodexWeekUsageTray.csproj\`
- Create: \`src/CodexWeekUsageTray/Program.cs\`
- Create: \`src/CodexWeekUsageTray/QuotaSnapshot.cs\`

**Interfaces:**

- Produces: \`public sealed record QuotaSnapshot(int UsedPercent, DateTimeOffset ResetsAt)\`.
- Produces: \`public int RemainingPercent { get; }\`.
- Produces: \`public static QuotaSnapshot? ParseWeekly(JsonElement result)\`.
- Produces: \`public static void RunSelfCheck()\`.

- [ ] **Step 1: Create the project file and ignored output paths**

Create the project file as:

\`\`\`xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net10.0-windows</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>
</Project>
\`\`\`

Ignore \`.vs/\`, \`bin/\`, \`obj/\`, and \`artifacts/\`.

- [ ] **Step 2: Implement the exact weekly-window parser**

Walk \`result.rateLimits\` and every object in \`result.rateLimitsByLimitId\`, testing each \`primary\` and \`secondary\` object. Accept only \`windowDurationMins == 10080\`; clamp \`usedPercent\` to 0–100; construct \`DateTimeOffset\` from Unix-seconds \`resetsAt\`; return \`null\` if absent.

\`\`\`csharp
public sealed record QuotaSnapshot(int UsedPercent, DateTimeOffset ResetsAt)
{
    public int RemainingPercent => 100 - UsedPercent;

    public static QuotaSnapshot? ParseWeekly(JsonElement result) =>
        EnumerateWindows(result).FirstOrDefault(window =>
            window.GetProperty("windowDurationMins").GetInt32() == 10_080) is var weekly && weekly.ValueKind != JsonValueKind.Undefined
            ? new QuotaSnapshot(
                Math.Clamp(weekly.GetProperty("usedPercent").GetInt32(), 0, 100),
                DateTimeOffset.FromUnixTimeSeconds(weekly.GetProperty("resetsAt").GetInt64()))
            : null;
}
\`\`\`

- [ ] **Step 3: Add an executable parser check**

\`RunSelfCheck()\` parses a fixed JSON fixture with a 10,080-minute window and asserts \`UsedPercent == 27\`, \`RemainingPercent == 73\`; it also asserts that a 300-minute-only fixture returns \`null\`. Make \`Program.Main\` run that check when invoked as \`--self-test\`; otherwise initialize Windows Forms and run \`TaskbarOverlayForm\`.

\`\`\`csharp
[STAThread]
static void Main(string[] args)
{
    if (args.SequenceEqual(["--self-test"]))
    {
        QuotaSnapshot.RunSelfCheck();
        return;
    }
    ApplicationConfiguration.Initialize();
    Application.Run(new TaskbarOverlayForm());
}
\`\`\`

- [ ] **Step 4: Verify task 1**

Run \`dotnet run --project src/CodexWeekUsageTray -- --self-test\` and \`dotnet build src/CodexWeekUsageTray -warnaserror\`.

Expected: both commands exit \`0\`; parser self-check has no account access.

### Task 2: Add the local Codex App Server client

**Files:**

- Create: \`src/CodexWeekUsageTray/CodexUsageClient.cs\`

**Interfaces:**

- Consumes: \`QuotaSnapshot.ParseWeekly(JsonElement)\`.
- Produces: \`public sealed class CodexUsageClient : IAsyncDisposable\`.
- Produces: \`public event EventHandler<QuotaSnapshot?>? WeeklyQuotaChanged\`.
- Produces: \`public Task<QuotaSnapshot?> RefreshAsync(CancellationToken cancellationToken)\`.

- [ ] **Step 1: Start and initialize the child process**

Start only \`codex app-server --stdio\` with redirected UTF-8 stdin/stdout/stderr, \`CreateNoWindow = true\`, and \`UseShellExecute = false\`. Before all account requests, write these JSONL records:

\`\`\`json
{"method":"initialize","id":1,"params":{"clientInfo":{"name":"codex-weekusage-tray","title":"Codex WeekUsage Tray","version":"1.0.0"}}}
{"method":"initialized","params":{}}
\`\`\`

- [ ] **Step 2: Correlate JSON-RPC requests and immediate changes**

Use an incrementing \`long\` ID and \`ConcurrentDictionary<long, TaskCompletionSource<JsonElement>>\`. A single output-reader parses JSON one line at a time: a response \`id\` resolves the matching task, and \`account/rateLimits/updated\` passes \`params.rateLimits\` to \`ParseWeekly\` then raises \`WeeklyQuotaChanged\`. Do not log JSON lines or process arguments.

- [ ] **Step 3: Read, refresh, and dispose safely**

\`RefreshAsync\` sends \`{ "method": "account/rateLimits/read", "id": n }\`, calls \`ParseWeekly\` on the response result, then raises \`WeeklyQuotaChanged\`. A \`PeriodicTimer\` calls it every five seconds after initialization; failed fallback reads leave the existing snapshot unchanged. \`DisposeAsync\` cancels the timer, completes pending calls with \`ObjectDisposedException\`, closes stdin, and only kills the child process this class started if still running.

- [ ] **Step 4: Verify task 2**

Run \`dotnet build src/CodexWeekUsageTray -warnaserror\` and \`dotnet run --project src/CodexWeekUsageTray -- --self-test\`.

Expected: both exit \`0\`; startup later gives either a weekly snapshot or a concise unavailable state without exposing account data.

### Task 3: Render the persistent overlay and popup

**Files:**

- Create: \`src/CodexWeekUsageTray/TaskbarLocator.cs\`
- Create: \`src/CodexWeekUsageTray/TaskbarOverlayForm.cs\`
- Create: \`src/CodexWeekUsageTray/UsagePopupForm.cs\`
- Modify: \`src/CodexWeekUsageTray/Program.cs\`

**Interfaces:**

- Consumes: \`CodexUsageClient.WeeklyQuotaChanged\`, \`CodexUsageClient.RefreshAsync\`, \`QuotaSnapshot\`.
- Produces: \`public static Rectangle TaskbarLocator.GetPrimaryTaskbarBounds()\`.
- Produces: \`public sealed class TaskbarOverlayForm : Form\`.
- Produces: \`public void UsagePopupForm.ShowFor(QuotaSnapshot? snapshot, Point overlayLocation, Size overlaySize)\`.

- [ ] **Step 1: Locate and track primary taskbar geometry**

P/Invoke \`FindWindow("Shell_TrayWnd", null)\` and \`GetWindowRect\`. Reposition the 78×32 overlay every 250 ms only when taskbar bounds changed. For a horizontal bar anchor at its right end; for vertical bars anchor at its bottom end.

\`\`\`csharp
Location = bounds.Width >= bounds.Height
    ? new Point(bounds.Right - Width - 8, bounds.Top + (bounds.Height - Height) / 2)
    : new Point(bounds.Left + (bounds.Width - Width) / 2, bounds.Bottom - Height - 8);
\`\`\`

- [ ] **Step 2: Paint the counter without a normal taskbar button**

Use \`FormBorderStyle.None\`, \`ShowInTaskbar = false\`, \`TopMost = true\`, \`WS_EX_TOOLWINDOW\`, and \`WS_EX_NOACTIVATE\`. Paint \`7D --\` until a snapshot arrives; otherwise paint \`7D {RemainingPercent}%\`. Apply green for at least 50%, amber for 20–49%, red below 20%.

- [ ] **Step 3: Add popup details and a local-only countdown**

The click popup appears directly above the overlay, closes on deactivation or a second counter click, and displays remaining percentage, used percentage, exact local reset time, and \`N일 N시간 남음\`. Its one-second UI timer recalculates only \`snapshot.ResetsAt - DateTimeOffset.Now\`. The \`새로 고침\` button invokes the provided \`Func<Task>\` which calls \`RefreshAsync\`; when snapshot is null display \`7일 제한 정보를 찾지 못했습니다\` and keep refresh enabled.

- [ ] **Step 4: Wire client lifecycle to UI**

In \`TaskbarOverlayForm.OnShown\`, create the client, subscribe to \`WeeklyQuotaChanged\`, and use \`BeginInvoke\` to update paint and popup state. If the child cannot start or query, retain \`7D --\` and surface only a concise error in the popup. On close, stop UI timers, dispose popup, unsubscribe, and await client disposal through a form-closing-safe cleanup path.

- [ ] **Step 5: Perform Windows manual QA**

Run \`dotnet run --project src/CodexWeekUsageTray\`.

Expected: \`7D xx%\` or \`7D --\` stays visible at the primary taskbar edge without a normal taskbar button. Clicking opens the popup above it; the popup shows local reset time and a ticking countdown when available, toggles closed on second click, and manual refresh does not show account details.

### Task 4: Publish and document the public-safe application

**Files:**

- Create: \`README.md\`
- Modify: \`.gitignore\`

**Interfaces:**

- Consumes: the executable project and \`--self-test\` command.
- Produces: local-only \`artifacts/win-x64/CodexWeekUsageTray.exe\`.

- [ ] **Step 1: Write the README**

Document prerequisites: Windows, installed \`codex\` on \`PATH\`, and an existing Codex sign-in. Include build/run commands, color thresholds, immediate server updates plus five-second fallback, and privacy behavior: no credentials or responses are persisted or logged. State that the counter is an overlay because Windows lacks a stable API to insert arbitrary text into the taskbar.

- [ ] **Step 2: Publish a release executable**

Run \`dotnet publish src/CodexWeekUsageTray -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o artifacts/win-x64\`.

- [ ] **Step 3: Run final verification**

Run \`dotnet run --project src/CodexWeekUsageTray -- --self-test\`, \`dotnet build src/CodexWeekUsageTray -c Release -warnaserror\`, \`git diff --check\`, and \`git status --short\`.

Expected: self-check and release build exit \`0\`, whitespace check has no findings, and all intended files are public-safe.

- [ ] **Step 4: Commit the implementation after review**

Run \`git add .gitignore README.md src docs/superpowers/specs/2026-08-13-codex-weekusage-taskbar-design.md\`, \`git commit -m "feat: add Codex weekly usage taskbar counter"\`, then \`git push\`.

Expected: one reviewed implementation commit on \`origin/main\`.

## Plan Review

Every spec requirement maps to a task: exact 10,080-minute selection, immediate updates, five-second fallback, persistent taskbar-edge counter, click popup, local reset countdown, unavailable state, privacy, and Windows QA. \`QuotaSnapshot\`, \`CodexUsageClient\`, and \`TaskbarOverlayForm\` are defined before they are consumed; no placeholders remain.
