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
            && authorizationUrl.Scheme == Uri.UriSchemeHttps)
        {
            return new CodexLoginStart(loginId, authorizationUrl);
        }

        throw new InvalidOperationException("Codex did not return a secure browser login URL.");
    }
}

public sealed record CodexLoginCompleted(string? LoginId, bool Succeeded, string? Error)
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
        var error = parameters.TryGetProperty("error", out var errorProperty)
            && errorProperty.ValueKind == JsonValueKind.String
                ? errorProperty.GetString()
                : null;
        return new CodexLoginCompleted(loginId, succeeded, error);
    }
}
