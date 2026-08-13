using System.Text.Json;
using CodexWeekUsageTray;

using var weeklyResponse = JsonDocument.Parse("""
{
  "rateLimitsByLimitId": {
    "codex_weekly": {
      "primary": {
        "usedPercent": 27,
        "windowDurationMins": 10080,
        "resetsAt": 1781395200
      }
    }
  }
}
""");
var weekly = QuotaSnapshot.ParseWeekly(weeklyResponse.RootElement)
    ?? throw new InvalidOperationException("Expected a seven-day quota window.");
if (weekly.UsedPercent != 27 || weekly.RemainingPercent != 73)
{
    throw new InvalidOperationException("The seven-day window must produce 27% used and 73% remaining.");
}

using var shortResponse = JsonDocument.Parse("""
{
  "rateLimits": {
    "primary": {
      "usedPercent": 27,
      "windowDurationMins": 300,
      "resetsAt": 1781395200
    }
  }
}
""");
if (QuotaSnapshot.ParseWeekly(shortResponse.RootElement) is not null)
{
    throw new InvalidOperationException("A non-weekly rate limit must not be displayed as seven-day usage.");
}

using var signedOutAccount = JsonDocument.Parse("""
{ "account": null, "requiresOpenaiAuth": true }
""");
if (CodexAccountStatus.Parse(signedOutAccount.RootElement).CanReadWeeklyQuota)
{
    throw new InvalidOperationException("A signed-out account must require login.");
}

using var chatGptAccount = JsonDocument.Parse("""
{ "account": { "type": "chatgpt" }, "requiresOpenaiAuth": true }
""");
if (!CodexAccountStatus.Parse(chatGptAccount.RootElement).CanReadWeeklyQuota)
{
    throw new InvalidOperationException("A ChatGPT account must be allowed to read weekly quota.");
}

using var loginStartResponse = JsonDocument.Parse("""
{ "loginId": "login-123", "authUrl": "https://chatgpt.com/auth/codex" }
""");
if (CodexLoginStart.Parse(loginStartResponse.RootElement).AuthorizationUrl.Scheme != Uri.UriSchemeHttps)
{
    throw new InvalidOperationException("Login must only open the HTTPS authorization URL from Codex.");
}

if (!TrayStatus.ShouldOfferLogin(null, loginRequired: false))
{
    throw new InvalidOperationException("A missing weekly quota must still offer a Codex login action.");
}

if (TrayStatus.ShouldOfferLogin(weekly, loginRequired: false))
{
    throw new InvalidOperationException("A weekly quota must not show a redundant login action.");
}

if (QuotaSnapshot.FormatTimeRemaining(new TimeSpan(days: 3, hours: 4, minutes: 5, seconds: 0)) != "3일 4시간 남음")
{
    throw new InvalidOperationException("The reset countdown must show whole days and hours.");
}
