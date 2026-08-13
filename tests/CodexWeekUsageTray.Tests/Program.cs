using System.Text.Json;
using System.Globalization;
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
var loginStart = CodexLoginStart.Parse(loginStartResponse.RootElement);
if (loginStart.AuthorizationUrl.Scheme != Uri.UriSchemeHttps)
{
    throw new InvalidOperationException("Login must only open the HTTPS authorization URL from Codex.");
}

if (loginStart.LoginId != "login-123")
{
    throw new InvalidOperationException("A managed ChatGPT login must retain Codex's login ID for cancellation and completion.");
}

using var openAiLoginStartResponse = JsonDocument.Parse("""
{ "loginId": "login-456", "authUrl": "https://auth.openai.com/codex/device" }
""");
if (CodexLoginStart.Parse(openAiLoginStartResponse.RootElement).AuthorizationUrl.Host != "auth.openai.com")
{
    throw new InvalidOperationException("An official OpenAI login URL must be accepted.");
}

AssertRejectedLoginUrl("https://login.example.test/auth/codex");
AssertRejectedLoginUrl("https://chatgpt.com.evil.test/auth/codex");

using var loginCompletedResponse = JsonDocument.Parse("""
{ "loginId": "login-123", "success": false, "error": "cancelled" }
""");
var loginCompleted = CodexLoginCompleted.Parse(loginCompletedResponse.RootElement);
if (loginCompleted.LoginId != "login-123" || loginCompleted.Succeeded)
{
    throw new InvalidOperationException("A managed ChatGPT login completion must retain its login ID and success state.");
}

var codexLocatorRoot = Path.Combine(Path.GetTempPath(), $"codex-locator-test-{Guid.NewGuid():N}");
try
{
    var officialCodexDirectory = Path.Combine(codexLocatorRoot, "Programs", "OpenAI", "Codex", "bin");
    Directory.CreateDirectory(officialCodexDirectory);
    var officialCodexPath = Path.Combine(officialCodexDirectory, "codex.exe");
    File.WriteAllText(officialCodexPath, string.Empty);
    var resolvedCodexPath = CodexUsageClient.ResolveExecutablePath(codexLocatorRoot);
    if (!string.Equals(resolvedCodexPath, officialCodexPath, StringComparison.OrdinalIgnoreCase))
    {
        throw new InvalidOperationException("The official Codex install path must take priority over PATH entries.");
    }

    File.Delete(officialCodexPath);
    try
    {
        _ = CodexUsageClient.ResolveExecutablePath(codexLocatorRoot);
        throw new InvalidOperationException("A missing Codex CLI must not be resolved from PATH or the current directory.");
    }
    catch (InvalidOperationException exception) when (exception.Message == "The official Codex CLI could not be found in its standard install location.")
    {
    }
}
finally
{
    if (Directory.Exists(codexLocatorRoot))
    {
        Directory.Delete(codexLocatorRoot, recursive: true);
    }
}

using var startupTimeout = new CancellationTokenSource(TimeSpan.FromSeconds(2));
try
{
    await using var client = await CodexUsageClient.StartAsync(startupTimeout.Token);
}
catch (OperationCanceledException exception)
{
    throw new InvalidOperationException("Codex App Server initialization must not wait indefinitely for its first JSON response.", exception);
}

if (!TrayStatus.ShouldOfferLogin(null, loginRequired: false))
{
    throw new InvalidOperationException("A missing weekly quota must still offer a Codex login action.");
}

if (TrayStatus.ShouldOfferLogin(weekly, loginRequired: false))
{
    throw new InvalidOperationException("A weekly quota must not show a redundant login action.");
}

if (TrayStatus.DisplayLabel(null) != "--")
{
    throw new InvalidOperationException("An unavailable quota must render as two dashes without a W prefix.");
}

if (TrayStatus.DisplayLabel(weekly) != "73")
{
    throw new InvalidOperationException("A weekly quota must render only its remaining percentage.");
}

if (QuotaSnapshot.FormatTimeRemaining(new TimeSpan(days: 3, hours: 4, minutes: 5, seconds: 0)) != "3d 4h")
{
    throw new InvalidOperationException("The time-left value must not repeat the word left.");
}

var originalCulture = CultureInfo.CurrentCulture;
try
{
    CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("ko-KR");
    var resetTime = QuotaSnapshot.FormatResetTime(new DateTimeOffset(2026, 8, 16, 12, 30, 0, TimeSpan.Zero));
    if (!resetTime.Contains("Aug", StringComparison.Ordinal) || resetTime.Contains('\uC6D4') || resetTime.Contains("\uC624\uD6C4", StringComparison.Ordinal))
    {
        throw new InvalidOperationException("The reset time must stay in simple English regardless of the Windows language.");
    }
}
finally
{
    CultureInfo.CurrentCulture = originalCulture;
}

using var weeklyTrayIcon = TrayIconRenderer.Create(weekly);
using var renderedWeeklyIcon = weeklyTrayIcon.ToBitmap();
var weeklyInkBounds = GetInkBounds(renderedWeeklyIcon);
if (weeklyInkBounds.Left < 2 || weeklyInkBounds.Right > 30)
{
    throw new InvalidOperationException("The 73 tray icon must keep both digits inside the 32px canvas instead of clipping to 7.");
}

static Rectangle GetInkBounds(Bitmap bitmap)
{
    var minX = bitmap.Width;
    var minY = bitmap.Height;
    var maxX = -1;
    var maxY = -1;
    for (var y = 0; y < bitmap.Height; y++)
    {
        for (var x = 0; x < bitmap.Width; x++)
        {
            if (!IsInk(bitmap.GetPixel(x, y)))
            {
                continue;
            }

            minX = Math.Min(minX, x);
            minY = Math.Min(minY, y);
            maxX = Math.Max(maxX, x);
            maxY = Math.Max(maxY, y);
        }
    }

    if (maxX < 0)
    {
        throw new InvalidOperationException("The tray icon must draw visible Codex-blue text.");
    }

    return Rectangle.FromLTRB(minX, minY, maxX + 1, maxY + 1);
}

static bool IsInk(Color pixel) => pixel.A > 0 && pixel.B > pixel.R && pixel.B > pixel.G;

static void AssertRejectedLoginUrl(string authUrl)
{
    using var response = JsonDocument.Parse($$"""
    { "loginId": "login-456", "authUrl": "{{authUrl}}" }
    """);
    try
    {
        _ = CodexLoginStart.Parse(response.RootElement);
    }
    catch (InvalidOperationException)
    {
        return;
    }

    throw new InvalidOperationException("A non-OpenAI HTTPS login URL must not be opened in the browser.");
}
