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

if (QuotaSnapshot.FormatTimeRemaining(new TimeSpan(days: 3, hours: 4, minutes: 5, seconds: 0)) != "3일 4시간 남음")
{
    throw new InvalidOperationException("The reset countdown must show whole days and hours.");
}
