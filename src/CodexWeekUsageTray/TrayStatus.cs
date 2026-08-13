namespace CodexWeekUsageTray;

public static class TrayStatus
{
    public static bool ShouldOfferLogin(QuotaSnapshot? snapshot, bool loginRequired) =>
        loginRequired || snapshot is null;
}
