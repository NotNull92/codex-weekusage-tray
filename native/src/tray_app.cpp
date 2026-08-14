#define WIN32_LEAN_AND_MEAN

#include "tray_app.h"

#include "main.h"
#include "popup.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <shellapi.h>
#include <string>

namespace {

constexpr UINT WM_APP_TRAY = WM_APP + 40;
constexpr UINT ID_SHOW_PANEL = 1;
constexpr UINT ID_REFRESH = 2;
constexpr UINT ID_SIGN_IN = 3;
constexpr UINT ID_QUIT = 4;
constexpr UINT TIMER_REFRESH = 1;
constexpr wchar_t kOwnerClass[] = L"CodexWeekUsageTrayNativeOwner";

class NativeApp {
public:
    explicit NativeApp(HINSTANCE instance) : instance_(instance) {}

    int Run(LaunchMode mode, std::optional<int> preview_remaining_percent) {
        WNDCLASSEXW owner_class{};
        owner_class.cbSize = sizeof(owner_class);
        owner_class.hInstance = instance_;
        owner_class.lpfnWndProc = OwnerProc;
        owner_class.lpszClassName = kOwnerClass;
        if (RegisterClassExW(&owner_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 1;
        }
        owner_ = CreateWindowExW(WS_EX_TOOLWINDOW, kOwnerClass, L"Codex WeekUsage Tray", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance_, this);
        if (owner_ == nullptr || !popup_.Create(instance_, owner_, this, PopupCommand)) return 1;
        preview_mode_ = mode != LaunchMode::Normal;
        if (mode == LaunchMode::PreviewPending) {
            state_.login_required = true;
        } else if (mode == LaunchMode::PreviewReady) {
            const int remaining = std::clamp(preview_remaining_percent.value_or(73), 0, 100);
            state_.quota = CodexTray::QuotaSnapshot{100 - remaining, remaining, static_cast<std::int64_t>(std::time(nullptr)) + 3 * 86400 + 4 * 3600};
        } else {
            state_.login_required = true;
        }
        UpdateIcon();
        AddTrayIcon();
        if (preview_mode_) ShowPopup();
        else {
            EnsureClient();
            SetTimer(owner_, TIMER_REFRESH, 5000, nullptr);
        }
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* app = reinterpret_cast<NativeApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* created = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            app = static_cast<NativeApp*>(created->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            return DefWindowProcW(window, message, wparam, lparam);
        }
        return app ? app->HandleOwnerMessage(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
    }

    static void PopupCommand(void* context, UINT command) { static_cast<NativeApp*>(context)->HandleCommand(command); }

    LRESULT HandleOwnerMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_APP_TRAY) {
            if (lparam == WM_LBUTTONUP || lparam == NIN_SELECT || lparam == NIN_KEYSELECT) {
                popup_.IsVisible() ? popup_.Hide() : ShowPopup();
            } else if (lparam == WM_RBUTTONUP) {
                ShowMenu();
            }
            return 0;
        }
        if (message == CodexTray::WM_APP_CODEX_EVENT) {
            for (const auto& event : client_.TakeEvents()) HandleCodexEvent(event);
            return 0;
        }
        if (message == WM_TIMER && wparam == TIMER_REFRESH) { RefreshNow(); return 0; }
        if (message == WM_COMMAND) {
            HandleCommand(LOWORD(wparam));
            return 0;
        }
        if (message == WM_DESTROY) {
            KillTimer(owner_, TIMER_REFRESH);
            client_.Stop();
            popup_.Destroy();
            if (tray_added_) {
                Shell_NotifyIconW(NIM_DELETE, &tray_);
            }
            if (icon_ != nullptr) {
                DestroyIcon(icon_);
            }
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(owner_, message, wparam, lparam);
    }

    bool EnsureClient() {
        if (preview_mode_ || client_running_) {
            return client_running_ || preview_mode_;
        }
        if (client_.Start(owner_)) {
            client_running_ = true;
            refresh_pending_ = true;
            return true;
        }
        state_.login_required = true;
        state_.safe_error = L"Install Codex, then try again.";
        popup_.Update(state_);
        return false;
    }

    void RefreshNow() {
        if (preview_mode_ || refresh_pending_ || !EnsureClient()) return;
        refresh_pending_ = true;
        if (!client_.RequestAccount()) HandleCodexEvent({CodexTray::CodexEventKind::Disconnected, {}, {}, std::nullopt, false});
    }

    void StartLogin() {
        if (preview_mode_ || login_start_pending_ || !EnsureClient()) return;
        if (!state_.pending_login_id.empty()) {
            client_.CancelLogin(state_.pending_login_id); state_.pending_login_id.clear();
        }
        state_.safe_error.clear();
        if (!client_.StartChatGptLogin()) { HandleCodexEvent({CodexTray::CodexEventKind::Disconnected, {}, {}, std::nullopt, false}); return; }
        login_start_pending_ = true;
        popup_.Update(state_);
    }

    void HandleCodexEvent(const CodexTray::CodexEvent& event) {
        if (event.kind == CodexTray::CodexEventKind::Disconnected) { client_.Stop(); client_running_ = false; }
        if (event.kind == CodexTray::CodexEventKind::LoginStarted || event.kind == CodexTray::CodexEventKind::Error || event.kind == CodexTray::CodexEventKind::Disconnected) login_start_pending_ = false;
        state_ = CodexTray::ApplyEvent(std::move(state_), event);
        if (event.kind == CodexTray::CodexEventKind::Account && event.success && !client_.RequestRateLimits()) { HandleCodexEvent({CodexTray::CodexEventKind::Disconnected, {}, {}, std::nullopt, false}); return; }
        if (state_.refresh_finished) refresh_pending_ = false;
        if (event.kind == CodexTray::CodexEventKind::LoginStarted && CodexTray::IsOfficialLoginUrl(event.authorization_url)) {
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(owner_, L"open", event.authorization_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
            if (result <= 32) {
                state_.login_in_progress = false;
                state_.safe_error = L"Could not open your browser.";
            }
        } else if (event.kind == CodexTray::CodexEventKind::LoginStarted) {
            state_.login_in_progress = false;
            state_.safe_error = L"Could not start sign in.";
        }
        if (state_.next_request == CodexTray::RequestKind::Account) {
            RefreshNow();
        }
        UpdateIcon();
        popup_.Update(state_);
    }

    void HandleCommand(UINT command) {
        switch (command) {
            case ID_SHOW_PANEL:
                ShowPopup();
                break;
            case ID_REFRESH:
            case ID_POPUP_CHECK:
            case ID_POPUP_REFRESH:
                RefreshNow();
                break;
            case ID_SIGN_IN:
            case ID_POPUP_SIGN_IN:
                StartLogin();
                break;
            case ID_POPUP_CLOSE:
                popup_.Hide();
                break;
            case ID_QUIT:
                DestroyWindow(owner_);
                break;
            default:
                break;
        }
    }

    void ShowPopup() {
        popup_.Show(state_);
    }

    void UpdateIcon() {
        const std::wstring label = CodexTray::DisplayLabel(state_.quota);
        if (label == last_tray_label_) {
            return;
        }
        HICON next_icon = CodexTray::CreateTrayIcon(state_.quota ? std::optional<int>(state_.quota->remaining_percent) : std::nullopt);
        if (next_icon == nullptr) {
            return;
        }
        const HICON old_icon = icon_;
        icon_ = next_icon;
        last_tray_label_ = label;
        lstrcpynW(tray_.szTip, label == L"--" ? L"Codex weekly limit" : (label + L"% left this week").c_str(), static_cast<int>(std::size(tray_.szTip)));
        if (tray_added_) {
            tray_.hIcon = icon_;
            tray_.uFlags = NIF_ICON | NIF_TIP;
            Shell_NotifyIconW(NIM_MODIFY, &tray_);
        }
        if (old_icon != nullptr) {
            DestroyIcon(old_icon);
        }
    }

    void AddTrayIcon() {
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = owner_;
        tray_.uID = 1;
        tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        tray_.uCallbackMessage = WM_APP_TRAY;
        tray_.hIcon = icon_;
        lstrcpynW(tray_.szTip, L"Codex weekly limit", static_cast<int>(std::size(tray_.szTip)));
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        if (tray_added_) {
            tray_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray_);
        }
    }

    void ShowMenu() {
        const HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, ID_SHOW_PANEL, L"Show panel");
        AppendMenuW(menu, MF_STRING, ID_REFRESH, L"Refresh");
        AppendMenuW(menu, MF_STRING, ID_SIGN_IN, L"Sign in to Codex");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit");
        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(owner_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, owner_, nullptr);
        DestroyMenu(menu);
    }

    HINSTANCE instance_{};
    HWND owner_{};
    NOTIFYICONDATAW tray_{};
    HICON icon_{};
    bool tray_added_{}, preview_mode_{}, client_running_{}, refresh_pending_{}, login_start_pending_{};
    std::wstring last_tray_label_;
    CodexTray::AppState state_;
    CodexTray::CodexClient client_;
    PopupWindow popup_;
};

}

int RunNativeApp(HINSTANCE instance, LaunchMode mode, std::optional<int> preview_remaining_percent) {
    return NativeApp(instance).Run(mode, preview_remaining_percent);
}
