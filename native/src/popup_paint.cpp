#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "popup_paint.h"

#include <array>
#include <ctime>

namespace {

int Scaled(HWND window, int pixels) {
    return MulDiv(pixels, static_cast<int>(GetDpiForWindow(window)), 96);
}

void DrawPopupText(HDC dc, std::wstring_view text, const RECT& rect, int height, COLORREF color, bool bold, UINT format) {
    const HFONT font = CreateFontW(-height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    const HGDIOBJ old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT target = rect;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &target, format | DT_NOPREFIX);
    SelectObject(dc, old_font);
    DeleteObject(font);
}

std::wstring FormatReset(std::int64_t reset_unix_seconds) {
    const std::time_t reset = static_cast<std::time_t>(reset_unix_seconds);
    std::tm local{};
    if (localtime_s(&local, &reset) != 0) {
        return L"Unknown";
    }
    wchar_t text[64]{};
    return wcsftime(text, std::size(text), L"%b %d, %I:%M %p", &local) == 0 ? L"Unknown" : text;
}

std::wstring FormatTimeLeft(std::int64_t reset_unix_seconds) {
    const auto seconds = reset_unix_seconds - static_cast<std::int64_t>(std::time(nullptr));
    if (seconds <= 0) {
        return L"Resetting";
    }
    return std::to_wstring(seconds / 86400) + L"d " + std::to_wstring((seconds / 3600) % 24) + L"h";
}

}

void PaintNativePopup(HWND window, const CodexTray::AppState& state) {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    const HBRUSH background = CreateSolidBrush(RGB(10, 12, 29));
    FillRect(dc, &client, background);
    DeleteObject(background);

    TRIVERTEX top[] = {{0, 0, 0x1800, 0x1600, 0x5000, 0xff00}, {client.right, client.bottom / 2, 0x0800, 0x1100, 0x3500, 0xff00}};
    GRADIENT_RECT top_rect{0, 1};
    GradientFill(dc, top, 2, &top_rect, 1, GRADIENT_FILL_RECT_V);
    TRIVERTEX bottom[] = {{0, client.bottom / 2, 0x0700, 0x1200, 0x3100, 0xff00}, {client.right, client.bottom, 0x0d00, 0x0d00, 0x2100, 0xff00}};
    GRADIENT_RECT bottom_rect{0, 1};
    GradientFill(dc, bottom, 2, &bottom_rect, 1, GRADIENT_FILL_RECT_V);

    const int inset = Scaled(window, 16);
    HPEN outer = CreatePen(PS_SOLID, 1, RGB(109, 122, 255));
    HGDIOBJ old_pen = SelectObject(dc, outer);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, inset, inset, client.right - inset, client.bottom - inset);
    SelectObject(dc, old_pen);
    DeleteObject(outer);
    HPEN inner = CreatePen(PS_SOLID, 1, RGB(51, 69, 178));
    old_pen = SelectObject(dc, inner);
    Rectangle(dc, inset + Scaled(window, 5), inset + Scaled(window, 5), client.right - inset - Scaled(window, 5), client.bottom - inset - Scaled(window, 5));
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(inner);

    const auto model = CodexTray::BuildPopupModel(state.quota, state.login_required, state.login_in_progress, state.safe_error);
    DrawPopupText(dc, L"CODEX", {Scaled(window, 28), Scaled(window, 30), client.right - Scaled(window, 28), Scaled(window, 54)}, Scaled(window, 17), RGB(180, 190, 255), true, DT_LEFT | DT_SINGLELINE);
    DrawPopupText(dc, L"WEEKLY LIMIT", {Scaled(window, 28), Scaled(window, 56), client.right - Scaled(window, 28), Scaled(window, 79)}, Scaled(window, 13), RGB(132, 145, 220), true, DT_LEFT | DT_SINGLELINE);
    DrawPopupText(dc, model.metric, {Scaled(window, 28), Scaled(window, 90), client.right - Scaled(window, 28), Scaled(window, 123)}, Scaled(window, 25), RGB(165, 177, 255), true, DT_LEFT | DT_SINGLELINE);
    DrawPopupText(dc, model.detail_one, {Scaled(window, 28), Scaled(window, 132), client.right - Scaled(window, 28), Scaled(window, 156)}, Scaled(window, 16), RGB(238, 241, 255), false, DT_LEFT | DT_SINGLELINE);
    DrawPopupText(dc, model.detail_two, {Scaled(window, 28), Scaled(window, 157), client.right - Scaled(window, 28), Scaled(window, 181)}, Scaled(window, 16), RGB(194, 203, 245), false, DT_LEFT | DT_SINGLELINE);
    if (state.quota) {
        DrawPopupText(dc, L"Used: " + std::to_wstring(state.quota->used_percent) + L"%", {Scaled(window, 28), Scaled(window, 180), client.right - Scaled(window, 28), Scaled(window, 198)}, Scaled(window, 12), RGB(180, 190, 255), false, DT_LEFT | DT_SINGLELINE);
        DrawPopupText(dc, L"Resets: " + FormatReset(state.quota->resets_at_unix_seconds), {Scaled(window, 28), Scaled(window, 198), client.right - Scaled(window, 28), Scaled(window, 216)}, Scaled(window, 12), RGB(180, 190, 255), false, DT_LEFT | DT_SINGLELINE);
        DrawPopupText(dc, L"Time left: " + FormatTimeLeft(state.quota->resets_at_unix_seconds), {Scaled(window, 28), Scaled(window, 216), client.right - Scaled(window, 28), Scaled(window, 230)}, Scaled(window, 12), RGB(180, 190, 255), false, DT_LEFT | DT_SINGLELINE);
    }
    EndPaint(window, &paint);
}
