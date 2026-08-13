namespace CodexWeekUsageTray;

public sealed class UsagePopupForm : Form
{
    private readonly Func<Task> _refresh;
    private readonly Label _remaining = new();
    private readonly Label _used = new();
    private readonly Label _reset = new();
    private readonly Label _countdown = new();
    private readonly Button _refreshButton = new();
    private readonly System.Windows.Forms.Timer _countdownTimer = new() { Interval = 1_000 };
    private QuotaSnapshot? _snapshot;
    private string? _error;

    public UsagePopupForm(Func<Task> refresh)
    {
        _refresh = refresh;
        AutoScaleMode = AutoScaleMode.Dpi;
        BackColor = Color.FromArgb(36, 36, 36);
        ClientSize = new Size(244, 132);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        ForeColor = Color.White;
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = false;
        StartPosition = FormStartPosition.Manual;
        TopMost = true;

        ConfigureLabel(_remaining, new Point(14, 12), new Size(216, 22), FontStyle.Bold);
        ConfigureLabel(_used, new Point(14, 38), new Size(216, 20), FontStyle.Regular);
        ConfigureLabel(_reset, new Point(14, 62), new Size(216, 20), FontStyle.Regular);
        ConfigureLabel(_countdown, new Point(14, 86), new Size(216, 20), FontStyle.Regular);
        _refreshButton.Location = new Point(166, 102);
        _refreshButton.Size = new Size(64, 24);
        _refreshButton.Text = "새로 고침";
        _refreshButton.UseVisualStyleBackColor = true;
        _refreshButton.Click += RefreshButtonClicked;

        Controls.AddRange([_remaining, _used, _reset, _countdown, _refreshButton]);
        _countdownTimer.Tick += (_, _) => UpdateCountdown();
    }

    public void ShowFor(QuotaSnapshot? snapshot, Point overlayLocation, Size overlaySize, string? error)
    {
        _snapshot = snapshot;
        _error = error;
        UpdateText();

        var screen = Screen.FromPoint(overlayLocation).WorkingArea;
        var x = Math.Clamp(overlayLocation.X + overlaySize.Width - Width, screen.Left, screen.Right - Width);
        var y = overlayLocation.Y - Height - 6;
        Location = new Point(x, y >= screen.Top ? y : overlayLocation.Y + overlaySize.Height + 6);

        Show();
        BringToFront();
        _countdownTimer.Start();
    }

    public void UpdateSnapshot(QuotaSnapshot? snapshot, string? error)
    {
        _snapshot = snapshot;
        _error = error;
        UpdateText();
    }

    protected override void OnDeactivate(EventArgs e)
    {
        base.OnDeactivate(e);
        Hide();
        _countdownTimer.Stop();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _countdownTimer.Dispose();
        }

        base.Dispose(disposing);
    }

    private static void ConfigureLabel(Label label, Point location, Size size, FontStyle style)
    {
        label.Font = new Font(FontFamily.GenericSansSerif, 9f, style);
        label.ForeColor = Color.White;
        label.Location = location;
        label.Size = size;
    }

    private async void RefreshButtonClicked(object? sender, EventArgs e)
    {
        _refreshButton.Enabled = false;
        try
        {
            await _refresh();
        }
        finally
        {
            _refreshButton.Enabled = true;
        }
    }

    private void UpdateText()
    {
        if (_snapshot is null)
        {
            _remaining.Text = "7일 제한 정보를 찾지 못했습니다";
            _used.Text = _error ?? "Codex 로그인과 제한 정보를 확인하세요.";
            _reset.Text = string.Empty;
            _countdown.Text = string.Empty;
            return;
        }

        _remaining.Text = $"7일 잔여 {_snapshot.RemainingPercent}%";
        _used.Text = $"사용 {_snapshot.UsedPercent}%";
        _reset.Text = $"초기화: {_snapshot.ResetsAt.ToLocalTime():yyyy-MM-dd HH:mm}";
        UpdateCountdown();
    }

    private void UpdateCountdown()
    {
        _countdown.Text = _snapshot is null
            ? string.Empty
            : QuotaSnapshot.FormatTimeRemaining(_snapshot.ResetsAt - DateTimeOffset.Now);
    }
}
