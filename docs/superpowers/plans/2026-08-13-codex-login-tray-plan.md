# Codex Login and System Tray Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show Codex weekly usage through a standard Windows notification-area icon and let unauthenticated users start the Codex-managed ChatGPT login flow.

**Architecture:** `CodexUsageClient` owns App Server authentication and quota RPCs. `TrayApplicationContext` owns the notification icon, popup, context menu, browser-login action, and shutdown; it replaces the taskbar overlay form and locator.

**Tech Stack:** .NET 10, Windows Forms `NotifyIcon`, Codex App Server JSON-RPC, no external packages.

## Global Constraints

- Windows 10/11 and .NET 10 Windows Forms only.
- The app never reads, stores, logs, or sends account credentials itself.
- Open only the HTTPS authorization URL returned by the Codex App Server.
- Respect Windows **Other system tray icons** visibility settings; never force pinning.

---

### Task 1: Account-state parsing

**Files:**
- Create: `src/CodexWeekUsageTray/CodexAccountStatus.cs`
- Modify: `tests/CodexWeekUsageTray.Tests/Program.cs`

**Interfaces:**
- Produces: `CodexAccountStatus.Parse(JsonElement result)` and `CanReadWeeklyQuota`.
- Consumes: the documented `account/read` response where `account.type` is `chatgpt` or `account` is `null`.

- [x] **Step 1: Write the failing test**

```csharp
using var signedOut = JsonDocument.Parse("""{ "account": null, "requiresOpenaiAuth": true }""");
if (CodexAccountStatus.Parse(signedOut.RootElement).CanReadWeeklyQuota)
    throw new InvalidOperationException("A missing ChatGPT account must require login.");
```

- [x] **Step 2: Run test to verify it fails**

Run: `dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj`

Expected: compilation failure because `CodexAccountStatus` does not exist.

- [x] **Step 3: Write minimal implementation**

```csharp
public sealed record CodexAccountStatus(bool CanReadWeeklyQuota)
{
    public static CodexAccountStatus Parse(JsonElement result) =>
        new(result.TryGetProperty("account", out var account)
            && account.ValueKind == JsonValueKind.Object
            && account.TryGetProperty("type", out var type)
            && type.GetString() == "chatgpt");
}
```

- [x] **Step 4: Run test to verify it passes**

Run: `dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj`

Expected: exit 0.

### Task 2: App Server account and login methods

**Files:**
- Modify: `src/CodexWeekUsageTray/CodexUsageClient.cs`
- Modify: `tests/CodexWeekUsageTray.Tests/Program.cs`

**Interfaces:**
- Consumes: `CodexAccountStatus.Parse(JsonElement)`.
- Produces: `ReadAccountAsync`, `StartChatGptLoginAsync`, `LoginCompleted` event.

- [x] **Step 1: Write the failing test for the HTTPS login URL contract**

```csharp
using var loginResponse = JsonDocument.Parse("""
{ "type": "chatgpt", "loginId": "login-1", "authUrl": "https://chatgpt.com/auth" }
""");
if (CodexLoginStart.Parse(loginResponse.RootElement).AuthorizationUrl != new Uri("https://chatgpt.com/auth"))
    throw new InvalidOperationException("The ChatGPT login start response must retain its HTTPS authorization URL.");
```

- [x] **Step 2: Run test to verify it fails**

Run: `dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj`

Expected: compilation failure because `CodexLoginStart` does not exist.

- [x] **Step 3: Implement account read, managed browser login, and login completion parsing**

```csharp
public async Task<CodexAccountStatus> ReadAccountAsync(CancellationToken token) =>
    CodexAccountStatus.Parse(await SendRequestAsync("account/read", new { refreshToken = false }, token));

public async Task<Uri> StartChatGptLoginAsync(CancellationToken token) =>
    CodexLoginStart.Parse(await SendRequestAsync(
        "account/login/start",
        new { type = "chatgpt", useHostedLoginSuccessPage = true, appBrand = "codex" },
        token)).AuthorizationUrl;
```

Parse `account/login/completed` in the read loop and raise `LoginCompleted` without persisting response data.

- [x] **Step 4: Run all checks**

Run: `dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj && dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -- --self-test`

Expected: exit 0.

### Task 3: Notification-area host and popup login action

**Files:**
- Create: `src/CodexWeekUsageTray/TrayApplicationContext.cs`
- Create: `src/CodexWeekUsageTray/TrayIconRenderer.cs`
- Modify: `src/CodexWeekUsageTray/UsagePopupForm.cs`
- Modify: `src/CodexWeekUsageTray/Program.cs`
- Delete: `src/CodexWeekUsageTray/TaskbarOverlayForm.cs`
- Delete: `src/CodexWeekUsageTray/TaskbarLocator.cs`

**Interfaces:**
- Consumes: `CodexUsageClient.ReadAccountAsync`, `StartChatGptLoginAsync`, `WeeklyQuotaChanged`, and `LoginCompleted`.
- Produces: a normal `NotifyIcon` with a context menu, an anchored status popup, and a login button when no ChatGPT session exists.

- [x] **Step 1: Write the failing build check for the new application host**

Run: `dotnet build src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -warnaserror`

Expected: the old visible overlay still builds; use the existing test project as the red test for the new account/login contracts from Tasks 1–2 before changing the host.

- [x] **Step 2: Implement the minimal notification-area host**

```csharp
ApplicationConfiguration.Initialize();
Application.Run(new TrayApplicationContext());
```

`TrayApplicationContext` must set the tooltip to login-required, loading, error, or weekly-remaining state; left-click opens `UsagePopupForm`, and its menu exposes status, refresh, login, and exit.

- [x] **Step 3: Implement popup login action**

```csharp
_loginButton.Visible = login is not null;
_loginButton.Click += async (_, _) => await login();
```

When a browser login is started, show progress and guard duplicate starts until the completion notification or a failure restores the state.

- [x] **Step 4: Run all automated checks**

Run: `dotnet run --project tests/CodexWeekUsageTray.Tests/CodexWeekUsageTray.Tests.csproj && dotnet run --project src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -- --self-test && dotnet build src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -warnaserror`

Expected: exit 0 with no warnings.

### Task 4: Public documentation and Windows QA

**Files:**
- Modify: `README.md`

- [x] **Step 1: Document the tray-only interaction and login privacy boundary**

State that Windows controls tray pinning/overflow, the app opens the Codex-managed ChatGPT sign-in page only after the user requests login, and the app does not handle credentials.

- [ ] **Step 2: Publish and manually verify**

Run: `dotnet publish src/CodexWeekUsageTray/CodexWeekUsageTray.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o artifacts/win-x64`

Expected: a visible notification-area icon, no overlap with the clock/date, status popup on click, and a login action when a ChatGPT session is unavailable.
