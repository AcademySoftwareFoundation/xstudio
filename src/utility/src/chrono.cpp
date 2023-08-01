// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/chrono.hpp"

xstudio::utility::sys_time_point
xstudio::utility::to_sys_time_point(const std::string &datetime) {
    std::istringstream in{datetime};
    sys_time_point tp;
#ifdef _WIN32
//TODO: Ahead to fix
#else
    in >> date::parse("%Y-%m-%dT%TZ", tp);
#endif
    return tp;
}
