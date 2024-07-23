// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ctime>      // localtime
#include <sstream>    // stringstream
#include <iomanip>    // put_time
#include <string>     // string
#include <flicks.hpp> // string
#include <date/date.h>

namespace xstudio {

namespace utility {
    using clock        = std::chrono::steady_clock;
    using time_point   = clock::time_point;
    using milliseconds = std::chrono::milliseconds;

#ifdef _WIN32
    using sysclock = std::chrono::high_resolution_clock;
#else
    using sysclock = std::chrono::system_clock;
#endif
    using sys_time_point    = sysclock::time_point;
    using sys_time_duration = sysclock::duration;

    inline std::string to_string(const sys_time_point &tp) {
#ifdef _WIN32
        std::stringstream ss;
        // TODO: Ahead Fix
        // ss << std::put_time(std::localtime(in_time_t), "%Y-%m-%d %X");
        return ss.str();
#else
        auto in_time_t = std::chrono::system_clock::to_time_t(tp);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
        return ss.str();
#endif
    }
    // 2021-12-21T10:26:37Z
    utility::sys_time_point to_sys_time_point(const std::string &datetime);


    template <typename T> inline auto to_epoc_microseconds(const T &tp) {
        return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch())
            .count();
    }

    template <typename T> inline auto to_epoc_milliseconds(const T &tp) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
            .count();
    }

    inline std::string date_time_and_zone() {
        return date::format(
            "%FT%TZ", date::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
    }

} // namespace utility

} // namespace xstudio

using json = nlohmann::json;

namespace nlohmann {
template <typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static void to_json(json &j, const std::chrono::time_point<Clock, Duration> &tp) {
        j["since_epoch"] =
            std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch())
                .count();
        j["unit"] = "microseconds";
    }
    static void from_json(const json &j, std::chrono::time_point<Clock, Duration> &p) {
        if (j.at("unit") == "microseconds") {
            std::chrono::time_point<Clock, Duration> tp(
                std::chrono::microseconds(j["since_epoch"].get<int64_t>()));
            p = tp;
        } else if (j.at("unit") == "milliseconds") {
            std::chrono::time_point<Clock, Duration> tp(
                std::chrono::milliseconds(j["since_epoch"].get<int64_t>()));
            p = tp;
        }
    }
};
} // namespace nlohmann


namespace std::chrono {
inline void to_json(json &j, const timebase::flicks &p) { j = json{{"count", p.count()}}; }

inline void from_json(const json &j, timebase::flicks &p) {
    p = timebase::flicks(j.at("count"));
}
} // namespace std::chrono
