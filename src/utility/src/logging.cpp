// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
// #include "spdlog/async.h"
#include <csignal>

void xstudio::utility::start_logger(
    const spdlog::level::level_enum lvl, const std::string &logfile) {
    // spdlog::init_thread_pool(8192, 1);
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_level(lvl);

    std::vector<spdlog::sink_ptr> sinks{stderr_sink}; //, rotating_sink};

    if (not logfile.empty()) {
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logfile, 1024 * 1024 * 10, 10);
        rotating_sink->set_level(spdlog::level::debug);
        sinks.push_back(rotating_sink);
    }

    // auto logger = std::make_shared<spdlog::async_logger>("xstudio", sinks.begin(),
    // sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    auto logger = std::make_shared<spdlog::logger>("xstudio", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    //spdlog::set_level(spdlog::level::debug);

    // spdlog::set_error_handler([](const std::string &msg){
    //     spdlog::warn("{}", msg);
    //     raise(SIGSEGV);
    // });

    spdlog::info("XStudio logging started.");
}

void xstudio::utility::stop_logger() {
    spdlog::info("XStudio logging stopped.");
    // spdlog::shutdown();
}
