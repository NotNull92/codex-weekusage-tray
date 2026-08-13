namespace CodexWeekUsageTray;

public sealed class UsagePopupForm : Form
{
    private readonly Func<Task> _refresh;
    private readonly Func<Task> _login;
    private readonly Label _remaining = new();
    private readonly Label _used = new();
    private readonly Label _reset = new();
    private readonly Label _countdown = new();
    private readonly Button _refreshButton = new();
    private readonly Button _loginButton = new();
    private readonly System.Windows.Forms.Timer _countdownTimer = new() { Interval = 1_000 };
    private QuotaSnapshot? _snapshot;
    private string? _error;

    public UsagePopupForm(Func<Task> refresh, Func<Task> login)
    {
        _refresh = refresh;
        _login = login;
        AutoScaleMode = AutoScaleMode.Dpi;
        BackColor = Color.FromArgb(36, 36, 36);
        ClientSize = new Size(244, 154);
        FormBorderStyle = FormBorderStyle.None;
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
        _loginButton.Location = new Point(14, 120);
        _loginButton.Size = new Size(126, 24);
        _loginButton.Text = "로그인";
        _loginButton.ForeColor = SystemColors.ControlText;
        _loginButton.UseVisualStyleBackColor = true;
        _loginButton.Click += LoginButtonClicked;
        _refreshButton.Location = new Point(150, 120);
        _refreshButton.Size = new Size(80, 24);
        _refreshButton.Text = "새로 고침";
        _refreshButton.ForeColor = SystemColors.ControlText;
        _refreshButton.UseVisualStyleBackColor = true;
        _refreshButton.Click += RefreshButtonClicked;

        Controls.AddRange([_remaining, _used, _reset, _countdown, _loginButton, _refreshButton]);
        _countdownTimer.Tick += (_, _) => UpdateCountdown();
    }

    public void ShowFor(QuotaSnapshot? snapshot, Point anchor, bool loginRequired, bool loginInProgress, string? error)
    {
        _snapshot = snapshot;
        _error = error;
        UpdateText(loginRequired, loginInProgress);

        var screen = Screen.FromPoint(anchor).WorkingArea;
        var x = Math.Clamp(anchor.X - Width, screen.Left, screen.Right - Width);
        var y = anchor.Y - Height - 8;
        Location = new Point(x, y >= screen.Top ? y : Math.Min(anchor.Y + 8, screen.Bottom - Height));

        Show();
        BringToFront();
        _countdownTimer.Start();
    }

    public void UpdateSnapshot(QuotaSnapshot? snapshot, bool loginRequired, bool loginInProgress, string? error)
    {
        _snapshot = snapshot;
        _error = error;
        UpdateText(loginRequired, loginInProgress);
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

    private async void LoginButtonClicked(object? sender, EventArgs e)
    {
        _loginButton.Enabled = false;
        try
        {
            await _login();
        }
        finally
        {
            _loginButton.Enabled = true;
        }
    }

    private void UpdateText(bool loginRequired, bool loginInProgress)
    {
        var shouldOfferLogin = TrayStatus.ShouldOfferLogin(_snapshot, loginRequired);
        _loginButton.Visible = shouldOfferLogin;
        _loginButton.Text = loginInProgress ? "로그인 다시 시작" : "로그인";
        _refreshButton.Text = shouldOfferLogin ? "다시 확인" : "새로 고침";

        if (shouldOfferLogin)
        {
            _remaining.Text = "Codex 로그인이 필요합니다";
            _used.Text = _error ?? "ChatGPT 계정으로 로그인하세요.";
            _reset.Text = "로그인 후 7일 제한을 표시합니다.";
            _countdown.Text = string.Empty;
            return;
        }

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
