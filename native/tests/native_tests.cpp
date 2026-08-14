#include "../src/core.h"
#include "../src/codex_client.h"
#include "../src/cleanup.h"
#include "../src/main.h"
#include "fixtures.h"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

int wmain() {
    using namespace CodexTray;

    Expect(DisplayLabel(std::nullopt) == L"--", "Missing quota must render two dashes.");

    const auto weekly = ParseWeeklyQuota(kWeeklyById);
    Expect(weekly && weekly->used_percent == 27, "Weekly quota must preserve used percentage.");
    Expect(weekly && weekly->remaining_percent == 73, "Weekly 27% used must display 73.");
    Expect(!ParseWeeklyQuota(kFiveHourRateLimits), "A five-hour window is not weekly.");
    Expect(DisplayLabel(weekly) == L"73", "Tray label must contain both digits.");

    Expect(IsOfficialLoginUrl(L"https://chatgpt.com/auth/codex"), "ChatGPT HTTPS URL must be allowed.");
    Expect(IsOfficialLoginUrl(L"https://auth.openai.com/codex/device"), "OpenAI HTTPS URL must be allowed.");
    Expect(!IsOfficialLoginUrl(L"https://chatgpt.com.evil.test/auth"), "Suffix host must be rejected.");
    Expect(!IsOfficialLoginUrl(L"http://chatgpt.com/auth"), "HTTP URL must be rejected.");
    Expect(!IsOfficialLoginUrl(L"https://chatgpt.com@evil.test/auth"), "User info URL must be rejected.");
    Expect(!IsOfficialLoginUrl(L"https://chatgpt.com:444/auth"), "Nonstandard port must be rejected.");

    const auto codex_path = StandardCodexPath(LR"(C:\Users\Test\AppData\Local)");
    Expect(codex_path.wstring() == LR"(C:\Users\Test\AppData\Local\Programs\OpenAI\Codex\bin\codex.exe)", "Codex path must be fixed.");
    std::array<wchar_t, 32768> original_local_app_data{};
    const DWORD original_length = GetEnvironmentVariableW(L"LOCALAPPDATA", original_local_app_data.data(), static_cast<DWORD>(original_local_app_data.size()));
    Expect(SetEnvironmentVariableW(L"LOCALAPPDATA", L"C:\\Untrusted") != FALSE, "Test must set a process-local hostile environment value.");
    const auto installed_codex_path = InstalledCodexPath();
    Expect(SetEnvironmentVariableW(L"LOCALAPPDATA", original_length == 0 ? nullptr : original_local_app_data.data()) != FALSE, "Test must restore the process-local environment value.");
    Expect(installed_codex_path && *installed_codex_path != StandardCodexPath(L"C:\\Untrusted"), "Installed Codex path must ignore inherited LOCALAPPDATA.");
    Expect(JsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"", "JSON writer must escape control characters.");

    Expect(BuildInitializeRequest(1).find("\"method\":\"initialize\"") != std::string::npos, "Client must initialize first.");
    Expect(BuildLoginRequest(7).find("\"type\":\"chatgpt\"") != std::string::npos, "Client must request managed ChatGPT login.");
    Expect(ParseServerLine(kRateLimitUpdated).kind == CodexEventKind::RateLimitUpdated, "Rate-limit event must be recognized.");
    const auto rate_limits_result = ParseServerLine(kRateLimitsResult);
    Expect(rate_limits_result.kind == CodexEventKind::RateLimits && rate_limits_result.quota && rate_limits_result.quota->remaining_percent == 73, "Rate-limit result must carry the weekly quota.");
    Expect(ParseServerLine(kLoginCompleted).kind == CodexEventKind::LoginCompleted, "Login completion must be recognized.");

    const auto number_ink = MeasureTrayInk(73);
    Expect(number_ink.left >= 1 && number_ink.right <= 31, "Tray number must fit inside the icon.");
    Expect(number_ink.width >= 20, "Tray number must render both digits rather than only 7.");
    Expect(MeasureTrayInk(std::nullopt).width >= 12, "Pending state must render two dashes.");

    const auto signed_out = BuildPopupModel(std::nullopt, true, false, L"");
    Expect(signed_out.metric == L"SIGN IN TO CODEX", "Signed-out panel must name Codex.");
    Expect(signed_out.actions == std::vector<PopupAction>{PopupAction::SignIn, PopupAction::Check, PopupAction::Close}, "Login state must expose all actions.");
    const auto ready = BuildPopupModel(QuotaSnapshot{27, 73, 1781395200}, false, false, L"");
    Expect(ready.metric == L"73% LEFT", "Ready panel must show remaining value.");
    Expect(ready.actions == std::vector<PopupAction>{PopupAction::Refresh, PopupAction::Close}, "Ready panel must keep refresh next to close.");

    auto state = ApplyEvent(AppState{}, CodexEvent{CodexEventKind::Account, {}, {}, std::nullopt, false});
    Expect(state.login_required && !state.quota, "Non-ChatGPT account must require login.");
    Expect(state.refresh_finished, "A rejected account check must allow a later sign-in refresh.");
    state = ApplyEvent(state, CodexEvent{CodexEventKind::LoginStarted, "expected", {}, std::nullopt, true});
    const auto ignored = ApplyEvent(state, CodexEvent{CodexEventKind::LoginCompleted, "other", {}, std::nullopt, true});
    Expect(ignored.pending_login_id == state.pending_login_id && ignored.login_required == state.login_required, "Mismatched login completion must be ignored.");
    state = ApplyEvent(state, CodexEvent{CodexEventKind::LoginCompleted, "expected", {}, std::nullopt, true});
    Expect(state.next_request == RequestKind::Account && !state.refresh_finished, "Successful login must request a fresh account check.");
    state = ApplyEvent(state, CodexEvent{CodexEventKind::Account, {}, {}, std::nullopt, true});
    Expect(state.next_request == RequestKind::RateLimits && !state.refresh_finished, "Signed-in account must request limits before completing refresh.");
    state = ApplyEvent(state, CodexEvent{CodexEventKind::RateLimits, {}, {}, QuotaSnapshot{27, 73, 1781395200}, true});
    Expect(state.quota && state.refresh_finished, "Rate-limit result must complete the login refresh.");

    const auto selected = SelectTrayEntries({
        {L"one", L"C:\\One\\CodexWeekUsageTray.exe"},
        {L"two", L"C:\\Two\\CodexWeekUsageTray.exe"},
        {L"other", L"C:\\Other\\OtherTrayApp.exe"},
        {L"suffix", L"C:\\Other\\CodexWeekUsageTray.exe.old"},
        {L"case", L"C:\\Three\\CODEXWEEKUSAGETRAY.EXE"},
    });
    Expect(selected.size() == 3, "Only exact tray EXE basenames may be removed.");
    Expect(selected[0].key_name == L"one" && selected[1].key_name == L"two" && selected[2].key_name == L"case", "Matching entries must be retained case-insensitively.");

    return 0;
}
