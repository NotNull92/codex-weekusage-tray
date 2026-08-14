#include "core.h"

namespace CodexTray {

std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota) {
    return quota ? std::to_wstring(quota->remaining_percent) : L"--";
}

}
