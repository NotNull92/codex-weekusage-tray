#pragma once

#include "codex_client.h"

#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace CodexTray {

struct InkBounds {
    int left;
    int top;
    int right;
    int bottom;
    int width;
    int height;
};

InkBounds MeasureTrayInk(std::optional<int> remaining_percent);
HICON CreateTrayIcon(std::optional<int> remaining_percent);

enum class PopupAction {
    SignIn,
    Check,
    Refresh,
    Close
};

struct PopupModel {
    std::wstring metric;
    std::wstring detail_one;
    std::wstring detail_two;
    std::vector<PopupAction> actions;
};

enum class RequestKind {
    None,
    Account,
    RateLimits
};

struct AppState {
    std::optional<QuotaSnapshot> quota;
    bool login_required{};
    bool login_in_progress{};
    std::string pending_login_id;
    std::wstring safe_error;
    RequestKind next_request{RequestKind::None};
    bool refresh_finished{};
};

PopupModel BuildPopupModel(const std::optional<QuotaSnapshot>& quota, bool login_required, bool login_in_progress, std::wstring_view safe_error);
AppState ApplyEvent(AppState state, const CodexEvent& event);

}
