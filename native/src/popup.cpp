#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "popup.h"

#include "popup_paint.h"

#include <algorithm>

namespace {

constexpr UINT TIMER_POPUP = 2;
constexpr wchar_t kPopupClass[] = L"CodexWeekUsageTrayNativePopup";

int Scaled(HWND window, int pixels) {
    return MulDiv(pixels, static_cast<int>(GetDpiForWindow(window)), 96);
}

void DrawButtonText(HDC dc, std::wstring_view text, const RECT& rect, int height) {
    const HFONT font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const HGDIOBJ old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 247, 255));
    RECT target = rect;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &target, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old_font);
    DeleteObject(font);
}

}

bool PopupWindow::Create(HINSTANCE instance, HWND owner, void* context, PopupCommandHandler command_handler) {
    instance_ = instance;
    context_ = context;
    command_handler_ = command_handler;
    WNDCLASSEXW popup_class{};
    popup_class.cbSize = sizeof(popup_class);
    popup_class.hInstance = instance_;
    popup_class.lpfnWndProc = WindowProc;
    popup_class.lpszClassName = kPopupClass;
    popup_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (RegisterClassExW(&popup_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kPopupClass, L"Codex Weekly Limit", WS_POPUP, 0, 0, 368, 300, owner, nullptr, instance_, this);
    if (window_ == nullptr) {
        return false;
    }
    sign_in_button_ = CreateWindowExW(0, L"BUTTON", L"Sign in", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, window_, reinterpret_cast<HMENU>(ID_POPUP_SIGN_IN), instance_, nullptr);
    check_button_ = CreateWindowExW(0, L"BUTTON", L"Check", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, window_, reinterpret_cast<HMENU>(ID_POPUP_CHECK), instance_, nullptr);
    refresh_button_ = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, window_, reinterpret_cast<HMENU>(ID_POPUP_REFRESH), instance_, nullptr);
    close_button_ = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | BS_OWNERDRAW, 0, 0, 0, 0, window_, reinterpret_cast<HMENU>(ID_POPUP_CLOSE), instance_, nullptr);
    return true;
}

void PopupWindow::Destroy() {
    if (window_ != nullptr) {
        KillTimer(window_, TIMER_POPUP);
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

void PopupWindow::Hide() {
    if (window_ != nullptr) {
        KillTimer(window_, TIMER_POPUP);
        SetWindowPos(window_, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(window_, SW_HIDE);
    }
}

bool PopupWindow::IsVisible() const {
    return window_ != nullptr && IsWindowVisible(window_);
}

void PopupWindow::Show(const CodexTray::AppState& state) {
    if (window_ == nullptr) {
        return;
    }
    state_ = state;
    const int width = Scaled(window_, 368);
    const int height = Scaled(window_, 300);
    POINT point{};
    GetCursorPos(&point);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
    const int x = std::clamp(static_cast<int>(point.x) - width / 2, static_cast<int>(monitor.rcWork.left), static_cast<int>(monitor.rcWork.right) - width);
    int y = point.y - height - Scaled(window_, 8);
    if (y < monitor.rcWork.top) {
        y = point.y + Scaled(window_, 8);
    }
    y = std::clamp(y, static_cast<int>(monitor.rcWork.top), static_cast<int>(monitor.rcWork.bottom) - height);
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    UpdateControls();
    SetTimer(window_, TIMER_POPUP, 1000, nullptr);
}

void PopupWindow::Update(const CodexTray::AppState& state) {
    state_ = state;
    if (window_ != nullptr) {
        UpdateControls();
    }
}

LRESULT CALLBACK PopupWindow::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* popup = reinterpret_cast<PopupWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* created = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        popup = static_cast<PopupWindow*>(created->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(popup));
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return popup ? popup->HandleMessage(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT PopupWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT) {
        PaintNativePopup(window_, state_);
        return 0;
    }
    if (message == WM_DRAWITEM) {
        DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    }
    if (message == WM_COMMAND) {
        command_handler_(context_, LOWORD(wparam));
        return 0;
    }
    if (message == WM_TIMER && wparam == TIMER_POPUP) {
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    }
    if (message == WM_CLOSE) {
        Hide();
        return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

void PopupWindow::UpdateControls() {
    const auto model = CodexTray::BuildPopupModel(state_.quota, state_.login_required, state_.login_in_progress, state_.safe_error);
    const auto has_action = [&](CodexTray::PopupAction action) {
        return std::find(model.actions.begin(), model.actions.end(), action) != model.actions.end();
    };
    const int x = Scaled(window_, 16);
    const int y = Scaled(window_, 248);
    const int height = Scaled(window_, 36);
    MoveWindow(sign_in_button_, x, y, Scaled(window_, 110), height, TRUE);
    MoveWindow(check_button_, Scaled(window_, 134), y, Scaled(window_, 96), height, TRUE);
    MoveWindow(refresh_button_, x, y, Scaled(window_, 214), height, TRUE);
    MoveWindow(close_button_, Scaled(window_, 238), y, Scaled(window_, 114), height, TRUE);
    SetWindowTextW(sign_in_button_, state_.login_in_progress ? L"Sign in again" : L"Sign in");
    ShowWindow(sign_in_button_, has_action(CodexTray::PopupAction::SignIn) ? SW_SHOW : SW_HIDE);
    ShowWindow(check_button_, has_action(CodexTray::PopupAction::Check) ? SW_SHOW : SW_HIDE);
    ShowWindow(refresh_button_, has_action(CodexTray::PopupAction::Refresh) ? SW_SHOW : SW_HIDE);
    ShowWindow(close_button_, SW_SHOW);
    InvalidateRect(window_, nullptr, FALSE);
}

void PopupWindow::DrawButton(const DRAWITEMSTRUCT& draw) const {
    const HBRUSH fill = CreateSolidBrush((draw.itemState & ODS_SELECTED) != 0 ? RGB(48, 56, 132) : RGB(25, 30, 70));
    FillRect(draw.hDC, &draw.rcItem, fill);
    DeleteObject(fill);
    const HPEN border = CreatePen(PS_SOLID, 1, RGB(109, 122, 255));
    const HGDIOBJ old_pen = SelectObject(draw.hDC, border);
    const HGDIOBJ old_brush = SelectObject(draw.hDC, GetStockObject(HOLLOW_BRUSH));
    Rectangle(draw.hDC, draw.rcItem.left, draw.rcItem.top, draw.rcItem.right, draw.rcItem.bottom);
    SelectObject(draw.hDC, old_pen);
    SelectObject(draw.hDC, old_brush);
    DeleteObject(border);
    wchar_t text[64]{};
    GetWindowTextW(draw.hwndItem, text, static_cast<int>(std::size(text)));
    DrawButtonText(draw.hDC, text, draw.rcItem, Scaled(window_, 15));
    if ((draw.itemState & ODS_FOCUS) != 0) {
        RECT focus = draw.rcItem;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(draw.hDC, &focus);
    }
}
