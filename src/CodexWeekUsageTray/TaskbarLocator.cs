using System.Runtime.InteropServices;

namespace CodexWeekUsageTray;

public static class TaskbarLocator
{
    private static readonly IntPtr Topmost = new(-1);
    private const uint NoMove = 0x0002;
    private const uint NoSize = 0x0001;
    private const uint NoActivate = 0x0010;

    public static Rectangle GetPrimaryTaskbarBounds()
    {
        var handle = FindWindow("Shell_TrayWnd", null);
        return handle == IntPtr.Zero || !GetWindowRect(handle, out var rect)
            ? Rectangle.Empty
            : Rectangle.FromLTRB(rect.Left, rect.Top, rect.Right, rect.Bottom);
    }

    public static void BringAboveTaskbar(IntPtr handle)
    {
        if (handle != IntPtr.Zero)
        {
            SetWindowPos(handle, Topmost, 0, 0, 0, 0, NoMove | NoSize | NoActivate);
        }
    }

    [DllImport("user32.dll", EntryPoint = "FindWindowW", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern IntPtr FindWindow(string className, string? windowName);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetWindowRect(IntPtr handle, out NativeRect rect);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(
        IntPtr handle,
        IntPtr insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
