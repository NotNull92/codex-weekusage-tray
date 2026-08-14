#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "core.h"

#include "../third_party/jsmn.h"

#include <algorithm>
#include <charconv>
#include <cwctype>
#include <limits>
#include <vector>
#include <windows.h>
#include <winhttp.h>

namespace {

using Tokens = std::vector<jsmntok_t>;

std::optional<Tokens> ParseTokens(std::string_view json) {
    jsmn_parser parser{};
    jsmn_init(&parser);
    const int token_count = jsmn_parse(&parser, json.data(), json.size(), nullptr, 0);
    if (token_count <= 0) {
        return std::nullopt;
    }

    Tokens tokens(static_cast<size_t>(token_count));
    jsmn_init(&parser);
    const int parsed = jsmn_parse(&parser, json.data(), json.size(), tokens.data(), static_cast<unsigned int>(tokens.size()));
    if (parsed != token_count) {
        return std::nullopt;
    }

    return tokens;
}

std::string_view TokenText(std::string_view json, const jsmntok_t& token) {
    if (token.start < 0 || token.end < token.start || static_cast<size_t>(token.end) > json.size()) {
        return {};
    }

    return json.substr(static_cast<size_t>(token.start), static_cast<size_t>(token.end - token.start));
}

std::optional<int> FindObjectValue(const Tokens& tokens, std::string_view json, int object_index, std::string_view name) {
    if (object_index < 0 || static_cast<size_t>(object_index) >= tokens.size() || tokens[object_index].type != JSMN_OBJECT) {
        return std::nullopt;
    }

    for (size_t index = 0; index + 1 < tokens.size(); ++index) {
        const auto& key = tokens[index];
        if (key.parent != object_index || key.type != JSMN_STRING || TokenText(json, key) != name) {
            continue;
        }

        const auto value_index = static_cast<int>(index + 1);
        if (tokens[value_index].parent == static_cast<int>(index)) {
            return value_index;
        }
    }

    return std::nullopt;
}

template <typename Integer>
std::optional<Integer> TokenInteger(const Tokens& tokens, std::string_view json, int index) {
    if (index < 0 || static_cast<size_t>(index) >= tokens.size() || tokens[index].type != JSMN_PRIMITIVE) {
        return std::nullopt;
    }

    const auto text = TokenText(json, tokens[index]);
    Integer value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }

    return value;
}

std::optional<CodexTray::QuotaSnapshot> QuotaFromWindow(const Tokens& tokens, std::string_view json, int window_index) {
    const auto duration_index = FindObjectValue(tokens, json, window_index, "windowDurationMins");
    const auto used_index = FindObjectValue(tokens, json, window_index, "usedPercent");
    const auto reset_index = FindObjectValue(tokens, json, window_index, "resetsAt");
    if (!duration_index || !used_index || !reset_index) {
        return std::nullopt;
    }

    const auto duration = TokenInteger<int>(tokens, json, *duration_index);
    const auto used = TokenInteger<int>(tokens, json, *used_index);
    const auto reset = TokenInteger<std::int64_t>(tokens, json, *reset_index);
    if (!duration || !used || !reset || *duration != 10'080) {
        return std::nullopt;
    }

    const int used_percent = std::clamp(*used, 0, 100);
    return CodexTray::QuotaSnapshot{used_percent, 100 - used_percent, *reset};
}

std::optional<CodexTray::QuotaSnapshot> QuotaFromBucket(const Tokens& tokens, std::string_view json, int bucket_index) {
    for (const auto name : {"primary", "secondary"}) {
        const auto window_index = FindObjectValue(tokens, json, bucket_index, name);
        if (!window_index || tokens[*window_index].type != JSMN_OBJECT) {
            continue;
        }

        const auto quota = QuotaFromWindow(tokens, json, *window_index);
        if (quota) {
            return quota;
        }
    }

    return std::nullopt;
}

bool IsDirectObjectValue(const Tokens& tokens, int object_index, int parent_object_index) {
    if (object_index < 0 || parent_object_index < 0 || static_cast<size_t>(object_index) >= tokens.size()) {
        return false;
    }

    const int key_index = tokens[object_index].parent;
    return key_index >= 0 && static_cast<size_t>(key_index) < tokens.size() && tokens[key_index].parent == parent_object_index;
}

}

namespace CodexTray {

std::optional<QuotaSnapshot> ParseWeeklyQuota(std::string_view json) {
    const auto tokens = ParseTokens(json);
    if (!tokens || tokens->front().type != JSMN_OBJECT) {
        return std::nullopt;
    }

    if (const auto rate_limits = FindObjectValue(*tokens, json, 0, "rateLimits"); rate_limits && (*tokens)[*rate_limits].type == JSMN_OBJECT) {
        if (const auto quota = QuotaFromBucket(*tokens, json, *rate_limits)) {
            return quota;
        }
    }

    const auto by_limit_id = FindObjectValue(*tokens, json, 0, "rateLimitsByLimitId");
    if (!by_limit_id || (*tokens)[*by_limit_id].type != JSMN_OBJECT) {
        return std::nullopt;
    }

    for (size_t index = 0; index < tokens->size(); ++index) {
        if ((*tokens)[index].type != JSMN_OBJECT || !IsDirectObjectValue(*tokens, static_cast<int>(index), *by_limit_id)) {
            continue;
        }

        if (const auto quota = QuotaFromBucket(*tokens, json, static_cast<int>(index))) {
            return quota;
        }
    }

    return std::nullopt;
}

std::wstring DisplayLabel(const std::optional<QuotaSnapshot>& quota) {
    return quota ? std::to_wstring(quota->remaining_percent) : L"--";
}

bool IsOfficialLoginUrl(std::wstring_view url) {
    if (url.empty() || url.size() > std::numeric_limits<DWORD>::max() || url.find(L'#') != std::wstring_view::npos) {
        return false;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUserNameLength = static_cast<DWORD>(-1);
    parts.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.size()), 0, &parts)
        || parts.nScheme != INTERNET_SCHEME_HTTPS
        || parts.nPort != INTERNET_DEFAULT_HTTPS_PORT
        || parts.dwHostNameLength == 0
        || parts.dwUserNameLength != 0
        || parts.dwPasswordLength != 0) {
        return false;
    }

    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::transform(host.begin(), host.end(), host.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return host == L"chatgpt.com" || host == L"auth.openai.com";
}

std::filesystem::path StandardCodexPath(std::wstring_view local_app_data) {
    return std::filesystem::path(local_app_data) / L"Programs" / L"OpenAI" / L"Codex" / L"bin" / L"codex.exe";
}

std::string JsonString(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');

    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value) {
        switch (character) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20) {
                    result += "\\u00";
                    result.push_back(digits[character >> 4]);
                    result.push_back(digits[character & 0x0f]);
                } else {
                    result.push_back(static_cast<char>(character));
                }
                break;
        }
    }

    result.push_back('"');
    return result;
}

}
