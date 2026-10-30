// SPDX-License-Identifier: Apache-2.0
#include "crash_handler.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

// MacOS side of the crash handler.
//
// The crash report below is the one xstudio had before the Linux report was
// reworked: signal number and ten frames on stderr, then exit(1). The reworked
// report (crash_report.cpp plus platform/posix/crash_handler.cpp) is written
// for Linux and MacOS but has only been compiled and tested on Linux, so MacOS
// keeps the old report until the new one has been built and tested there. To
// switch: in CMakeLists.txt compile crash_report.cpp and
// platform/posix/crash_handler.cpp for APPLE instead of this file, and enable
// the resident memory block in the posix file.

namespace {

void handler(int sig) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

} // namespace

namespace xstudio::launch {

void install_crash_handler(const std::string &, int, char **) { signal(SIGSEGV, handler); }

void set_crash_report_log_file(const std::string &) {}

} // namespace xstudio::launch
