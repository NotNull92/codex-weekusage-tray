using System.Drawing.Drawing2D;

namespace CodexWeekUsageTray;

public sealed class UsagePopupForm : Form
{
    private readonly Func<Task> _refresh;
    private readonly Func<Task> _login;
    private readonly Label _product = new();
    private readonly Label _title = new();
    private readonly Label _remaining = new();
    private readonly Label _used = new();
    private readonly Label _reset = new();
    private readonly Label _countdown = new();
    private readonly Button _refreshButton = new();
    private readonly Button _loginButton = new();
    private readonly Button _closeButton = new();
    private readonly System.Windows.Forms.Timer _countdownTimer = new() { Interval = 1_000 };
    private QuotaSnapshot? _snapshot;
    private string? _error;

    public UsagePopupForm(Func<Task> refresh, Func<Task> login)
    {
        _refresh = refresh;
        _login = login;
        AutoScaleMode = AutoScaleMode.Dpi;
        BackColor = CodexTheme.Surface;
        ClientSize = new Size(368, 282);
        DoubleBuffered = true;
        FormBorderStyle = FormBorderStyle.None;
        ForeColor = CodexTheme.Text;
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = false;
        StartPosition = FormStartPosition.Manual;
        Text = "Codex weekly limit";
        TopMost = true;

        ConfigureLabel(_product, new Point(16, 16), new Size(336, 20), 13f, FontStyle.Bold, CodexTheme.GlowStart);
        _product.Text = "CODEX";
        ConfigureLabel(_title, new Point(16, 42), new Size(336, 18), 11f, FontStyle.Bold, CodexTheme.TextDim);
        _title.Text = "WEEKLY LIMIT";
        ConfigureLabel(_remaining, new Point(16, 68), new Size(336, 52), 32f, FontStyle.Bold, CodexTheme.Text);
        ConfigureLabel(_used, new Point(16, 132), new Size(336, 22), 11f, FontStyle.Regular, CodexTheme.TextMuted);
        ConfigureLabel(_reset, new Point(16, 156), new Size(336, 22), 11f, FontStyle.Regular, CodexTheme.TextMuted);
        ConfigureLabel(_countdown, new Point(16, 180), new Size(336, 22), 11f, FontStyle.Bold, CodexTheme.GlowStart);

        ConfigureButton(_loginButton, "Sign in", primary: true);
        _loginButton.Click += LoginButtonClicked;
        ConfigureButton(_refreshButton, "Refresh", primary: false);
        _refreshButton.Click += RefreshButtonClicked;
        ConfigureButton(_closeButton, "Close", primary: false);
        _closeButton.AccessibleName = "Close panel";
        _closeButton.Click += (_, _) => HidePanel();

        Controls.AddRange([
            _product,
            _title,
            _remaining,
            _used,
            _reset,
            _countdown,
            _loginButton,
            _refreshButton,
            _closeButton,
        ]);
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

    public void HidePanel()
    {
        Hide();
        _countdownTimer.Stop();
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

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        e.Graphics.Clear(CodexTheme.Surface);
        using var background = new LinearGradientBrush(
            ClientRectangle,
            CodexTheme.Surface,
            CodexTheme.SurfaceRaised,
            LinearGradientMode.ForwardDiagonal);
        e.Graphics.FillRectangle(background, ClientRectangle);
        DrawGlow(e.Graphics, new Rectangle(182, -72, 240, 184), CodexTheme.GlowStart);
        DrawGlow(e.Graphics, new Rectangle(-92, 136, 220, 172), CodexTheme.GlowEnd);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        using var outerGlow = new Pen(Color.FromArgb(72, CodexTheme.GlowMiddle), 3f);
        using var frame = new Pen(Color.FromArgb(220, CodexTheme.GlowStart));
        e.Graphics.DrawRectangle(outerGlow, 2, 2, Width - 5, Height - 5);
        e.Graphics.DrawRectangle(frame, 5, 5, Width - 11, Height - 11);
        using var corner = new Pen(CodexTheme.GlowEnd, 2f);
        e.Graphics.DrawLine(corner, 6, 24, 6, 6);
        e.Graphics.DrawLine(corner, 6, 6, 24, 6);
        e.Graphics.DrawLine(corner, Width - 25, Height - 6, Width - 7, Height - 6);
        e.Graphics.DrawLine(corner, Width - 7, Height - 24, Width - 7, Height - 6);
    }

    private static void ConfigureLabel(Label label, Point location, Size size, float fontSize, FontStyle style, Color foreColor)
    {
        label.AutoEllipsis = true;
        label.BackColor = Color.Transparent;
        label.Font = CodexTheme.Mono(fontSize, style);
        label.ForeColor = foreColor;
        label.Location = location;
        label.Size = size;
    }

    private static void ConfigureButton(Button button, string text, bool primary)
    {
        button.BackColor = primary ? CodexTheme.GlowEnd : CodexTheme.SurfaceRaised;
        button.Cursor = Cursors.Hand;
        button.FlatAppearance.BorderColor = primary ? CodexTheme.GlowStart : CodexTheme.GlowMiddle;
        button.FlatAppearance.BorderSize = 1;
        button.FlatAppearance.MouseDownBackColor = primary ? CodexTheme.GlowMiddle : CodexTheme.Surface;
        button.FlatAppearance.MouseOverBackColor = primary ? CodexTheme.GlowMiddle : Color.FromArgb(42, 49, 111);
        button.FlatStyle = FlatStyle.Flat;
        button.Font = CodexTheme.Mono(11f, FontStyle.Bold);
        button.ForeColor = CodexTheme.Text;
        button.Size = new Size(100, 36);
        button.Text = text;
        button.UseVisualStyleBackColor = false;
    }

    private static void DrawGlow(Graphics graphics, Rectangle bounds, Color color)
    {
        using var path = new GraphicsPath();
        path.AddEllipse(bounds);
        using var brush = new PathGradientBrush(path)
        {
            CenterColor = Color.FromArgb(58, color),
            SurroundColors = [Color.FromArgb(0, color)],
        };
        graphics.FillPath(brush, path);
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
        _loginButton.Text = loginInProgress ? "Start again" : "Sign in";
        _refreshButton.Text = shouldOfferLogin ? "Check" : "Refresh";
        LayoutActions(shouldOfferLogin);

        _used.ForeColor = _error is null ? CodexTheme.TextMuted : CodexTheme.Error;
        if (shouldOfferLogin)
        {
            _remaining.Text = "SIGN IN TO CODEX";
            _remaining.ForeColor = CodexTheme.GlowStart;
            _used.Text = _error ?? "Open your browser to sign in.";
            _reset.Text = "Then see your weekly limit here.";
            _countdown.Text = string.Empty;
            return;
        }

        if (_snapshot is null)
        {
            _remaining.Text = "NO WEEKLY LIMIT";
            _remaining.ForeColor = CodexTheme.GlowStart;
            _used.Text = _error ?? "Sign in to Codex or check again.";
            _reset.Text = string.Empty;
            _countdown.Text = string.Empty;
            return;
        }

        _remaining.Text = $"{_snapshot.RemainingPercent}% LEFT";
        _remaining.ForeColor = CodexTheme.GlowStart;
        _used.Text = $"Used: {_snapshot.UsedPercent}%";
        _reset.Text = $"Resets: {QuotaSnapshot.FormatResetTime(_snapshot.ResetsAt)}";
        UpdateCountdown();
    }

    private void LayoutActions(bool shouldOfferLogin)
    {
        if (shouldOfferLogin)
        {
            _loginButton.Location = new Point(16, 230);
            _loginButton.Size = new Size(110, 36);
            _refreshButton.Location = new Point(134, 230);
            _refreshButton.Size = new Size(96, 36);
            _closeButton.Location = new Point(238, 230);
            _closeButton.Size = new Size(114, 36);
            return;
        }

        _refreshButton.Location = new Point(16, 230);
        _refreshButton.Size = new Size(214, 36);
        _closeButton.Location = new Point(238, 230);
        _closeButton.Size = new Size(114, 36);
    }

    private void UpdateCountdown()
    {
        _countdown.Text = _snapshot is null
            ? string.Empty
            : $"Time left: {QuotaSnapshot.FormatTimeRemaining(_snapshot.ResetsAt - DateTimeOffset.Now)}";
    }
}
