// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;

#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
// #include "spdlog/async.h"
#include <caf/error.hpp>
#include <csignal>
#include <filesystem>

namespace {

// The session log file hangs off this sink, so that it can be replaced after
// logging has started (the preference is read after the command line).
// dist_sink adds and removes sinks under its own lock.
std::shared_ptr<spdlog::sinks::dist_sink_mt> file_sink_slot;
spdlog::sink_ptr file_sink;
std::string file_sink_path;

} // namespace

void xstudio::utility::start_logger(
    const spdlog::level::level_enum lvl, const std::string &logfile) {

    static bool logging_started = false;
    if (logging_started)
        return;
    logging_started = true;

    // spdlog::init_thread_pool(8192, 1);
    auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    stderr_sink->set_level(lvl);

    file_sink_slot = std::make_shared<spdlog::sinks::dist_sink_mt>();
    file_sink_slot->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks{stderr_sink, file_sink_slot};

    // auto logger = std::make_shared<spdlog::async_logger>("xstudio", sinks.begin(),
    // sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    auto logger = std::make_shared<spdlog::logger>("xstudio", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);

    // spdlog::set_error_handler([](const std::string &msg){
    //     spdlog::warn("{}", msg);
    //     raise(SIGSEGV);
    // });

    spdlog::info("XStudio logging started.");

    if (not logfile.empty())
        set_log_file(logfile);
}

void xstudio::utility::set_log_file(const std::string &logfile) {

    if (not file_sink_slot) {
        spdlog::error("Cannot write the session log to {}: logging not started.", logfile);
        return;
    }

    spdlog::sink_ptr sink;
    try {
        std::error_code ignored;
        std::filesystem::create_directories(
            std::filesystem::path(logfile).parent_path(), ignored);
        // rotate on open: each run writes a new file, the previous runs are
        // kept as .1 ... .10
        sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logfile, 1024 * 1024 * 10, 10, true);
        sink->set_level(spdlog::level::debug);
    } catch (const std::exception &e) {
        spdlog::error("Cannot write the session log to {}: {}", logfile, e.what());
        return;
    }

    if (file_sink) {
        spdlog::info("Session log continues in {}", logfile);
        file_sink_slot->remove_sink(file_sink);
        file_sink->flush();
    }
    file_sink      = sink;
    file_sink_path = logfile;
    file_sink_slot->add_sink(sink);
    // the log file is written line by line, so that its last lines are not lost
    // if xstudio crashes
    spdlog::default_logger()->flush_on(spdlog::level::debug);
    spdlog::info("Session log written to {}", logfile);
}

std::string xstudio::utility::log_file() { return file_sink_path; }

void xstudio::utility::log_caf_error(
    const caf::error &err,
    const spdlog::level::level_enum level,
    const std::string_view context,
    const SourceLocation where) {

    if (not err)
        return;

    // the file name alone locates the site with the line number; the function
    // name is "operator()" inside the lambdas these calls live in
    std::string_view file(where.file_name());
    const auto slash = file.find_last_of("/\\");
    if (slash != std::string_view::npos)
        file.remove_prefix(slash + 1);

    if (context.empty())
        spdlog::log(level, "{}:{} {}", file, where.line(), caf::to_string(err));
    else
        spdlog::log(level, "{}:{} {} {}", file, where.line(), context, caf::to_string(err));
}

void xstudio::utility::stop_logger() {
    spdlog::info("XStudio logging stopped.");
    spdlog::default_logger()->flush();

    // spdlog::shutdown();
}
