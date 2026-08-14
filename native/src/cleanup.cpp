#define WIN32_LEAN_AND_MEAN

#include "cleanup.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <windows.h>
#include <tlhelp32.h>

namespace {

bool SameText(std::wstring_view left, std::wstring_view right) {
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

std::optional<std::wstring> RegistryString(HKEY key, const wchar_t* name) {
    DWORD type{};
    DWORD bytes{};
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        return std::nullopt;
    }
    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::wstring(value.data());
}

std::vector<CodexTray::TrayEntry> ReadTrayEntries() {
    HKEY root{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings", 0, KEY_READ, &root) != ERROR_SUCCESS) {
        return {};
    }
    std::vector<CodexTray::TrayEntry> entries;
    for (DWORD index = 0;; ++index) {
        std::array<wchar_t, 256> key_name{};
        DWORD length = static_cast<DWORD>(key_name.size());
        const LONG enumerated = RegEnumKeyExW(root, index, key_name.data(), &length, nullptr, nullptr, nullptr, nullptr);
        if (enumerated == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (enumerated != ERROR_SUCCESS) {
            continue;
        }
        HKEY entry{};
        if (RegOpenKeyExW(root, key_name.data(), 0, KEY_QUERY_VALUE, &entry) != ERROR_SUCCESS) {
            continue;
        }
        const auto executable_path = RegistryString(entry, L"ExecutablePath");
        RegCloseKey(entry);
        if (executable_path) {
            entries.push_back({std::wstring(key_name.data(), length), *executable_path});
        }
    }
    RegCloseKey(root);
    return entries;
}

bool IsSelectedPath(const std::vector<CodexTray::TrayEntry>& entries, std::wstring_view executable_path) {
    return std::any_of(entries.begin(), entries.end(), [&](const CodexTray::TrayEntry& entry) {
        return SameText(entry.executable_path, executable_path);
    });
}

unsigned int StopSelectedProcesses(const std::vector<CodexTray::TrayEntry>& entries) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    unsigned int stopped{};
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    for (BOOL found = Process32FirstW(snapshot, &process); found; found = Process32NextW(snapshot, &process)) {
        if (process.th32ProcessID == GetCurrentProcessId() || !SameText(process.szExeFile, L"CodexWeekUsageTray.exe")) {
            continue;
        }
        const HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, process.th32ProcessID);
        if (handle == nullptr) {
            continue;
        }
        std::array<wchar_t, 32768> executable_path{};
        DWORD length = static_cast<DWORD>(executable_path.size());
        if (QueryFullProcessImageNameW(handle, 0, executable_path.data(), &length) && IsSelectedPath(entries, std::wstring_view(executable_path.data(), length)) && TerminateProcess(handle, 0)) {
            ++stopped;
        }
        CloseHandle(handle);
    }
    CloseHandle(snapshot);
    return stopped;
}

}

namespace CodexTray {

std::vector<TrayEntry> SelectTrayEntries(const std::vector<TrayEntry>& entries) {
    std::vector<TrayEntry> selected;
    for (const auto& entry : entries) {
        if (SameText(std::filesystem::path(entry.executable_path).filename().wstring(), L"CodexWeekUsageTray.exe")) {
            selected.push_back(entry);
        }
    }
    return selected;
}

}

int RunUninstall(bool dry_run) {
    const auto entries = CodexTray::SelectTrayEntries(ReadTrayEntries());
    if (entries.empty()) {
        std::wprintf(L"No CodexWeekUsageTray tray settings were found.\n");
        return 0;
    }
    std::wprintf(L"Found %zu CodexWeekUsageTray tray setting(s):\n", entries.size());
    for (const auto& entry : entries) {
        std::wprintf(L"  %ls\n", entry.executable_path.c_str());
    }
    if (dry_run) {
        std::wprintf(L"Dry run only. Nothing was changed.\n");
        return 0;
    }
    HKEY root{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings", 0, KEY_WRITE, &root) != ERROR_SUCCESS) {
        std::wprintf(L"Could not remove CodexWeekUsageTray tray settings.\n");
        return 1;
    }
    const unsigned int stopped = StopSelectedProcesses(entries);
    unsigned int removed{};
    for (const auto& entry : entries) {
        if (RegDeleteTreeW(root, entry.key_name.c_str()) == ERROR_SUCCESS) {
            ++removed;
        }
    }
    RegCloseKey(root);
    std::wprintf(L"Removed %u tray setting(s). Stopped %u matching app process(es).\n", removed, stopped);
    std::wprintf(L"Files were not deleted.\n");
    return removed == entries.size() ? 0 : 1;
}
