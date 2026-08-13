namespace CodexWeekUsageTray;

internal static class CodexTheme
{
    public static readonly Color Surface = Color.FromArgb(8, 10, 23);
    public static readonly Color SurfaceRaised = Color.FromArgb(16, 19, 54);
    public static readonly Color Text = Color.FromArgb(248, 248, 255);
    public static readonly Color TextMuted = Color.FromArgb(185, 193, 233);
    public static readonly Color TextDim = Color.FromArgb(126, 134, 179);
    public static readonly Color GlowStart = Color.FromArgb(149, 162, 255);
    public static readonly Color GlowMiddle = Color.FromArgb(86, 106, 255);
    public static readonly Color GlowEnd = Color.FromArgb(74, 89, 254);
    public static readonly Color Error = Color.FromArgb(255, 154, 168);

    public static Font Mono(float size, FontStyle style = FontStyle.Regular) =>
        new("Cascadia Mono", size, style, GraphicsUnit.Pixel);
}
