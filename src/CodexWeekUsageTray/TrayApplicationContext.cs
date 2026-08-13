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
    private bool _loginStarting;
    private string? _pendingLoginId;

    public TrayApplicationContext()
    {
        _popup = new UsagePopupForm(RefreshAsync, StartLoginAsync);
        _popup.CreateControl();
        _menu.Items.Add("Show panel", null, (_, _) => TogglePopup());
        _menu.Items.Add("Refresh", null, async (_, _) => await RefreshAsync());
        _loginMenu = new ToolStripMenuItem("Sign in to Codex", null, async (_, _) => await StartLoginAsync());
        _menu.Items.Add(_loginMenu);
        _menu.Items.Add(new ToolStripSeparator());
        _menu.Items.Add("Quit", null, (_, _) => ExitThread());

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
            UpdateStatus(_snapshot, true, "We could not get your limit.");
        }
    }

    private async Task StartLoginAsync()
    {
        if (_loginStarting)
        {
            return;
        }

        _loginStarting = true;
        try
        {
            if (!await EnsureClientAsync() || _client is not { } client)
            {
                return;
            }

            if (_pendingLoginId is not null)
            {
                await client.CancelChatGptLoginAsync(_pendingLoginId, CancellationToken.None);
            }

            _pendingLoginId = null;
            _loginInProgress = false;
            UpdateStatus(_snapshot, true, "Opening your browser...");
            var login = await client.StartChatGptLoginAsync(CancellationToken.None);
            _pendingLoginId = login.LoginId;
            _loginInProgress = true;
            using var browser = Process.Start(new ProcessStartInfo(login.AuthorizationUrl.AbsoluteUri) { UseShellExecute = true })
                ?? throw new InvalidOperationException("We could not open your browser.");
            UpdateStatus(_snapshot, true, "Finish sign-in in your browser.");
        }
        catch (Exception)
        {
            _loginInProgress = _pendingLoginId is not null;
            UpdateStatus(_snapshot, true, "We could not start sign-in.");
        }
        finally
        {
            _loginStarting = false;
            UpdateStatus(_snapshot, _loginRequired, _error);
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
        catch (Exception)
        {
            UpdateStatus(null, true, "Codex is not ready. Check that it is installed.");
            return false;
        }
    }

    private void LoginCompleted(CodexLoginCompleted completion) =>
        RunOnUi(async () =>
        {
            if (_pendingLoginId is null
                || !string.Equals(_pendingLoginId, completion.LoginId, StringComparison.Ordinal))
            {
                return;
            }

            _pendingLoginId = null;
            _loginInProgress = false;
            if (completion.Succeeded)
            {
                await RefreshAsync();
            }
            else
            {
                UpdateStatus(_snapshot, true, completion.Error is null
                    ? "Sign-in did not finish."
                    : "Sign-in did not finish.");
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
            _popup.HidePanel();
            return;
        }

        _popup.ShowFor(_snapshot, Cursor.Position, _loginRequired, _loginInProgress, _error);
    }

    private void UpdateStatus(QuotaSnapshot? snapshot, bool loginRequired, string? error)
    {
        _snapshot = snapshot;
        _loginRequired = TrayStatus.ShouldOfferLogin(snapshot, loginRequired);
        _error = error;
        _popup.UpdateSnapshot(snapshot, _loginRequired, _loginInProgress, error);
        _loginMenu.Enabled = !_loginStarting;
        _loginMenu.Text = _loginInProgress ? "Start sign-in again" : "Sign in to Codex";

        var nextIcon = TrayIconRenderer.Create(snapshot);
        _notifyIcon.Icon = nextIcon;
        _icon?.Dispose();
        _icon = nextIcon;
        _notifyIcon.Text = snapshot is not null
            ? $"{snapshot.RemainingPercent}% left this week"
            : _loginRequired
                ? "-- · Sign in to Codex"
                : "-- · Checking Codex";
    }

    private void RunOnUi(Action action)
    {
        if (!_popup.IsDisposed && _popup.IsHandleCreated)
        {
            _popup.BeginInvoke(action);
        }
    }
}
