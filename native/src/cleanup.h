#pragma once

#include <string>
#include <vector>

namespace CodexTray {

struct TrayEntry {
    std::wstring key_name;
    std::wstring executable_path;
};

std::vector<TrayEntry> SelectTrayEntries(const std::vector<TrayEntry>& entries);

}

int RunUninstall(bool dry_run);
