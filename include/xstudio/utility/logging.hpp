// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(_WIN32)
#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#endif
/*
    \file logging.h
    Stop and start logging system
*/

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <string>
#include <string_view>

namespace caf {
class error;
}

#ifdef XSTUDIO_DEBUG

#define START_SLOW_WATCHER() spdlog::stopwatch _sw;

#define CHECK_SLOW_WATCHER() CHECK_SLOW_WATCHER_EXACT(1.0, 0.5, 0.1)

#define CHECK_SLOW_WATCHER_FAST() CHECK_SLOW_WATCHER_EXACT(0.1, 0.05, 0.025)

#define CHECK_SLOW_WATCHER_EXACT(a, b, c)                                                      \
    if (_sw.elapsed().count() > a)                                                             \
        spdlog::critical("{}@{} Sec: {:.3f}", __PRETTY_FUNCTION__, __LINE__, _sw);             \
    else if (_sw.elapsed().count() > b)                                                        \
        spdlog::error("{}@{} Sec: {:.3f}", __PRETTY_FUNCTION__, __LINE__, _sw);                \
    else if (_sw.elapsed().count() > c)                                                        \
        spdlog::warn("{}@{} Sec: {:.3f}", __PRETTY_FUNCTION__, __LINE__, _sw);                 \
    else                                                                                       \
        spdlog::debug("{}@{} Sec: {:.3f}", __PRETTY_FUNCTION__, __LINE__, _sw);


#else

#define START_SLOW_WATCHER()                                                                   \
    {                                                                                          \
    }
#define CHECK_SLOW_WATCHER()                                                                   \
    {                                                                                          \
    }
#define CHECK_SLOW_WATCHER_FAST()                                                              \
    {                                                                                          \
    }
#define CHECK_SLOW_WATCHER_EXACT(a, b, c)                                                      \
    {                                                                                          \
    }

#endif


namespace xstudio::utility {

/*!
 *  \addtogroup utility
 *  @{
 */


/*! Start logger

  \param lvl - logging level of the terminal output
  \param logfile - optional file to log to, see set_log_file()

*/
void start_logger(
    const spdlog::level::level_enum lvl = spdlog::level::info, const std::string &logfile = "");


//! Stop logger
void stop_logger();

/*! Write the session log to a file, replacing the file set before, if any.

  The file receives every level, is flushed line by line so it is complete up
  to a crash, and is rotated when it is opened, so each run starts a new file
  and the previous runs are kept next to it as .1, .2 ... When the file cannot
  be opened the error is logged and the terminal output continues alone.
*/
void set_log_file(const std::string &logfile);

//! Path of the session log file, empty when the log goes to the terminal only.
std::string log_file();

/*! Where a log call was made.

  Built on the compiler builtins that std::source_location wraps, so that it
  works at C++17. Replace it with std::source_location when the project builds
  as C++20: the accessors have the same names and no call site changes.
*/
struct SourceLocation {
    static SourceLocation current(
        const char *file     = __builtin_FILE(),
        int line             = __builtin_LINE(),
        const char *function = __builtin_FUNCTION()) noexcept {
        return SourceLocation{file, line, function};
    }

    [[nodiscard]] const char *file_name() const noexcept { return file_; }
    [[nodiscard]] int line() const noexcept { return line_; }
    [[nodiscard]] const char *function_name() const noexcept { return function_; }

    const char *file_     = "";
    int line_             = 0;
    const char *function_ = "";
};

/*! Log an error delivered to a request's error handler.

  The line names the call site as file:line, then the context, if given, then
  the error. An empty error is not logged: CAF delivers one when the actor
  that was asked exited normally before answering (its exit reason is the
  empty error), which says nothing beyond "no answer".

  Call as log_caf_error(err), log_caf_error(err, spdlog::level::err) or
  log_caf_error(err, spdlog::level::warn, "what was being asked").
*/
void log_caf_error(
    const caf::error &err,
    spdlog::level::level_enum level = spdlog::level::warn,
    std::string_view context        = {},
    SourceLocation where            = SourceLocation::current());
/*! @} End of Doxygen UIDataModels*/

} // namespace xstudio::utility
/*! @} End of Doxygen UIDataModels*/
