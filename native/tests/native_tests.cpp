#include "../src/core.h"
#include "../src/codex_client.h"
#include "../src/main.h"
#include "fixtures.h"

#include <filesystem>
#include <stdexcept>
#include <string>

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
    Expect(JsonString("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"", "JSON writer must escape control characters.");

    Expect(BuildInitializeRequest(1).find("\"method\":\"initialize\"") != std::string::npos, "Client must initialize first.");
    Expect(BuildLoginRequest(7).find("\"type\":\"chatgpt\"") != std::string::npos, "Client must request managed ChatGPT login.");
    Expect(ParseServerLine(kRateLimitUpdated).kind == CodexEventKind::RateLimitUpdated, "Rate-limit event must be recognized.");
    Expect(ParseServerLine(kLoginCompleted).kind == CodexEventKind::LoginCompleted, "Login completion must be recognized.");

    const auto number_ink = MeasureTrayInk(73);
    Expect(number_ink.left >= 1 && number_ink.right <= 31, "Tray number must fit inside the icon.");
    Expect(number_ink.width >= 20, "Tray number must render both digits rather than only 7.");
    Expect(MeasureTrayInk(std::nullopt).width >= 12, "Pending state must render two dashes.");

    return 0;
}
