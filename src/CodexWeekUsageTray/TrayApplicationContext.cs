using System.Diagnostics;

namespace CodexWeekUsageTray;

public sealed class TrayApplicationContext : ApplicationContext
{
    private readonly ContextMenuStrip _menu = new();
    private readonly ToolStripMenuItem _loginMenu;
    private readonly NotifyIcon _notifyIcon;
    private readonly UsagePopupForm _popup;
    private CodexUsageClient? _client;
    private Task<bool>? _clientStartup;
    private Icon? _icon;
    private QuotaSnapshot? _snapshot;
    private string? _error;
    private bool _loginRequired;
    private bool _loginInProgress;

    public TrayApplicationContext()
    {
        _popup = new UsagePopupForm(RefreshAsync, StartLoginAsync);
        _popup.CreateControl();
        _menu.Items.Add("상태 보기", null, (_, _) => TogglePopup());
        _menu.Items.Add("새로 고침", null, async (_, _) => await RefreshAsync());
        _loginMenu = new ToolStripMenuItem("Codex 로그인", null, async (_, _) => await StartLoginAsync());
        _menu.Items.Add(_loginMenu);
        _menu.Items.Add(new ToolStripSeparator());
        _menu.Items.Add("종료", null, (_, _) => ExitThread());

        _notifyIcon = new NotifyIcon
        {
            ContextMenuStrip = _menu,
            Visible = true,
        };
        _notifyIcon.MouseUp += NotifyIconMouseUp;
        UpdateStatus(null, false, null);
        Application.Idle += StartOnIdle;
    }

    protected override void ExitThreadCore()
    {
        Application.Idle -= StartOnIdle;
        _notifyIcon.MouseUp -= NotifyIconMouseUp;
        _notifyIcon.Visible = false;
        _notifyIcon.Dispose();
        _icon?.Dispose();
        _popup.Dispose();

        if (_client is not null)
        {
            _client.WeeklyQuotaChanged -= WeeklyQuotaChanged;
            _client.LoginCompleted -= LoginCompleted;
            _ = _client.DisposeAsync();
        }

        base.ExitThreadCore();
    }

    private async void StartOnIdle(object? sender, EventArgs e)
    {
        Application.Idle -= StartOnIdle;
        await RefreshAsync();
    }

    private async Task RefreshAsync()
    {
        if (!await EnsureClientAsync() || _client is not { } client)
        {
            return;
        }

        try
        {
            var account = await client.ReadAccountAsync(CancellationToken.None);
            if (!account.CanReadWeeklyQuota)
            {
                UpdateStatus(null, true, null);
                return;
            }

            var snapshot = await client.RefreshAsync(CancellationToken.None);
            UpdateStatus(snapshot, snapshot is null, null);
        }
        catch
        {
            UpdateStatus(_snapshot, true, "Codex 사용량을 가져올 수 없습니다.");
        }
    }

    private async Task StartLoginAsync()
    {
        if (_loginInProgress)
        {
            return;
        }

        if (!await EnsureClientAsync() || _client is not { } client)
        {
            return;
        }

        _loginInProgress = true;
        UpdateStatus(_snapshot, true, "브라우저에서 로그인을 시작합니다.");
        try
        {
            var authorizationUrl = await client.StartChatGptLoginAsync(CancellationToken.None);
            Process.Start(new ProcessStartInfo(authorizationUrl.AbsoluteUri) { UseShellExecute = true });
            UpdateStatus(_snapshot, true, "브라우저에서 로그인을 완료하세요.");
        }
        catch
        {
            _loginInProgress = false;
            UpdateStatus(_snapshot, true, "로그인을 시작할 수 없습니다.");
        }
    }

    private void WeeklyQuotaChanged(object? sender, QuotaSnapshot? snapshot) =>
        RunOnUi(() => UpdateStatus(snapshot, false, null));

    private async Task<bool> EnsureClientAsync()
    {
        if (_client is not null)
        {
            return true;
        }

        _clientStartup ??= StartClientAsync();
        var startup = _clientStartup;
        var started = await startup;
        if (!started && ReferenceEquals(_clientStartup, startup))
        {
            _clientStartup = null;
        }

        return started;
    }

    private async Task<bool> StartClientAsync()
    {
        try
        {
            _client = await CodexUsageClient.StartAsync(CancellationToken.None);
            _client.WeeklyQuotaChanged += WeeklyQuotaChanged;
            _client.LoginCompleted += LoginCompleted;
            return true;
        }
        catch
        {
            UpdateStatus(null, true, "Codex CLI를 실행할 수 없습니다.");
            return false;
        }
    }

    private void LoginCompleted(bool succeeded) =>
        RunOnUi(async () =>
        {
            _loginInProgress = false;
            if (succeeded)
            {
                await RefreshAsync();
            }
            else
            {
                UpdateStatus(_snapshot, true, "로그인이 완료되지 않았습니다.");
            }
        });

    private void NotifyIconMouseUp(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            TogglePopup();
        }
    }

    private void TogglePopup()
    {
        if (_popup.Visible)
        {
            _popup.Hide();
            return;
        }

        _popup.ShowFor(_snapshot, Cursor.Position, _loginRequired, _error);
    }

    private void UpdateStatus(QuotaSnapshot? snapshot, bool loginRequired, string? error)
    {
        _snapshot = snapshot;
        _loginRequired = TrayStatus.ShouldOfferLogin(snapshot, loginRequired);
        _error = error;
        _popup.UpdateSnapshot(snapshot, _loginRequired, error);
        _loginMenu.Enabled = !_loginInProgress;

        var nextIcon = TrayIconRenderer.Create(snapshot, _loginRequired, error is not null && !_loginRequired);
        _notifyIcon.Icon = nextIcon;
        _icon?.Dispose();
        _icon = nextIcon;
        _notifyIcon.Text = snapshot is not null
            ? $"W {snapshot.RemainingPercent}% · 7일 잔여"
            : _loginRequired
                ? "W -- · Codex 로그인 필요"
                : "W -- · Codex 사용량 확인 중";
    }

    private void RunOnUi(Action action)
    {
        if (!_popup.IsDisposed && _popup.IsHandleCreated)
        {
            _popup.BeginInvoke(action);
        }
    }
}
