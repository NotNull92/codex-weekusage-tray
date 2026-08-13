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
        using var brush = new SolidBrush(color);
        using var font = new Font(FontFamily.GenericSansSerif, 16f, FontStyle.Bold, GraphicsUnit.Pixel);
        using var format = new StringFormat
        {
            Alignment = StringAlignment.Center,
            LineAlignment = StringAlignment.Center,
        };
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        graphics.FillEllipse(brush, 2, 2, 28, 28);
        graphics.DrawString("W", font, Brushes.Black, new RectangleF(0, 0, 32, 32), format);

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
