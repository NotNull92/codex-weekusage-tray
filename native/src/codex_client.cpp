#define WIN32_LEAN_AND_MEAN

#include "codex_client.h"

#include "codex_protocol.h"

#include <array>
#include <filesystem>
#include <shlobj.h>
#include <utility>

namespace {

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length == 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length) == 0) {
        return {};
    }

    return result;
}

bool IsExistingFile(const std::filesystem::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

CodexTray::CodexEvent Event(CodexTray::CodexEventKind kind) {
    CodexTray::CodexEvent event;
    event.kind = kind;
    return event;
}

}

namespace CodexTray {

CodexEvent ParseServerLine(std::string_view line) {
    if (const auto method = JsonStringField(line, "method")) {
        const auto parameters = JsonObjectField(line, "params");
        if (!parameters) {
            return {};
        }

        if (*method == "account/rateLimits/updated") {
            CodexEvent event = Event(CodexEventKind::RateLimitUpdated);
            event.quota = ParseWeeklyQuota(*parameters);
            return event;
        }

        if (*method == "account/login/completed") {
            CodexEvent event;
            event.kind = CodexEventKind::LoginCompleted;
            event.login_id = JsonStringField(*parameters, "loginId").value_or("");
            event.success = JsonBooleanField(*parameters, "success").value_or(false);
            return event;
        }

        return {};
    }

    if (JsonObjectField(line, "error")) {
        return Event(CodexEventKind::Error);
    }

    const auto result = JsonObjectField(line, "result");
    if (!result) {
        return {};
    }

    if (JsonObjectField(*result, "rateLimits") || JsonObjectField(*result, "rateLimitsByLimitId")) {
        CodexEvent event = Event(CodexEventKind::RateLimits);
        event.quota = ParseWeeklyQuota(*result);
        return event;
    }

    if (const auto account = JsonObjectField(*result, "account")) {
        CodexEvent event;
        event.kind = CodexEventKind::Account;
        event.success = JsonStringField(*account, "type").value_or("") == "chatgpt";
        return event;
    }

    const auto login_id = JsonStringField(*result, "loginId");
    const auto authorization_url = JsonStringField(*result, "authUrl");
    if (!login_id || !authorization_url) {
        return Event(CodexEventKind::Started);
    }

    const auto url = Utf8ToWide(*authorization_url);
    if (url.empty() || !IsOfficialLoginUrl(url)) {
        return Event(CodexEventKind::Error);
    }

    CodexEvent event = Event(CodexEventKind::LoginStarted);
    event.login_id = *login_id;
    event.authorization_url = url;
    return event;
}

std::optional<std::filesystem::path> InstalledCodexPath() {
    PWSTR local_app_data{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local_app_data))) {
        return std::nullopt;
    }
    const std::filesystem::path executable = StandardCodexPath(local_app_data);
    CoTaskMemFree(local_app_data);
    return executable;
}

CodexClient::~CodexClient() {
    Stop();
}

bool CodexClient::Start(HWND receiver) {
    if (process_ != nullptr) {
        return true;
    }

    const auto executable = InstalledCodexPath();
    if (!executable || !IsExistingFile(*executable)) {
        return false;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE child_stdin_read{};
    HANDLE child_stdout_write{};
    HANDLE null_stderr{};
    if (!CreatePipe(&child_stdin_read, &stdin_write_, &attributes, 0)
        || !SetHandleInformation(stdin_write_, HANDLE_FLAG_INHERIT, 0)
        || !CreatePipe(&stdout_read_, &child_stdout_write, &attributes, 0)
        || !SetHandleInformation(stdout_read_, HANDLE_FLAG_INHERIT, 0)
        || (null_stderr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) == INVALID_HANDLE_VALUE) {
        if (child_stdin_read != nullptr) {
            CloseHandle(child_stdin_read);
        }
        if (child_stdout_write != nullptr) {
            CloseHandle(child_stdout_write);
        }
        Stop();
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_stdin_read;
    startup.hStdOutput = child_stdout_write;
    startup.hStdError = null_stderr;

    PROCESS_INFORMATION process_information{};
    std::wstring command = L"\"" + executable->wstring() + L"\" app-server --stdio";
    awaiting_initialization_ = true;
    const BOOL created = CreateProcessW(executable->c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process_information);
    CloseHandle(child_stdin_read);
    CloseHandle(child_stdout_write);
    CloseHandle(null_stderr);
    if (!created) {
        Stop();
        return false;
    }

    CloseHandle(process_information.hThread);
    process_ = process_information.hProcess;
    receiver_ = receiver;
    stopping_ = false;
    reader_ = std::thread(&CodexClient::ReadLoop, this);
    if (!Write(BuildInitializeRequest(next_id_++))) {
        Stop();
        return false;
    }
    return true;
}

void CodexClient::Stop() {
    stopping_ = true;
    awaiting_initialization_ = false;

    {
        std::lock_guard lock(write_mutex_);
        if (stdin_write_ != nullptr) {
            CloseHandle(stdin_write_);
            stdin_write_ = nullptr;
        }
    }
    if (reader_.joinable()) CancelSynchronousIo(reinterpret_cast<HANDLE>(reader_.native_handle()));
    if (stdout_read_ != nullptr) CancelIoEx(stdout_read_, nullptr);
    if (process_ != nullptr && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) TerminateProcess(process_, 0);
    if (reader_.joinable()) reader_.join();
    if (stdout_read_ != nullptr) { CloseHandle(stdout_read_); stdout_read_ = nullptr; }
    if (process_ != nullptr) { WaitForSingleObject(process_, 2000); CloseHandle(process_); process_ = nullptr; }
    std::lock_guard events_lock(events_mutex_);
    events_.clear();
    receiver_ = nullptr;
}

bool CodexClient::RequestAccount() {
    return Write(BuildAccountRequest(next_id_++));
}

bool CodexClient::RequestRateLimits() {
    return Write(BuildRateLimitsRequest(next_id_++));
}

bool CodexClient::StartChatGptLogin() {
    return Write(BuildLoginRequest(next_id_++));
}

void CodexClient::CancelLogin(std::string_view login_id) {
    Write(BuildCancelLoginRequest(next_id_++, login_id));
}

bool CodexClient::Write(std::string_view line) {
    std::lock_guard lock(write_mutex_);
    if (stdin_write_ == nullptr || line.size() > static_cast<size_t>(MAXDWORD - 1)) {
        return false;
    }

    std::string message(line);
    message.push_back('\n');
    DWORD written{};
    return WriteFile(stdin_write_, message.data(), static_cast<DWORD>(message.size()), &written, nullptr) && written == message.size();
}

void CodexClient::ReadLoop() {
    const HANDLE stdout_read = stdout_read_;
    std::array<char, 4096> bytes{};
    std::string pending;
    while (!stopping_) {
        DWORD read{};
        if (!ReadFile(stdout_read, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read == 0) {
            if (!stopping_) {
                Post(Event(CodexEventKind::Disconnected));
            }
            return;
        }

        pending.append(bytes.data(), read);
        size_t newline{};
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.size() > 1024 * 1024) {
                Post(Event(CodexEventKind::Disconnected));
                return;
            }
            CodexEvent event = ParseServerLine(line);
            if (event.kind == CodexEventKind::Started && awaiting_initialization_.exchange(false)) {
                if (!Write(BuildInitializedNotification()) || !Write(BuildAccountRequest(next_id_++))) {
                    event = Event(CodexEventKind::Error);
                }
            }
            Post(std::move(event));
        }

        if (pending.size() > 1024 * 1024) {
            Post(Event(CodexEventKind::Disconnected));
            return;
        }
    }
}

void CodexClient::Post(CodexEvent event) {
    if (event.kind == CodexEventKind::Ignore || receiver_ == nullptr) {
        return;
    }

    std::lock_guard lock(events_mutex_);
    events_.push_back(std::move(event));
    PostMessageW(receiver_, WM_APP_CODEX_EVENT, 0, 0);
}

std::vector<CodexEvent> CodexClient::TakeEvents() {
    std::lock_guard lock(events_mutex_);
    return std::exchange(events_, {});
}

}
