#pragma once

#include <optional>
#include <string>

namespace CodexTray {

struct QuotaSnapshot {
    int remaining_percent;
};

std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota);

}
