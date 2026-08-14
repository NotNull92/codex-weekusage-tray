#pragma once

#include <optional>
#include <windows.h>

enum class LaunchMode {
    Normal,
    PreviewPending,
    PreviewReady
};

int RunNativeApp(HINSTANCE instance, LaunchMode mode, std::optional<int> preview_remaining_percent);
