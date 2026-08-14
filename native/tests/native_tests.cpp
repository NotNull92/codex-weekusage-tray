#include "../src/core.h"

#include <stdexcept>

int wmain() {
    if (CodexTray::DisplayLabel(std::nullopt) != L"--") {
        throw std::runtime_error("Missing quota must render two dashes.");
    }

    return 0;
}
