using System.Text.Json;
using System.Globalization;

namespace CodexWeekUsageTray;

public sealed record QuotaSnapshot(int UsedPercent, DateTimeOffset ResetsAt)
{
    public int RemainingPercent => 100 - UsedPercent;

    public static string FormatTimeRemaining(TimeSpan remaining) =>
        remaining <= TimeSpan.Zero
            ? "Resetting"
            : $"{remaining.Days}d {remaining.Hours}h";

    public static string FormatResetTime(DateTimeOffset resetsAt) =>
        resetsAt.ToLocalTime().ToString("MMM d, h:mm tt", CultureInfo.InvariantCulture);

    public static void RunSelfCheck()
    {
        using var response = JsonDocument.Parse("""
        { "rateLimits": { "primary": { "usedPercent": 27, "windowDurationMins": 10080, "resetsAt": 1781395200 } } }
        """);
        var snapshot = ParseWeekly(response.RootElement)
            ?? throw new InvalidOperationException("Seven-day quota window was not selected.");
        if (snapshot.RemainingPercent != 73)
        {
            throw new InvalidOperationException("Seven-day remaining percentage is incorrect.");
        }
    }

    public static QuotaSnapshot? ParseWeekly(JsonElement result)
    {
        foreach (var window in EnumerateWindows(result))
        {
            if (!window.TryGetProperty("windowDurationMins", out var duration)
                || duration.GetInt32() != 10_080
                || !window.TryGetProperty("usedPercent", out var usedPercent)
                || !window.TryGetProperty("resetsAt", out var resetsAt))
            {
                continue;
            }

            return new QuotaSnapshot(
                Math.Clamp(usedPercent.GetInt32(), 0, 100),
                DateTimeOffset.FromUnixTimeSeconds(resetsAt.GetInt64()));
        }

        return null;
    }

    private static IEnumerable<JsonElement> EnumerateWindows(JsonElement result)
    {
        if (result.TryGetProperty("rateLimits", out var rateLimits))
        {
            foreach (var window in EnumerateBucketWindows(rateLimits))
            {
                yield return window;
            }
        }

        if (!result.TryGetProperty("rateLimitsByLimitId", out var byLimitId)
            || byLimitId.ValueKind != JsonValueKind.Object)
        {
            yield break;
        }

        foreach (var limit in byLimitId.EnumerateObject())
        {
            foreach (var window in EnumerateBucketWindows(limit.Value))
            {
                yield return window;
            }
        }
    }

    private static IEnumerable<JsonElement> EnumerateBucketWindows(JsonElement bucket)
    {
        foreach (var name in new[] { "primary", "secondary" })
        {
            if (bucket.TryGetProperty(name, out var window)
                && window.ValueKind == JsonValueKind.Object)
            {
                yield return window;
            }
        }
    }
}
