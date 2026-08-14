#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "main.h"

#include <algorithm>
#include <array>
#include <shellapi.h>
#include <string>

namespace {

constexpr UINT WM_APP_TRAY = WM_APP + 40;
constexpr UINT ID_SHOW_PANEL = 1;
constexpr UINT ID_REFRESH = 2;
constexpr UINT ID_SIGN_IN = 3;
constexpr UINT ID_QUIT = 4;
constexpr wchar_t kOwnerClass[] = L"CodexWeekUsageTrayNativeOwner";

struct BitmapPixels {
    HBITMAP bitmap{};
    DWORD* pixels{};
};

std::wstring TrayLabel(const std::optional<int>& remaining_percent) {
    return remaining_percent ? std::to_wstring(std::clamp(*remaining_percent, 0, 100)) : L"--";
}

BitmapPixels RenderTrayBitmap(const std::optional<int>& remaining_percent) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 32;
    info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    BitmapPixels rendered;
    rendered.bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&rendered.pixels), nullptr, 0);
    if (rendered.bitmap == nullptr || rendered.pixels == nullptr) {
        return rendered;
    }

    std::fill(rendered.pixels, rendered.pixels + 32 * 32, 0U);
    const HDC dc = CreateCompatibleDC(nullptr);
    const HGDIOBJ previous = SelectObject(dc, rendered.bitmap);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(149, 162, 255));

    const std::wstring label = TrayLabel(remaining_percent);
    for (int height = 28; height >= 8; --height) {
        const HFONT font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        const HGDIOBJ old_font = SelectObject(dc, font);
        RECT measured{0, 0, 0, 0};
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &measured, DT_CALCRECT | DT_SINGLELINE);
        if (measured.right <= 30 && measured.bottom <= 30) {
            RECT target{0, 0, 32, 32};
            DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &target, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, old_font);
            DeleteObject(font);
            break;
        }
        SelectObject(dc, old_font);
        DeleteObject(font);
    }

    SelectObject(dc, previous);
    DeleteDC(dc);
    for (size_t index = 0; index < 32 * 32; ++index) {
        if ((rendered.pixels[index] & 0x00ffffffU) != 0) {
            rendered.pixels[index] |= 0xff000000U;
        }
    }
    return rendered;
}

class NativeApp {
public:
    explicit NativeApp(HINSTANCE instance) : instance_(instance) {}

    int Run() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.hInstance = instance_;
        window_class.lpfnWndProc = WindowProc;
        window_class.lpszClassName = kOwnerClass;
        if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 1;
        }

        owner_ = CreateWindowExW(WS_EX_TOOLWINDOW, kOwnerClass, L"Codex WeekUsage Tray", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance_, this);
        if (owner_ == nullptr) {
            return 1;
        }

        UpdateIcon(std::nullopt);
        AddTrayIcon();

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* app = reinterpret_cast<NativeApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* created = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            app = static_cast<NativeApp*>(created->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            return DefWindowProcW(window, message, wparam, lparam);
        }
        return app ? app->HandleMessage(message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_APP_TRAY) {
            if (lparam == WM_RBUTTONUP) {
                ShowMenu();
            }
            return 0;
        }
        if (message == WM_COMMAND) {
            if (LOWORD(wparam) == ID_QUIT) {
                DestroyWindow(owner_);
            }
            return 0;
        }
        if (message == WM_DESTROY) {
            if (tray_added_) {
                Shell_NotifyIconW(NIM_DELETE, &tray_);
                tray_added_ = false;
            }
            if (icon_ != nullptr) {
                DestroyIcon(icon_);
                icon_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(owner_, message, wparam, lparam);
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
    }

    void UpdateIcon(std::optional<int> remaining_percent) {
        HICON next_icon = CodexTray::CreateTrayIcon(remaining_percent);
        if (next_icon == nullptr) {
            return;
        }
        const HICON old_icon = icon_;
        icon_ = next_icon;
        if (tray_added_) {
            tray_.hIcon = icon_;
            tray_.uFlags = NIF_ICON;
            Shell_NotifyIconW(NIM_MODIFY, &tray_);
        }
        if (old_icon != nullptr) {
            DestroyIcon(old_icon);
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
    bool tray_added_{};
};

}

namespace CodexTray {

InkBounds MeasureTrayInk(std::optional<int> remaining_percent) {
    const auto rendered = RenderTrayBitmap(remaining_percent);
    InkBounds bounds{32, 32, -1, -1, 0, 0};
    if (rendered.bitmap == nullptr) {
        return bounds;
    }

    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            if ((rendered.pixels[y * 32 + x] & 0x00ffffffU) == 0) {
                continue;
            }
            bounds.left = std::min(bounds.left, x);
            bounds.top = std::min(bounds.top, y);
            bounds.right = std::max(bounds.right, x);
            bounds.bottom = std::max(bounds.bottom, y);
        }
    }
    if (bounds.right >= bounds.left && bounds.bottom >= bounds.top) {
        bounds.width = bounds.right - bounds.left + 1;
        bounds.height = bounds.bottom - bounds.top + 1;
    }
    DeleteObject(rendered.bitmap);
    return bounds;
}

HICON CreateTrayIcon(std::optional<int> remaining_percent) {
    const auto rendered = RenderTrayBitmap(remaining_percent);
    if (rendered.bitmap == nullptr) {
        return nullptr;
    }

    const HBITMAP mask = CreateBitmap(32, 32, 1, 1, nullptr);
    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = rendered.bitmap;
    info.hbmMask = mask;
    const HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    DeleteObject(rendered.bitmap);
    return icon;
}

}

#ifndef CODEX_TRAY_TESTS
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return NativeApp(instance).Run();
}
#endif
