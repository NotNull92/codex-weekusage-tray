namespace CodexWeekUsageTray;

public sealed class TaskbarOverlayForm : Form
{
    private const int ToolWindow = 0x00000080;
    private const int NoActivate = 0x08000000;
    private readonly System.Windows.Forms.Timer _taskbarTimer = new() { Interval = 250 };
    private readonly UsagePopupForm _popup;
    private CodexUsageClient? _client;
    private QuotaSnapshot? _snapshot;
    private Rectangle _taskbarBounds;
    private string? _error;

    public TaskbarOverlayForm()
    {
        AutoScaleMode = AutoScaleMode.Dpi;
        BackColor = Color.FromArgb(32, 32, 32);
        ClientSize = new Size(82, 32);
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        StartPosition = FormStartPosition.Manual;
        TopMost = true;
        _popup = new UsagePopupForm(RefreshAsync);
        _taskbarTimer.Tick += (_, _) => PositionOnTaskbar();
        MouseUp += (_, _) => TogglePopup();
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var parameters = base.CreateParams;
            parameters.ExStyle |= ToolWindow | NoActivate;
            return parameters;
        }
    }

    protected override async void OnShown(EventArgs e)
    {
        base.OnShown(e);
        PositionOnTaskbar();
        _taskbarTimer.Start();

        try
        {
            _client = await CodexUsageClient.StartAsync(CancellationToken.None);
            _client.WeeklyQuotaChanged += WeeklyQuotaChanged;
            await RefreshAsync();
        }
        catch
        {
            _error = "Codex 사용량을 가져올 수 없습니다.";
            Invalidate();
        }
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _taskbarTimer.Stop();
        _popup.Dispose();
        if (_client is not null)
        {
            _client.WeeklyQuotaChanged -= WeeklyQuotaChanged;
            _client.DisposeAsync().AsTask().GetAwaiter().GetResult();
        }

        base.OnFormClosing(e);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        var remaining = _snapshot?.RemainingPercent;
        var color = remaining switch
        {
            >= 50 => Color.FromArgb(123, 215, 138),
            >= 20 => Color.FromArgb(244, 185, 66),
            < 20 => Color.FromArgb(241, 113, 113),
            _ => Color.FromArgb(210, 210, 210),
        };
        var text = remaining is null ? "W --" : $"W {remaining}%";
        using var font = new Font(FontFamily.GenericSansSerif, 9f, FontStyle.Bold);
        TextRenderer.DrawText(
            e.Graphics,
            text,
            font,
            ClientRectangle,
            color,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
    }

    private async Task RefreshAsync()
    {
        if (_client is null)
        {
            return;
        }

        try
        {
            UpdateSnapshot(await _client.RefreshAsync(CancellationToken.None), null);
        }
        catch
        {
            _error = "Codex 사용량을 가져올 수 없습니다.";
            _popup.UpdateSnapshot(_snapshot, _error);
            Invalidate();
        }
    }

    private void WeeklyQuotaChanged(object? sender, QuotaSnapshot? snapshot)
    {
        if (IsDisposed || !IsHandleCreated)
        {
            return;
        }

        BeginInvoke(() => UpdateSnapshot(snapshot, null));
    }

    private void UpdateSnapshot(QuotaSnapshot? snapshot, string? error)
    {
        _snapshot = snapshot;
        _error = error;
        _popup.UpdateSnapshot(snapshot, error);
        Invalidate();
    }

    private void TogglePopup()
    {
        if (_popup.Visible)
        {
            _popup.Hide();
            return;
        }

        _popup.ShowFor(_snapshot, Location, Size, _error);
    }

    private void PositionOnTaskbar()
    {
        var bounds = TaskbarLocator.GetPrimaryTaskbarBounds();
        if (bounds == Rectangle.Empty || bounds == _taskbarBounds)
        {
            return;
        }

        _taskbarBounds = bounds;
        Location = bounds.Width >= bounds.Height
            ? new Point(bounds.Right - Width - 8, bounds.Top + (bounds.Height - Height) / 2)
            : new Point(bounds.Left + (bounds.Width - Width) / 2, bounds.Bottom - Height - 8);
        TaskbarLocator.BringAboveTaskbar(Handle);
    }
}
