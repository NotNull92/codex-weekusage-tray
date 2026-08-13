using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Runtime.InteropServices;

namespace CodexWeekUsageTray;

internal static class TrayIconRenderer
{
    public static Icon Create(QuotaSnapshot? snapshot)
    {
        using var bitmap = Render(snapshot);

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

    internal static Bitmap Render(QuotaSnapshot? snapshot)
    {
        var bitmap = new Bitmap(32, 32);
        using var graphics = Graphics.FromImage(bitmap);
        var label = TrayStatus.DisplayLabel(snapshot);
        using var font = CreateFittedFont(graphics, label);
        using var format = new StringFormat(StringFormat.GenericTypographic)
        {
            Alignment = StringAlignment.Center,
            LineAlignment = StringAlignment.Center,
            FormatFlags = StringFormatFlags.NoWrap,
        };
        using var brush = new LinearGradientBrush(
            new Rectangle(0, 0, 32, 32),
            CodexTheme.GlowStart,
            CodexTheme.GlowEnd,
            LinearGradientMode.Vertical);
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        graphics.Clear(Color.Transparent);
        graphics.DrawString(label, font, brush, new Rectangle(0, 0, 32, 32), format);
        return bitmap;
    }

    private static Font CreateFittedFont(Graphics graphics, string label)
    {
        var fontSize = label.Length switch
        {
            1 => 27f,
            2 => 24f,
            _ => 17f,
        };

        while (fontSize >= 8f)
        {
            var font = CodexTheme.Mono(fontSize, FontStyle.Bold);
            var measured = graphics.MeasureString(label, font, SizeF.Empty, StringFormat.GenericTypographic);
            if (measured.Width <= 30f && measured.Height <= 30f)
            {
                return font;
            }

            font.Dispose();
            fontSize -= 1f;
        }

        return CodexTheme.Mono(8f, FontStyle.Bold);
    }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(IntPtr handle);
}
