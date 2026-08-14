#include "codex_client.h"

#include "codex_protocol.h"

namespace {

std::string Request(unsigned int id, std::string_view method, std::string_view parameters) {
    return "{\"id\":" + std::to_string(id) + ",\"method\":\"" + std::string(method) + "\",\"params\":" + std::string(parameters) + "}";
}

}

std::string BuildInitializedNotification() {
    return R"json({"method":"initialized","params":{}})json";
}

std::string BuildAccountRequest(unsigned int id) {
    return Request(id, "account/read", R"json({"refreshToken":false})json");
}

std::string BuildRateLimitsRequest(unsigned int id) {
    return Request(id, "account/rateLimits/read", "{}");
}

std::string BuildCancelLoginRequest(unsigned int id, std::string_view login_id) {
    return Request(id, "account/login/cancel", "{\"loginId\":" + CodexTray::JsonString(login_id) + "}");
}

namespace CodexTray {

std::string BuildInitializeRequest(unsigned int id) {
    return Request(id, "initialize", R"json({"clientInfo":{"name":"codex-weekusage-tray","title":"Codex WeekUsage Tray","version":"2.0.0"}})json");
}

std::string BuildLoginRequest(unsigned int id) {
    return Request(id, "account/login/start", R"json({"type":"chatgpt","useHostedLoginSuccessPage":true,"appBrand":"codex"})json");
}

}
