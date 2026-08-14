#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "main.h"

#include <algorithm>

namespace {

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
    for (int height = 30; height >= 8; --height) {
        const HFONT font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        const HGDIOBJ old_font = SelectObject(dc, font);
        RECT measured{0, 0, 0, 0};
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &measured, DT_CALCRECT | DT_SINGLELINE);
        if (measured.right <= 31 && measured.bottom <= 31) {
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
