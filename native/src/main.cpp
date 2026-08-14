#define WIN32_LEAN_AND_MEAN

#include "main.h"

#include "cleanup.h"
#include "tray_app.h"

#include <cwchar>
#include <shellapi.h>

namespace CodexTray {

PopupModel BuildPopupModel(const std::optional<QuotaSnapshot>& quota, bool login_required, bool login_in_progress, std::wstring_view safe_error) {
    if (login_required) {
        return {
            L"SIGN IN TO CODEX",
            login_in_progress ? L"Finish sign in in your browser." : L"Open your browser to sign in.",
            safe_error.empty() ? L"Then see your weekly limit here." : std::wstring(safe_error),
            {PopupAction::SignIn, PopupAction::Check, PopupAction::Close}
        };
    }
    if (quota) {
        return {
            std::to_wstring(quota->remaining_percent) + L"% LEFT",
            L"Your Codex weekly limit is ready.",
            safe_error.empty() ? L"Refresh to check again." : std::wstring(safe_error),
            {PopupAction::Refresh, PopupAction::Close}
        };
    }
    return {
        L"NO WEEKLY LIMIT",
        L"Check your Codex account again.",
        safe_error.empty() ? L"No weekly limit was found." : std::wstring(safe_error),
        {PopupAction::Check, PopupAction::Close}
    };
}

AppState ApplyEvent(AppState state, const CodexEvent& event) {
    state.next_request = RequestKind::None;
    state.refresh_finished = false;
    switch (event.kind) {
        case CodexEventKind::Account:
            state.login_in_progress = false;
            state.login_required = !event.success;
            state.safe_error.clear();
            if (event.success) {
                state.next_request = RequestKind::RateLimits;
            } else {
                state.quota.reset();
                state.refresh_finished = true;
            }
            break;
        case CodexEventKind::RateLimits:
        case CodexEventKind::RateLimitUpdated:
            state.quota = event.quota;
            state.login_required = false;
            state.safe_error.clear();
            state.refresh_finished = true;
            break;
        case CodexEventKind::LoginStarted:
            state.pending_login_id = event.login_id;
            state.login_in_progress = true;
            state.login_required = true;
            state.safe_error.clear();
            break;
        case CodexEventKind::LoginCompleted:
            if (state.pending_login_id.empty() || event.login_id != state.pending_login_id) {
                break;
            }
            state.pending_login_id.clear();
            state.login_in_progress = false;
            state.login_required = !event.success;
            if (event.success) {
                state.next_request = RequestKind::Account;
            } else {
                state.safe_error = L"Sign-in did not finish.";
            }
            break;
        case CodexEventKind::Error:
            state.login_in_progress = false;
            state.safe_error = L"Could not check Codex.";
            state.refresh_finished = true;
            break;
        case CodexEventKind::Ignore:
        case CodexEventKind::Started:
            break;
    }
    return state;
}

}

#ifndef CODEX_TRAY_TESTS
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argument_count{};
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    LaunchMode mode = LaunchMode::Normal;
    std::optional<int> preview_remaining_percent;
    bool self_test{};
    bool uninstall{};
    bool uninstall_dry_run{};
    for (int index = 1; arguments != nullptr && index < argument_count; ++index) {
        const std::wstring_view argument(arguments[index]);
        if (argument == L"--self-test") {
            self_test = true;
        } else if (argument == L"--preview-pending") {
            mode = LaunchMode::PreviewPending;
        } else if (argument.rfind(L"--preview-ready=", 0) == 0) {
            mode = LaunchMode::PreviewReady;
            preview_remaining_percent = static_cast<int>(wcstol(argument.data() + 16, nullptr, 10));
        } else if (argument == L"--uninstall") {
            uninstall = true;
        } else if (argument == L"--uninstall-dry-run") {
            uninstall_dry_run = true;
        }
    }
    if (arguments != nullptr) {
        LocalFree(arguments);
    }
    if (self_test) {
        const auto model = CodexTray::BuildPopupModel(CodexTray::QuotaSnapshot{27, 73, 1781395200}, false, false, L"");
        return model.metric == L"73% LEFT" && model.actions.size() == 2 ? 0 : 1;
    }
    if (uninstall || uninstall_dry_run) {
        return RunUninstall(uninstall_dry_run);
    }
    return RunNativeApp(instance, mode, preview_remaining_percent);
}
#endif
