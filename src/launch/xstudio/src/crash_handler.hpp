// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace xstudio::launch {

// When the process receives a fatal signal (SIGSEGV, SIGABRT, SIGFPE, SIGILL
// and, where it exists, SIGBUS) a crash report is written to stderr and to a
// new file in report_dir, then the process ends at once with status 1 and
// without a core dump. The normal shutdown is skipped, so output that is still
// buffered is lost; the --log-file file is not affected, it is written line by
// line.
//
// The report has the build (version, git hash, build type), the signal and
// what it says about the fault, the process, thread, uptime, memory use, host
// and command line, then the stack of the crashing thread, one frame per line
// as module path, offset into the module, symbol name and offset into the
// symbol. Symbol names are left mangled, c++filt makes them readable, and
// addr2line turns a module offset into a file and line when the build has
// debug information.
//
// Nothing in the handler allocates memory, so the report is also written when
// the crash was caused by a corrupted heap.
//
// argv is kept for the command line in the report.
//
// Only Linux has this report so far. Windows and MacOS keep the previous
// handler (signal number and ten frames for SIGSEGV, exit(1)), see the files
// under platform/.
void install_crash_handler(const std::string &report_dir, int argc, char **argv);

// Names the session log file in the report, so that the two can be paired.
// Call it once the log file is known, with an empty path when there is none.
void set_crash_report_log_file(const std::string &path);

} // namespace xstudio::launch
