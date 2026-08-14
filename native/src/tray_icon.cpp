#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "main.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kIconSize = 32;
// GDI hinting reshapes bold digits at icon sizes, so nothing is drawn at the
// final size. The label is rendered several times larger, where the outlines
// are reproduced faithfully, and then box-filtered down.
constexpr int kSupersample = 4;
constexpr int kLargeSize = kIconSize * kSupersample;
// A pixel of breathing room on every side at final resolution.
constexpr int kMargin = 1;
constexpr int kUsableSize = kIconSize - 2 * kMargin;
constexpr int kUsableHeight = kUsableSize * kSupersample;
// Three digits will not fit across the icon at a height worth reading, so the
// glyph may be condensed by this much before the type size is reduced instead.
constexpr int kMinCondensePercent = 72;
constexpr int kWidestInk = kUsableHeight * 100 / kMinCondensePercent;
// Coverage below this is not treated as ink, so stray antialiasing does not
// widen the measured bounds.
constexpr unsigned char kInkThreshold = 12;

constexpr COLORREF kGlyphColor = RGB(149, 162, 255);

struct BitmapPixels {
    HBITMAP bitmap{};
    DWORD* pixels{};
};

std::wstring TrayLabel(const std::optional<int>& remaining_percent) {
    return remaining_percent ? std::to_wstring(std::clamp(*remaining_percent, 0, 100)) : L"--";
}

// Box-filtering a supersampled glyph leaves most of a thin stem at partial
// coverage, which reads as a grey smudge once the shell scales the icon down.
// This drops the faintest fringe and pushes what is left towards solid.
unsigned char SharpenCoverage(unsigned int average) {
    constexpr double kFringeCut = 0.09;
    constexpr double kFill = 0.72;
    double level = static_cast<double>(average) / 255.0;
    level = (level - kFringeCut) / (1.0 - kFringeCut);
    if (level <= 0.0) {
        return 0;
    }
    if (level >= 1.0) {
        return 255;
    }
    return static_cast<unsigned char>(std::pow(level, kFill) * 255.0 + 0.5);
}

// An oversized scratch surface the label is drawn onto, plus the coverage that
// draw produced and the exact bounds of the ink.
class GlyphCanvas {
public:
    GlyphCanvas(int width, int height) : width_(width), height_(height), coverage_(static_cast<size_t>(width) * height) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width_;
        info.bmiHeader.biHeight = -height_;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels_), nullptr, 0);
        dc_ = CreateCompatibleDC(nullptr);
        if (dc_ != nullptr && bitmap_ != nullptr) {
            previous_ = SelectObject(dc_, bitmap_);
            SetBkMode(dc_, TRANSPARENT);
            SetTextColor(dc_, RGB(255, 255, 255));
        }
    }

    ~GlyphCanvas() {
        if (dc_ != nullptr) {
            if (previous_ != nullptr) {
                SelectObject(dc_, previous_);
            }
            DeleteDC(dc_);
        }
        if (bitmap_ != nullptr) {
            DeleteObject(bitmap_);
        }
    }

    GlyphCanvas(const GlyphCanvas&) = delete;
    GlyphCanvas& operator=(const GlyphCanvas&) = delete;

    bool Valid() const { return dc_ != nullptr && bitmap_ != nullptr && pixels_ != nullptr; }

    // Draws the label centred and records its coverage. Returns false when the
    // glyph produced no ink at all.
    bool Draw(const std::wstring& label, int font_height) {
        if (!Valid()) {
            return false;
        }
        std::fill(pixels_, pixels_ + static_cast<size_t>(width_) * height_, 0U);
        // Grayscale antialiasing, not ClearType: subpixel colour fringes would
        // corrupt the coverage read back below.
        const HFONT font = CreateFontW(-font_height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (font == nullptr) {
            return false;
        }
        const HGDIOBJ old_font = SelectObject(dc_, font);
        SIZE extent{};
        GetTextExtentPoint32W(dc_, label.c_str(), static_cast<int>(label.size()), &extent);
        TextOutW(dc_, (width_ - extent.cx) / 2, (height_ - extent.cy) / 2, label.c_str(), static_cast<int>(label.size()));
        GdiFlush();
        SelectObject(dc_, old_font);
        DeleteObject(font);

        ink_left_ = width_;
        ink_top_ = height_;
        ink_right_ = -1;
        ink_bottom_ = -1;
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                const DWORD pixel = pixels_[static_cast<size_t>(y) * width_ + x];
                const unsigned char blue = static_cast<unsigned char>(pixel & 0xffU);
                const unsigned char green = static_cast<unsigned char>((pixel >> 8) & 0xffU);
                const unsigned char red = static_cast<unsigned char>((pixel >> 16) & 0xffU);
                const unsigned char value = std::max(blue, std::max(green, red));
                coverage_[static_cast<size_t>(y) * width_ + x] = value;
                if (value >= kInkThreshold) {
                    ink_left_ = std::min(ink_left_, x);
                    ink_top_ = std::min(ink_top_, y);
                    ink_right_ = std::max(ink_right_, x);
                    ink_bottom_ = std::max(ink_bottom_, y);
                }
            }
        }
        return ink_right_ >= ink_left_ && ink_bottom_ >= ink_top_;
    }

    int InkLeft() const { return ink_left_; }
    int InkTop() const { return ink_top_; }
    int InkWidth() const { return ink_right_ - ink_left_ + 1; }
    int InkHeight() const { return ink_bottom_ - ink_top_ + 1; }

    unsigned char At(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            return 0;
        }
        return coverage_[static_cast<size_t>(y) * width_ + x];
    }

private:
    int width_{};
    int height_{};
    std::vector<unsigned char> coverage_;
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
    DWORD* pixels_{};
    int ink_left_{};
    int ink_top_{};
    int ink_right_{-1};
    int ink_bottom_{-1};
};

BitmapPixels RenderTrayBitmap(const std::optional<int>& remaining_percent) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kIconSize;
    info.bmiHeader.biHeight = -kIconSize;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    BitmapPixels rendered;
    rendered.bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&rendered.pixels), nullptr, 0);
    if (rendered.bitmap == nullptr || rendered.pixels == nullptr) {
        return rendered;
    }
    std::fill(rendered.pixels, rendered.pixels + kIconSize * kIconSize, 0U);

    const std::wstring label = TrayLabel(remaining_percent);
    GlyphCanvas canvas(kLargeSize * 3, kLargeSize * 2);
    if (!canvas.Valid()) {
        return rendered;
    }

    // Largest type whose actual ink, not its line box, fits the icon. Measuring
    // the ink is what lets digits fill the height instead of leaving the room a
    // descender would have needed.
    int best = 0;
    int low = 4;
    int high = kLargeSize * 2;
    while (low <= high) {
        const int middle = (low + high) / 2;
        if (canvas.Draw(label, middle) && canvas.InkWidth() <= kWidestInk && canvas.InkHeight() <= kUsableHeight) {
            best = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    if (best == 0 || !canvas.Draw(label, best)) {
        return rendered;
    }

    // Where the ink lands in the finished icon: its natural size vertically,
    // condensed horizontally only if it would otherwise overflow.
    const int source_width = canvas.InkWidth();
    const int source_height = canvas.InkHeight();
    const int target_height = std::max(1, source_height / kSupersample);
    const int target_width = std::clamp(source_width / kSupersample, 1, kUsableSize);
    const int target_left = (kIconSize - target_width) / 2;
    const int target_top = (kIconSize - target_height) / 2;

    const unsigned int red = GetRValue(kGlyphColor);
    const unsigned int green = GetGValue(kGlyphColor);
    const unsigned int blue = GetBValue(kGlyphColor);

    for (int y = 0; y < kIconSize; ++y) {
        const int row = y - target_top;
        const int source_top = canvas.InkTop() + row * source_height / target_height;
        const int source_bottom = canvas.InkTop() + (row + 1) * source_height / target_height;
        for (int x = 0; x < kIconSize; ++x) {
            const int column = x - target_left;
            unsigned int alpha = 0;
            if (row >= 0 && row < target_height && column >= 0 && column < target_width) {
                const int source_left = canvas.InkLeft() + column * source_width / target_width;
                const int source_right = canvas.InkLeft() + (column + 1) * source_width / target_width;
                unsigned int total = 0;
                unsigned int samples = 0;
                for (int sy = source_top; sy < source_bottom; ++sy) {
                    for (int sx = source_left; sx < source_right; ++sx) {
                        total += canvas.At(sx, sy);
                        ++samples;
                    }
                }
                if (samples != 0) {
                    alpha = SharpenCoverage(total / samples);
                }
            }
            // Premultiplied BGRA, which is what the shell alpha-blends.
            const DWORD pixel = (alpha << 24) | ((red * alpha / 255U) << 16) | ((green * alpha / 255U) << 8) | (blue * alpha / 255U);
            rendered.pixels[static_cast<size_t>(y) * kIconSize + x] = pixel;
        }
    }
    return rendered;
}

}

namespace CodexTray {

InkBounds MeasureTrayInk(std::optional<int> remaining_percent) {
    const auto rendered = RenderTrayBitmap(remaining_percent);
    InkBounds bounds{kIconSize, kIconSize, -1, -1, 0, 0};
    if (rendered.bitmap == nullptr) {
        return bounds;
    }
    for (int y = 0; y < kIconSize; ++y) {
        for (int x = 0; x < kIconSize; ++x) {
            const DWORD alpha = rendered.pixels[y * kIconSize + x] >> 24;
            if (alpha < kInkThreshold) {
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
    // Fully opaque mask; the alpha channel above does the real work. Its bits
    // are undefined until cleared, so clear them.
    const HBITMAP mask = CreateBitmap(kIconSize, kIconSize, 1, 1, nullptr);
    if (mask != nullptr) {
        const HDC mask_dc = CreateCompatibleDC(nullptr);
        if (mask_dc != nullptr) {
            const HGDIOBJ previous_mask = SelectObject(mask_dc, mask);
            PatBlt(mask_dc, 0, 0, kIconSize, kIconSize, BLACKNESS);
            SelectObject(mask_dc, previous_mask);
            DeleteDC(mask_dc);
        }
    }
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
