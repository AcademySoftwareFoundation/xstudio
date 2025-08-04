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
    {}
#define CHECK_SLOW_WATCHER()                                                                   \
    {}
#define CHECK_SLOW_WATCHER_FAST()                                                              \
    {}
#define CHECK_SLOW_WATCHER_EXACT(a, b, c)                                                      \
    {}

#endif


namespace xstudio {
/*!
 *  \addtogroup xstudio
 *  @{
 */
namespace utility {

    /*!
     *  \addtogroup utility
     *  @{
     */


    /*! Start logger

      \param lvl - logging level
      \param logfile - optional file to log to

    */
    void start_logger(
        const spdlog::level::level_enum lvl = spdlog::level::info,
        const std::string &logfile          = "");


    //! Stop logger
    void stop_logger();
    /*! @} End of Doxygen UIDataModels*/

} // namespace utility
/*! @} End of Doxygen UIDataModels*/
} // namespace xstudio
