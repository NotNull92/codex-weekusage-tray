#pragma once

#include "core.h"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <windows.h>

namespace CodexTray {

enum class CodexEventKind {
    Ignore,
    Started,
    Account,
    RateLimits,
    RateLimitUpdated,
    LoginStarted,
    LoginCompleted,
    Error
};

struct CodexEvent {
    CodexEventKind kind{CodexEventKind::Ignore};
    std::string login_id;
    std::wstring authorization_url;
    std::optional<QuotaSnapshot> quota;
    bool success{};
};

constexpr UINT WM_APP_CODEX_EVENT = WM_APP + 41;

std::string BuildInitializeRequest(unsigned int id);
std::string BuildLoginRequest(unsigned int id);
CodexEvent ParseServerLine(std::string_view line);
std::optional<std::filesystem::path> InstalledCodexPath();

class CodexClient {
public:
    CodexClient() = default;
    ~CodexClient();

    bool Start(HWND receiver);
    void Stop();
    void RequestAccount();
    void RequestRateLimits();
    bool StartChatGptLogin();
    void CancelLogin(std::string_view login_id);

private:
    bool Write(std::string_view line);
    void ReadLoop();
    void Post(CodexEvent event);

    HANDLE process_{};
    HANDLE stdin_write_{};
    HANDLE stdout_read_{};
    HWND receiver_{};
    std::atomic_bool stopping_{};
    std::atomic_bool awaiting_initialization_{};
    std::atomic_uint next_id_{1};
    std::mutex write_mutex_;
    std::thread reader_;
};

}
