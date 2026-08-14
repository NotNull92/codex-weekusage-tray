#pragma once

inline constexpr char kWeeklyById[] = R"json({"rateLimitsByLimitId":{"codex_weekly":{"primary":{"usedPercent":27,"windowDurationMins":10080,"resetsAt":1781395200}}}})json";
inline constexpr char kFiveHourRateLimits[] = R"json({"rateLimits":{"primary":{"usedPercent":27,"windowDurationMins":300,"resetsAt":1781395200}}})json";
inline constexpr char kRateLimitUpdated[] = R"json({"method":"account/rateLimits/updated","params":{"rateLimitsByLimitId":{"codex_weekly":{"primary":{"usedPercent":27,"windowDurationMins":10080,"resetsAt":1781395200}}}}})json";
inline constexpr char kRateLimitsResult[] = R"json({"id":3,"result":{"rateLimits":{"primary":{"usedPercent":27,"windowDurationMins":10080,"resetsAt":1781395200}}}})json";
inline constexpr char kLoginCompleted[] = R"json({"method":"account/login/completed","params":{"loginId":"login-7","success":true}})json";
