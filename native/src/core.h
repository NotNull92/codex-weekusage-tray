#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace CodexTray {

struct QuotaSnapshot {
    int used_percent;
    int remaining_percent;
    std::int64_t resets_at_unix_seconds;
};

std::optional<QuotaSnapshot> ParseWeeklyQuota(std::string_view json);
std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota);
bool IsOfficialLoginUrl(std::wstring_view url);
std::filesystem::path StandardCodexPath(std::wstring_view local_app_data);
std::string JsonString(std::string_view value);

}
