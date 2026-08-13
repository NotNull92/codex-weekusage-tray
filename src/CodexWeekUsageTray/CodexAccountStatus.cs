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

public sealed record CodexLoginStart(Uri AuthorizationUrl)
{
    public static CodexLoginStart Parse(JsonElement result)
    {
        if (result.TryGetProperty("authUrl", out var authUrl)
            && Uri.TryCreate(authUrl.GetString(), UriKind.Absolute, out var authorizationUrl)
            && authorizationUrl.Scheme == Uri.UriSchemeHttps)
        {
            return new CodexLoginStart(authorizationUrl);
        }

        throw new InvalidOperationException("Codex did not return a secure browser login URL.");
    }
}
