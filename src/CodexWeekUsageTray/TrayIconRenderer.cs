using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;

namespace CodexWeekUsageTray;

internal static class TrayIconRenderer
{
    public static Icon Create(QuotaSnapshot? snapshot, bool loginRequired, bool hasError)
    {
        var color = loginRequired
            ? Color.FromArgb(244, 185, 66)
            : hasError
                ? Color.FromArgb(241, 113, 113)
                : snapshot?.RemainingPercent switch
                {
                    >= 50 => Color.FromArgb(123, 215, 138),
                    >= 20 => Color.FromArgb(244, 185, 66),
                    < 20 => Color.FromArgb(241, 113, 113),
                    _ => Color.FromArgb(210, 210, 210),
                };

        using var bitmap = new Bitmap(32, 32);
        using var graphics = Graphics.FromImage(bitmap);
        var label = snapshot is null ? "W --" : $"W{snapshot.RemainingPercent}";
        using var font = new Font("Segoe UI", label.Length > 3 ? 12f : 14f, FontStyle.Bold, GraphicsUnit.Pixel);
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        TextRenderer.DrawText(
            graphics,
            label,
            font,
            new Rectangle(0, 0, 32, 32),
            color,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);

        var handle = bitmap.GetHicon();
        try
        {
            using var icon = Icon.FromHandle(handle);
            return (Icon)icon.Clone();
        }
        finally
        {
            DestroyIcon(handle);
        }
    }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(IntPtr handle);
}
