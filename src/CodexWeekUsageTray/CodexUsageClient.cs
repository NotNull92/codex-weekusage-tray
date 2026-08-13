using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace CodexWeekUsageTray;

public sealed class CodexUsageClient : IAsyncDisposable
{
    private readonly CancellationTokenSource _cancellation = new();
    private readonly ConcurrentDictionary<long, TaskCompletionSource<JsonElement>> _pending = new();
    private readonly SemaphoreSlim _writeLock = new(1, 1);
    private readonly Process _process;
    private readonly Task _readLoop;
    private readonly Task _errorDrain;
    private readonly Task _refreshLoop;
    private long _nextRequestId;

    private CodexUsageClient(Process process)
    {
        _process = process;
        _readLoop = ReadLoopAsync();
        _errorDrain = DrainErrorsAsync();
        _refreshLoop = RefreshLoopAsync();
    }

    public event EventHandler<QuotaSnapshot?>? WeeklyQuotaChanged;

    public static async Task<CodexUsageClient> StartAsync(CancellationToken cancellationToken)
    {
        var process = Process.Start(new ProcessStartInfo
        {
            FileName = "codex",
            Arguments = "app-server --stdio",
            CreateNoWindow = true,
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardInputEncoding = Encoding.UTF8,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        }) ?? throw new InvalidOperationException("Codex App Server could not be started.");

        var client = new CodexUsageClient(process);
        try
        {
            await client.SendRequestAsync(
                "initialize",
                new
                {
                    clientInfo = new
                    {
                        name = "codex-weekusage-tray",
                        title = "Codex WeekUsage Tray",
                        version = "1.0.0",
                    },
                },
                cancellationToken);
            await client.SendNotificationAsync("initialized", new { }, cancellationToken);
            return client;
        }
        catch
        {
            await client.DisposeAsync();
            throw;
        }
    }

    public async Task<QuotaSnapshot?> RefreshAsync(CancellationToken cancellationToken)
    {
        var result = await SendRequestAsync("account/rateLimits/read", new { }, cancellationToken);
        var snapshot = QuotaSnapshot.ParseWeekly(result);
        WeeklyQuotaChanged?.Invoke(this, snapshot);
        return snapshot;
    }

    public async ValueTask DisposeAsync()
    {
        _cancellation.Cancel();
        _process.StandardInput.Close();

        if (!_process.HasExited)
        {
            _process.Kill(entireProcessTree: true);
        }

        foreach (var pending in _pending.Values)
        {
            pending.TrySetException(new ObjectDisposedException(nameof(CodexUsageClient)));
        }

        try
        {
            await Task.WhenAll(_readLoop, _errorDrain, _refreshLoop);
        }
        catch (OperationCanceledException)
        {
        }
        finally
        {
            _process.Dispose();
            _writeLock.Dispose();
            _cancellation.Dispose();
        }
    }

    private async Task<JsonElement> SendRequestAsync(string method, object parameters, CancellationToken cancellationToken)
    {
        var id = Interlocked.Increment(ref _nextRequestId);
        var completion = new TaskCompletionSource<JsonElement>(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!_pending.TryAdd(id, completion))
        {
            throw new InvalidOperationException("Could not create a Codex request.");
        }

        try
        {
            await WriteAsync(new { method, id, @params = parameters }, cancellationToken);
            return await completion.Task.WaitAsync(cancellationToken);
        }
        finally
        {
            _pending.TryRemove(id, out _);
        }
    }

    private Task SendNotificationAsync(string method, object parameters, CancellationToken cancellationToken) =>
        WriteAsync(new { method, @params = parameters }, cancellationToken);

    private async Task WriteAsync(object message, CancellationToken cancellationToken)
    {
        await _writeLock.WaitAsync(cancellationToken);
        try
        {
            await _process.StandardInput.WriteLineAsync(JsonSerializer.Serialize(message));
            await _process.StandardInput.FlushAsync(cancellationToken);
        }
        finally
        {
            _writeLock.Release();
        }
    }

    private async Task ReadLoopAsync()
    {
        try
        {
            while (await _process.StandardOutput.ReadLineAsync(_cancellation.Token) is { } line)
            {
                using var message = JsonDocument.Parse(line);
                var root = message.RootElement;
                if (root.TryGetProperty("id", out var responseId)
                    && responseId.TryGetInt64(out var id)
                    && _pending.TryGetValue(id, out var completion))
                {
                    if (root.TryGetProperty("result", out var result))
                    {
                        completion.TrySetResult(result.Clone());
                    }
                    else
                    {
                        completion.TrySetException(new InvalidOperationException("Codex usage request failed."));
                    }

                    continue;
                }

                if (root.TryGetProperty("method", out var method)
                    && method.GetString() == "account/rateLimits/updated"
                    && root.TryGetProperty("params", out var parameters))
                {
                    WeeklyQuotaChanged?.Invoke(this, QuotaSnapshot.ParseWeekly(parameters));
                }
            }
        }
        catch (OperationCanceledException) when (_cancellation.IsCancellationRequested)
        {
        }
        finally
        {
            foreach (var pending in _pending.Values)
            {
                pending.TrySetException(new InvalidOperationException("Codex App Server stopped."));
            }
        }
    }

    private async Task DrainErrorsAsync()
    {
        try
        {
            while (await _process.StandardError.ReadLineAsync(_cancellation.Token) is not null)
            {
            }
        }
        catch (OperationCanceledException) when (_cancellation.IsCancellationRequested)
        {
        }
    }

    private async Task RefreshLoopAsync()
    {
        using var timer = new PeriodicTimer(TimeSpan.FromSeconds(5));
        try
        {
            while (await timer.WaitForNextTickAsync(_cancellation.Token))
            {
                try
                {
                    await RefreshAsync(_cancellation.Token);
                }
                catch (OperationCanceledException) when (_cancellation.IsCancellationRequested)
                {
                }
                catch
                {
                }
            }
        }
        catch (OperationCanceledException) when (_cancellation.IsCancellationRequested)
        {
        }
    }
}
