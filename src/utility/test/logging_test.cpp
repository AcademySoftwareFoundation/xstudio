// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio::utility;
using namespace std::chrono_literals;

TEST(LoggingTest, Test) {
    start_logger(spdlog::level::debug);
    {
        START_SLOW_WATCHER();
        CHECK_SLOW_WATCHER();
    }

    {
        START_SLOW_WATCHER();
        std::this_thread::sleep_for(100ms);
        CHECK_SLOW_WATCHER();
    }

    {
        START_SLOW_WATCHER();
        std::this_thread::sleep_for(500ms);
        CHECK_SLOW_WATCHER();
    }

    {
        START_SLOW_WATCHER();
        std::this_thread::sleep_for(2000ms);
        CHECK_SLOW_WATCHER();
    }
}