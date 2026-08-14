#pragma once

#include <optional>
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

}
