#pragma once

#include <string>
#include <string_view>

std::string BuildInitializedNotification();
std::string BuildAccountRequest(unsigned int id);
std::string BuildRateLimitsRequest(unsigned int id);
std::string BuildCancelLoginRequest(unsigned int id, std::string_view login_id);
