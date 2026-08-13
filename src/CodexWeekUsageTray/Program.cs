namespace CodexWeekUsageTray;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        if (args.SequenceEqual(["--self-test"]))
        {
            QuotaSnapshot.RunSelfCheck();
            return;
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new TrayApplicationContext());
    }
}
