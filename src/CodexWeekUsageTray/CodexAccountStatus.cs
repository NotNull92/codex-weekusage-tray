using System.Text.Json;

namespace CodexWeekUsageTray;

public sealed record CodexAccountStatus(bool CanReadWeeklyQuota)
{
    public static CodexAccountStatus Parse(JsonElement result)
    {
        var canReadWeeklyQuota = result.TryGetProperty("account", out var account)
            && account.ValueKind == JsonValueKind.Object
            && account.TryGetProperty("type", out var type)
            && string.Equals(type.GetString(), "chatgpt", StringComparison.OrdinalIgnoreCase);

        return new CodexAccountStatus(canReadWeeklyQuota);
    }
}

public sealed record CodexLoginStart(string LoginId, Uri AuthorizationUrl)
{
    public static CodexLoginStart Parse(JsonElement result)
    {
        if (result.TryGetProperty("loginId", out var loginIdProperty)
            && loginIdProperty.GetString() is { Length: > 0 } loginId
            && result.TryGetProperty("authUrl", out var authUrl)
            && Uri.TryCreate(authUrl.GetString(), UriKind.Absolute, out var authorizationUrl)
            && authorizationUrl.Scheme == Uri.UriSchemeHttps
            && IsOfficialLoginHost(authorizationUrl.Host))
        {
            return new CodexLoginStart(loginId, authorizationUrl);
        }

        throw new InvalidOperationException("Codex did not return a secure browser login URL.");
    }

    private static bool IsOfficialLoginHost(string host) =>
        string.Equals(host, "chatgpt.com", StringComparison.OrdinalIgnoreCase)
        || string.Equals(host, "auth.openai.com", StringComparison.OrdinalIgnoreCase);
}

public sealed record CodexLoginCompleted(string? LoginId, bool Succeeded)
{
    public static CodexLoginCompleted Parse(JsonElement parameters)
    {
        var loginId = parameters.TryGetProperty("loginId", out var loginIdProperty)
            && loginIdProperty.ValueKind == JsonValueKind.String
                ? loginIdProperty.GetString()
                : null;
        var succeeded = parameters.TryGetProperty("success", out var success)
            && success.ValueKind is JsonValueKind.True or JsonValueKind.False
            && success.GetBoolean();
        return new CodexLoginCompleted(loginId, succeeded);
    }
}
