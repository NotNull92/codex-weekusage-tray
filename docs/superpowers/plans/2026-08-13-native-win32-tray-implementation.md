# Native Win32 Codex WeekUsage Tray Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the .NET WinForms host with a single native C++17 Win32 EXE that preserves the current Codex weekly-limit tray, popup, login, refresh, and cleanup behavior while materially reducing native-host memory.

**Architecture:** A hidden Win32 owner window owns `Shell_NotifyIcon`, the context menu, a borderless popup window, and all UI state. A small `CodexClient` child-process component starts only the standard OpenAI Codex CLI path, communicates through JSONL pipes, and posts parsed replies/events back to the UI thread. The native host retains the required Codex App Server process; it is the only component allowed to handle ChatGPT OAuth and tokens.

**Tech Stack:** C++17, UCRT64 MinGW `g++` 16.1.0, Win32, Shell32, Ole32, UUID, GDI/msimg32, jsmn (vendored MIT source), no .NET runtime, no package manager, no network client.

## Current implementation record

Tasks 1–7 are complete. The final implementation is split into small native modules: `main.cpp` (entry/reducer), `tray_app.cpp` (owner window, timer, menu, client dispatch), `tray_icon.cpp` (transparent numeric icon), `popup.cpp` and `popup_paint.cpp` (native controls and GDI panel), `cleanup.cpp` (safe direct cleanup), and `codex_protocol.cpp` (JSON-RPC request construction). The older per-task code excerpts below are historical implementation instructions; this map is the current source of truth.

The native host resolves Codex with `SHGetKnownFolderPath(FOLDERID_LocalAppData)`, not an inherited environment variable. The build links MinGW's UUID import library for that Windows known-folder identifier.

## Global Constraints

- Target only Windows 10/11 x64.
- Preserve the current user-facing English copy, Codex lavender-to-blue colors, large `--`/percentage tray text, 368-by-282 popup layout, menu actions, and five-second fallback refresh.
- Use only `%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe`; never resolve `codex.exe` through the current directory or `PATH`.
- Accept OAuth URLs only when absolute, HTTPS, and the exact hostname is `chatgpt.com` or `auth.openai.com`.
- The native host must never receive, print, write, or upload a password, API key, refresh token, or account data.
- Release output is one `CodexWeekUsageTray.exe`, no PDB, no .NET content, no uninstaller script. `--uninstall` and `--uninstall-dry-run` live in the EXE.
- Retain `src/` and existing tests until native parity and release QA pass. Delete them only in the final, isolated cleanup task.
- Every production behavior starts with a native failing test. Tests must avoid real browser login and must not mutate the user’s live tray-settings key.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `native/build.cmd` | Uses the installed UCRT64 MinGW compiler to build release, tests, and symbols-free artifacts. |
| `native/third_party/jsmn.h` | Pinned upstream MIT JSON tokenizer source and license notice. |
| `native/src/core.h` / `native/src/core.cpp` | Pure JSON parsing/escaping, quota selection, time formatting, secure URL validation, and standard Codex-path construction. |
| `native/src/codex_client.h` / `native/src/codex_client.cpp` | Process/pipes, JSON-RPC framing, response correlation, and immutable UI event messages. |
| `native/src/main.cpp` | Application entry point and pure UI state reducer. |
| `native/src/tray_app.cpp` | Hidden owner window, Shell notification icon, menu, timer, client event dispatch, and browser launch. |
| `native/src/tray_icon.cpp` | Transparent 32×32 bold lavender `--`/percentage icon renderer. |
| `native/src/popup.cpp` / `native/src/popup_paint.cpp` | Native popup controls and GDI neon panel drawing. |
| `native/src/cleanup.cpp` | Direct, current-user notification-icon cleanup. |
| `native/src/codex_protocol.cpp` | JSON-RPC request construction. |
| `native/tests/native_tests.cpp` | Deterministic core, protocol, and uninstaller-selection unit tests. |
| `native/tests/fixtures.h` | Synthetic JSONL responses only; no personal account data. |
| `README.md` | Native build/run/security/remove instructions and verified memory/size result. |
| `.gitignore` | Ignore `native/out/` build products. |

---

### Task 1: Establish a reproducible native build and test harness

**Files:**
- Create: `native/build.cmd`
- Create: `native/tests/native_tests.cpp`
- Modify: `.gitignore`

**Interfaces:**
- Produces `native/out/CodexWeekUsageTray.exe` and `native/out/CodexWeekUsageTrayTests.exe`.
- Test entry point is `int wmain()` and returns zero only after every assertion passes.

- [x] **Step 1: Write the failing test harness**

Create `native/tests/native_tests.cpp` containing a deliberately unresolved core call:

```cpp
#include "../src/core.h"

#include <stdexcept>

int wmain() {
    if (CodexTray::DisplayLabel(std::nullopt) != L"--") {
        throw std::runtime_error("Missing quota must render two dashes.");
    }
}
```

- [x] **Step 2: Run the test to verify it fails for the missing module**

Run:

```powershell
cmd /d /c native\build.cmd tests
```

Expected: compilation fails because `native/src/core.h` does not exist.

- [x] **Step 3: Add the minimal compiler command and test-only header declaration**

Create `native/build.cmd` with a fixed UCRT64 compiler path, an `out` directory, and the command shape below. Create only the declaration required for the initial test; later tasks add implementation.

```bat
@echo off
setlocal
set "ROOT=%~dp0"
set "CXX=C:\Users\PC\msys64\ucrt64\bin\g++.exe"
set "OUT=%ROOT%out"
if not exist "%OUT%" mkdir "%OUT%"
if /I "%~1"=="tests" (
  "%CXX%" -std=c++17 -Wall -Wextra -Werror -I"%ROOT%src" "%ROOT%tests\native_tests.cpp" "%ROOT%src\core.cpp" -o "%OUT%\CodexWeekUsageTrayTests.exe"
  exit /b %errorlevel%
)
"%CXX%" -std=c++17 -Os -s -static -municode -mwindows -I"%ROOT%src" "%ROOT%src\main.cpp" "%ROOT%src\core.cpp" "%ROOT%src\codex_client.cpp" -lgdiplus -lshell32 -lcomctl32 -lole32 -lwinhttp -o "%OUT%\CodexWeekUsageTray.exe"
```

Add `native/out/` to `.gitignore`.

- [x] **Step 4: Run the test to verify the harness executes**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: exit 0 after `DisplayLabel` is minimally implemented in Task 2.

- [x] **Step 5: Commit the harness**

```bash
git add .gitignore native/build.cmd native/tests/native_tests.cpp native/src/core.h native/src/core.cpp
git commit -m "build: add native test harness"
```

### Task 2: Implement and lock down pure quota, path, URL, and JSON helpers

**Files:**
- Create: `native/third_party/jsmn.h`
- Create: `native/src/core.h`
- Create: `native/src/core.cpp`
- Create: `native/tests/fixtures.h`
- Modify: `native/tests/native_tests.cpp`

**Interfaces:**
- `std::optional<QuotaSnapshot> ParseWeeklyQuota(std::string_view json)` returns only a 10,080-minute quota window.
- `std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota)` returns `--` or an unclipped full remaining percentage.
- `bool IsOfficialLoginUrl(std::wstring_view url)` accepts only exact approved HTTPS hosts.
- `std::filesystem::path StandardCodexPath(std::wstring_view local_app_data)` creates the fixed CLI location.
- `std::string JsonString(std::string_view value)` returns an escaped JSON string literal.

- [x] **Step 1: Write the failing pure behavior tests**

Extend `native/tests/native_tests.cpp` using only synthetic fixtures:

```cpp
Expect(ParseWeeklyQuota(kWeeklyById)->remaining_percent == 73, "Weekly 27% used must display 73.");
Expect(!ParseWeeklyQuota(kFiveHourRateLimits), "A five-hour window is not weekly.");
Expect(DisplayLabel(ParseWeeklyQuota(kWeeklyById)) == L"73", "Tray label must contain both digits.");
Expect(IsOfficialLoginUrl(L"https://chatgpt.com/auth/codex"), "ChatGPT HTTPS URL must be allowed.");
Expect(IsOfficialLoginUrl(L"https://auth.openai.com/codex/device"), "OpenAI HTTPS URL must be allowed.");
Expect(!IsOfficialLoginUrl(L"https://chatgpt.com.evil.test/auth"), "Suffix host must be rejected.");
Expect(!IsOfficialLoginUrl(L"http://chatgpt.com/auth"), "HTTP URL must be rejected.");
Expect(StandardCodexPath(LR"(C:\Users\Test\AppData\Local)").wstring() == LR"(C:\Users\Test\AppData\Local\Programs\OpenAI\Codex\bin\codex.exe)", "Codex path must be fixed.");
Expect(JsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"", "JSON writer must escape control characters.");
```

- [x] **Step 2: Run the test to verify it fails for unimplemented core behavior**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: fail at the first new assertion or missing symbol, not due to live Codex access.

- [x] **Step 3: Vendor the pinned tokenizer and implement exact helpers**

Fetch only the `jsmn.h` release file from the upstream MIT project, retain its copyright/license text, and record the exact upstream tag/commit in the accompanying license notice. Build a small token traversal wrapper that reads only object strings, booleans, numbers, and nested objects needed by:

```cpp
namespace CodexTray {
struct QuotaSnapshot {
    int used_percent;
    int remaining_percent;
    std::int64_t resets_at_unix;
};

std::optional<QuotaSnapshot> ParseWeeklyQuota(std::string_view json);
std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota);
bool IsOfficialLoginUrl(std::wstring_view url);
std::filesystem::path StandardCodexPath(std::wstring_view local_app_data);
std::string JsonString(std::string_view value);
}
```

`ParseWeeklyQuota` must examine both `rateLimits.primary` and the `rateLimitsByLimitId` object values, and select a window only when `windowDurationMins == 10080`. Clamp `usedPercent` to 0–100 before calculating `100 - usedPercent`. URL parsing must use `WinHttpCrackUrl` or equivalent Win32 parsing, compare hosts case-insensitively, reject userinfo, ports other than default HTTPS, fragments, and non-HTTPS schemes.

- [x] **Step 4: Run the pure native tests to verify they pass**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: exit 0; no Codex child or browser starts.

- [x] **Step 5: Commit the safe protocol core**

```bash
git add native/third_party/jsmn.h native/src/core.h native/src/core.cpp native/tests/fixtures.h native/tests/native_tests.cpp
git commit -m "feat: add native Codex protocol core"
```

### Task 3: Add a bounded native Codex App Server client

**Files:**
- Create: `native/src/codex_client.h`
- Create: `native/src/codex_client.cpp`
- Modify: `native/tests/native_tests.cpp`

**Interfaces:**
- `CodexClient::Start(HWND receiver)` launches the fixed path with `app-server --stdio` and begins one reader thread.
- `CodexClient::RequestAccount()`, `RequestRateLimits()`, `StartChatGptLogin()`, and `CancelLogin(std::string_view)` write newline-delimited JSON-RPC.
- `WM_APP_CODEX_EVENT` carries an owning `CodexEvent*` allocated by the reader and freed by the receiving UI handler.
- `CodexClient::Stop()` closes pipes, requests reader stop, waits, then terminates only the child it created.

- [x] **Step 1: Write failing JSON-RPC framing and event tests**

Add a fake-line parser test that proves exact request and response routing before any process starts:

```cpp
Expect(BuildInitializeRequest(1).find("\"method\":\"initialize\"") != std::string::npos, "Client must initialize first.");
Expect(BuildLoginRequest(7).find("\"type\":\"chatgpt\"") != std::string::npos, "Client must request managed ChatGPT login.");
Expect(ParseServerLine(kRateLimitUpdated).kind == CodexEventKind::RateLimitUpdated, "Rate-limit event must be recognized.");
Expect(ParseServerLine(kLoginCompleted).kind == CodexEventKind::LoginCompleted, "Login completion must be recognized.");
```

- [x] **Step 2: Run the test to verify it fails for the absent client module**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: compile failure for `codex_client.h` declarations.

- [x] **Step 3: Implement process, pipe, request, and event ownership rules**

Implement the following concrete lifecycle:

```cpp
enum class CodexEventKind { Started, Account, RateLimits, RateLimitUpdated, LoginStarted, LoginCompleted, Error };
struct CodexEvent { CodexEventKind kind; std::string login_id; std::wstring authorization_url; std::optional<QuotaSnapshot> quota; bool success{}; };

constexpr UINT WM_APP_CODEX_EVENT = WM_APP + 41;
```

- Create stdin/stdout/stderr anonymous pipes with non-inheritable parent ends.
- Build a quoted command line from only `StandardCodexPath(LocalAppData)` plus the literal ` app-server --stdio`.
- Use `CREATE_NO_WINDOW`, no shell, no environment changes, and zero current-directory override.
- Send `initialize` and then `initialized` before an account request. Serialize writes under one mutex.
- Reader thread accumulates bytes until `\n`, accepts at most 1 MiB per JSON line, parses the line, and posts a heap-owned `CodexEvent` with `PostMessageW`.
- Route JSON-RPC errors into an `Error` event using only the generic user-safe text; never copy server payloads to logs or UI.
- Ignore unknown notifications and completion events whose `loginId` does not match UI state.

- [x] **Step 4: Run deterministic client tests and an offline app-server startup check**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Then, with no browser login action:

```powershell
native\out\CodexWeekUsageTray.exe --self-test
```

Expected: tests exit 0; self-test only verifies parsing/framing and does not start a browser.

- [x] **Step 5: Commit the client**

```bash
git add native/src/codex_client.h native/src/codex_client.cpp native/tests/native_tests.cpp native/tests/fixtures.h native/build.cmd
git commit -m "feat: add native Codex app server client"
```

### Task 4: Build the native tray shell and high-contrast Codex icon

**Files:**
- Create: `native/src/main.cpp`
- Modify: `native/build.cmd`
- Modify: `native/tests/native_tests.cpp`

**Interfaces:**
- `NativeApp::Run(HINSTANCE)` creates the owner window and message loop.
- Tray callback message `WM_APP_TRAY = WM_APP + 40` supports left-click toggle and context menu.
- `HICON CreateTrayIcon(std::optional<QuotaSnapshot>)` draws `--` or the complete decimal percentage centered in 32×32 pixels.

- [x] **Step 1: Write the failing tray-icon geometry test**

Add a testable bitmap-render function and assert that both characters of `73` have visible non-transparent ink inside the 32×32 canvas:

```cpp
const auto bounds = MeasureInkBounds(RenderTrayBitmap(73));
Expect(bounds.left >= 1 && bounds.right <= 31, "Tray number must fit inside the icon.");
Expect(bounds.width >= 20, "Tray number must render both digits rather than only 7.");
Expect(MeasureInkBounds(RenderTrayBitmap(std::nullopt)).width >= 12, "Pending state must render two dashes.");
```

- [x] **Step 2: Run the test to verify it fails for the missing renderer**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: compile failure for the native renderer API.

- [x] **Step 3: Implement the shell and renderer with Win32 ownership**

Implement one hidden top-level owner window and one `NOTIFYICONDATAW` record with `NIF_ICON | NIF_MESSAGE | NIF_TIP`. Render transparent 32×32 ARGB DIBs via GDI+, choose the largest fitted bold `Cascadia Mono` or `Segoe UI` font, and fill text with `#95A2FF` to `#4A59FE`. Replace the icon only when the label changes; destroy the previous `HICON` after `Shell_NotifyIconW(NIM_MODIFY, ...)` has accepted the new one.

Create a context menu containing exactly:

```text
Show panel
Refresh
Sign in to Codex
──────────────
Quit
```

On `WM_LBUTTONUP` / `NIN_SELECT`, toggle the panel. On `WM_RBUTTONUP`, use `SetForegroundWindow` then `TrackPopupMenu` so standard Windows tray behavior remains intact. The hidden owner is the only long-lived top-level window; it never occupies taskbar space.

- [x] **Step 4: Build and manually verify the native notification icon**

Run:

```powershell
cmd /d /c native\build.cmd
native\out\CodexWeekUsageTray.exe
```

Expected: a normal notification-area icon is visible or present in Windows overflow, does not cover the clock, shows `--` initially, and right-click displays all four commands. Close through **Quit** before the next task.

- [x] **Step 5: Commit the tray shell**

```bash
git add native/src/main.cpp native/build.cmd native/tests/native_tests.cpp
git commit -m "feat: add native tray shell"
```

### Task 5: Implement the neon popup and exact native interaction states

**Files:**
- Modify: `native/src/main.cpp`
- Modify: `native/tests/native_tests.cpp`

**Interfaces:**
- `PopupState` contains quota, login-required, login-in-progress, and a short safe error code/message.
- `ShowPopupNearCursor()` and `HidePopup()` are UI-thread-only.
- Commands `ID_SIGN_IN`, `ID_REFRESH`, and `ID_CLOSE` call into the owner-window action handlers.

- [ ] **Step 1: Write the failing UI-state text and action-layout tests**

Make popup model methods deterministic and test them without a live window:

```cpp
const auto signedOut = BuildPopupModel(std::nullopt, true, false, L"");
Expect(signedOut.metric == L"SIGN IN TO CODEX", "Signed-out panel must name Codex.");
Expect(signedOut.actions == std::vector<Action>{Action::SignIn, Action::Check, Action::Close}, "Login state must expose all actions.");
const auto ready = BuildPopupModel(QuotaSnapshot{27, 73, 1781395200}, false, false, L"");
Expect(ready.metric == L"73% LEFT", "Ready panel must show remaining value.");
Expect(ready.actions == std::vector<Action>{Action::Refresh, Action::Close}, "Ready panel must keep refresh next to close.");
```

- [ ] **Step 2: Run the test to verify the popup model fails before implementation**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: fail for missing `BuildPopupModel`.

- [ ] **Step 3: Implement the popup window and controls**

Create a borderless, non-taskbar popup with `WS_POPUP`, 368×282 logical pixels, `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`, and a 16-pixel scaled inset. Paint the existing dark surface, two blue glow fields, two thin neon frames, and corner bars with GDI+. Use child `STATIC` and `BUTTON` controls for keyboard accessibility and visible native text. Place actions exactly as follows:

```text
login needed: Sign in 16,230 110×36; Check 134,230 96×36; Close 238,230 114×36
ready:        Refresh 16,230 214×36; Close 238,230 114×36
```

Use the current simple English states:

```text
CODEX
WEEKLY LIMIT
SIGN IN TO CODEX / NO WEEKLY LIMIT / NN% LEFT
Open your browser to sign in.
Then see your weekly limit here.
Used: NN%
Resets: Mon, Aug 16, 9:30 AM UTC
Time left: 3d 4h
```

Position above the cursor when it fits, otherwise below it, inside the monitor work area. Start its one-second `WM_TIMER` only while visible; it updates the countdown without making a network request. **Close** hides only the popup.

- [ ] **Step 4: Build and visually QA popup states**

Run the EXE with native `--preview-pending` and `--preview-ready=73` switches that create no App Server child and no persistent state. Inspect both at 100%, 125%, and 150% Windows scaling:

```powershell
native\out\CodexWeekUsageTray.exe --preview-pending
native\out\CodexWeekUsageTray.exe --preview-ready=73
```

Expected: all English text and buttons are fully visible, **Close** is right-most, neon elements are drawn by Win32/GDI+ rather than a screenshot, and the system tray remains uncluttered.

- [ ] **Step 5: Commit the popup**

```bash
git add native/src/main.cpp native/tests/native_tests.cpp
git commit -m "feat: add native Codex limit panel"
```

### Task 6: Connect native UI actions to live App Server quota and login flows

**Files:**
- Modify: `native/src/main.cpp`
- Modify: `native/src/codex_client.h`
- Modify: `native/src/codex_client.cpp`
- Modify: `native/tests/native_tests.cpp`

**Interfaces:**
- `NativeApp::HandleCodexEvent(std::unique_ptr<CodexEvent>)` owns all state transitions.
- One `pending_login_id_` is used to correlate cancellation and completion.
- `RefreshNow()` does `account/read`, then `account/rateLimits/read` only for a ChatGPT account.

- [ ] **Step 1: Write failing login and refresh state-transition tests**

Test only models and parsed synthetic events:

```cpp
auto state = ApplyEvent(EmptyState(), AccountEvent(false));
Expect(state.login_required && !state.quota, "Non-ChatGPT account must require login.");
state = ApplyEvent(state, AccountEvent(true));
Expect(state.next_request == RequestKind::RateLimits, "ChatGPT account must request limits.");
state.pending_login_id = "expected";
Expect(ApplyEvent(state, LoginCompletedEvent("other", true)) == state, "Mismatched login completion must be ignored.");
Expect(ApplyEvent(state, LoginCompletedEvent("expected", false)).login_required, "Failed matching login must remain actionable.");
```

- [ ] **Step 2: Run the test to verify the state reducer fails before integration**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: fail for the missing state reducer or incorrect transition.

- [ ] **Step 3: Implement the exact event flow**

Implement these UI-thread transitions:

1. Startup and **Refresh** ensure one running client, send `account/read` with `refreshToken:false`, then fetch limits only for account type `chatgpt`.
2. Receive `account/rateLimits/updated` and update the icon/panel immediately; retain the last valid snapshot only while a refresh error occurs.
3. Run `account/rateLimits/read` every 5 seconds with a coalescing `refresh_pending_` flag so no overlapping request is written.
4. **Sign in** cancels `pending_login_id_` if set, calls `account/login/start` with `type:"chatgpt"`, `useHostedLoginSuccessPage:true`, and `appBrand:"codex"`, validates the returned URL with `IsOfficialLoginUrl`, and opens it using `ShellExecuteW`.
5. On a matching successful `account/login/completed`, run the account-and-limits refresh. On failure, retain `Sign-in did not finish.` and action buttons. Browser-launch or server errors show only short generic copy.

- [ ] **Step 4: Run deterministic tests and safe live account QA**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --self-test
native\out\CodexWeekUsageTray.exe
```

Expected: tests and self-test exit 0. The live run obtains the existing account/weekly value or a clear login-required state, updates at most once every five seconds, and does not open a browser unless the user clicks **Sign in**.

- [ ] **Step 5: Commit the live integration**

```bash
git add native/src/main.cpp native/src/codex_client.h native/src/codex_client.cpp native/tests/native_tests.cpp
git commit -m "feat: connect native tray to Codex usage"
```

### Task 7: Move uninstaller behavior into the native EXE

**Files:**
- Modify: `native/src/main.cpp`
- Modify: `native/tests/native_tests.cpp`
- Modify: `README.md`

**Interfaces:**
- `CodexWeekUsageTray.exe --uninstall` removes matching current-user tray settings immediately.
- `CodexWeekUsageTray.exe --uninstall-dry-run` prints matching paths and makes no changes.
- Production command line accepts no registry-root override.

- [ ] **Step 1: Write failing target-selection tests**

Define a pure selection input and prove exact basename behavior:

```cpp
const auto selected = SelectTrayEntries({
    {L"one", L"C:\\One\\CodexWeekUsageTray.exe"},
    {L"two", L"C:\\Two\\CodexWeekUsageTray.exe"},
    {L"other", L"C:\\Other\\OtherTrayApp.exe"},
});
Expect(selected.size() == 2, "Only exact tray EXE basenames may be removed.");
Expect(selected[0].key_name == L"one" && selected[1].key_name == L"two", "Matching entries must be retained.");
```

- [ ] **Step 2: Run the test to verify selection fails before implementation**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
```

Expected: fail for missing `SelectTrayEntries`.

- [ ] **Step 3: Implement registry and process cleanup**

Open only `HKCU\Control Panel\NotifyIconSettings`, enumerate direct subkeys, read `ExecutablePath`, and select exact case-insensitive file name matches. In dry-run mode print `Found N ...` and no changes. Otherwise collect selected full paths, enumerate running processes with Toolhelp32, query each process image path, terminate only those whose full path matches the selected set, then call `RegDeleteTreeW` only on the selected direct key names. Report the number of settings removed and processes stopped. Never delete application files.

- [ ] **Step 4: Run isolated registry-helper and real dry-run checks**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --uninstall-dry-run
```

Expected: tests exit 0; dry-run reports zero or exact current matching entries, and does not alter the registry.

- [ ] **Step 5: Commit native cleanup**

```bash
git add native/src/main.cpp native/tests/native_tests.cpp README.md
git commit -m "feat: add native tray cleanup"
```

### Task 8: Complete parity, security, and memory QA before removing C#

**Files:**
- Modify: `README.md`
- Create: `docs/superpowers/evidence/2026-08-13-native-win32-qa.md`

**Interfaces:**
- Native release version is `2.0.0`.
- Evidence records native EXE byte count, hash, private memory, working set, Codex-child memory, and exact validation commands.

- [ ] **Step 1: Create the release verification checklist before release packaging**

Add a checklist containing each expected artifact and scenario:

```text
[ ] Tests and --self-test pass.
[ ] Release has exactly CodexWeekUsageTray.exe.
[ ] Authenticode state is recorded.
[ ] SHA-256 manifest matches archive and EXE.
[ ] ESET scan has zero detections.
[ ] Normal tray / ready popup / login-required popup visually pass.
[ ] Native host private memory is materially below 60 MiB; Codex child measured separately.
```

- [ ] **Step 2: Verify the checklist initially fails because release artifacts do not exist**

Run:

```powershell
Test-Path artifacts\release-v2.0.0\CodexWeekUsageTray.exe
```

Expected: `False` before packaging.

- [ ] **Step 3: Produce and inspect the release artifact**

Build `native/out/CodexWeekUsageTray.exe`, copy only it into `artifacts/release-v2.0.0/`, run its `--self-test`, use `Get-AuthenticodeSignature`, calculate SHA-256, archive it with `SHA256SUMS-v2.0.0.txt`, and inspect ZIP central-directory entries. Run ESET with `/clean-mode=none` on both the release folder and ZIP.

Use a fresh released native process and its direct Codex child to take three five-second samples with `Get-Process`. Record private memory and working set separately. The native host passes only if private memory is materially lower than 60 MiB; do not count Codex App Server memory as host savings.

- [ ] **Step 4: Perform Windows UI and behavior QA**

Use Computer Use read-only visual checks on the Release EXE:

1. Tray icon shows `--` on missing value and both digits for 73.
2. Clicking opens the native panel; **Close** hides only the panel.
3. Every English label and button is readable at 100%, 125%, and 150% scaling.
4. Right-click menu shows the four required actions.
5. Sign-in-required panel offers **Sign in** and **Check**; click sign-in only if explicitly authorizing a browser OAuth attempt in that QA session.
6. No window overlaps the taskbar clock/date.

- [ ] **Step 5: Commit QA evidence and native release documentation**

```bash
git add README.md docs/superpowers/evidence/2026-08-13-native-win32-qa.md
git commit -m "docs: verify native tray release"
```

### Task 9: Remove the superseded managed implementation and publish v2.0.0

**Files:**
- Delete: `src/CodexWeekUsageTray/`
- Delete: `tests/CodexWeekUsageTray.Tests/`
- Delete: `tests/Uninstall-CodexWeekUsageTray.Tests.ps1`
- Delete: `uninstall.cmd`
- Delete: `uninstall.ps1`
- Modify: `README.md`

**Interfaces:**
- The repository has one production implementation: `native/`.
- `CodexWeekUsageTray.exe --uninstall` replaces both removed uninstaller scripts.

- [ ] **Step 1: Add a failing repository-surface assertion**

Add to native tests or release script:

```cpp
Expect(std::filesystem::exists(L"native/src/main.cpp"), "Native implementation must remain present.");
```

And add an artifact check ensuring no managed DLL, runtimeconfig, PDB, `.csproj`, or script is in the release archive.

- [ ] **Step 2: Run tests and record that the managed source still exists before cleanup**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
Test-Path src\CodexWeekUsageTray\CodexWeekUsageTray.csproj
```

Expected: native tests pass and the final command is `True` before deletion.

- [ ] **Step 3: Delete only the superseded implementation after exact target confirmation**

First list the exact five targets above and confirm each resolves inside this repository. Then remove only those paths with PowerShell `Remove-Item -LiteralPath`; do not remove `artifacts/`, docs, or Git history. Update README commands to use:

```powershell
cmd /d /c native\build.cmd
native\out\CodexWeekUsageTray.exe
native\out\CodexWeekUsageTray.exe --uninstall
```

- [ ] **Step 4: Run complete native and package verification after cleanup**

Run:

```powershell
cmd /d /c native\build.cmd tests
native\out\CodexWeekUsageTrayTests.exe
native\out\CodexWeekUsageTray.exe --self-test
git status --short
```

Expected: native checks pass; working tree contains only intended deletion/documentation changes before commit.

- [ ] **Step 5: Commit the replacement, then create a public v2.0.0 release**

```bash
git add -A
git commit -m "feat: replace managed tray with native Win32 app"
git push origin feat/native-win32
```

Then fast-forward `main` only after confirming its worktree is clean and still at the reviewed base, and create the release from that exact `main` commit:

```bash
git -C C:/Users/PC/Desktop/Cowork/codex-weekusage-tray merge --ff-only feat/native-win32
git -C C:/Users/PC/Desktop/Cowork/codex-weekusage-tray push origin main
gh release create v2.0.0 artifacts/CodexWeekUsageTray-win-x64-v2.0.0.zip artifacts/SHA256SUMS-v2.0.0.txt --target main --title "v2.0.0" --notes "Native Win32 rewrite: no .NET runtime required. Verify SHA-256 before running. The EXE is not Authenticode-signed yet."
```

Verify the public download’s ZIP hash, the contained EXE hash, and the exact three or fewer expected archive entries before reporting completion.

---

## Plan Self-Review

- **Spec coverage:** Tasks 1–3 cover the native toolchain and official App Server JSONL lifecycle; 4–6 cover tray, popup, browser login, direct notifications, and five-second refresh; 7 covers immediate cleanup; 8 measures memory, security, package contents, and visual parity; 9 removes only the retired managed implementation after evidence exists.
- **No-placeholder check:** The plan defines concrete paths, function names, test commands, expected failures, UI coordinates, release names, and deletion targets. The external jsmn source is explicitly pinned and license-preserved rather than unspecified.
- **Type consistency:** `QuotaSnapshot`, `CodexEvent`, `CodexEventKind`, `CodexClient`, `PopupState`, `BuildPopupModel`, and `WM_APP_CODEX_EVENT` retain one definition and one direction of ownership across tasks.
