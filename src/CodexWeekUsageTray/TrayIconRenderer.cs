using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;

namespace CodexWeekUsageTray;

internal static class TrayIconRenderer
{
    public static Icon Create(QuotaSnapshot? snapshot, bool loginRequired, bool hasError)
    {
        using var bitmap = new Bitmap(32, 32);
        using var graphics = Graphics.FromImage(bitmap);
        var label = TrayStatus.DisplayLabel(snapshot);
        var fontSize = label.Length switch
        {
            1 => 27f,
            2 => 28f,
            _ => 17f,
        };
        using var font = new Font("Segoe UI", fontSize, FontStyle.Bold, GraphicsUnit.Pixel);
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        TextRenderer.DrawText(
            graphics,
            label,
            font,
            new Rectangle(0, 0, 32, 32),
            Color.FromArgb(16, 163, 127),
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
