// SPDX-License-Identifier: Apache-2.0
#include "crash_handler.hpp"

#include <windows.h>

#include <DbgHelp.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>

// Windows side of the crash handler.
//
// The crash report below is the one xstudio had before the Linux report was
// reworked: signal number and ten frames on stderr, then exit(1). The reworked
// report (platform independent part in crash_report.cpp, Windows part at the
// end of this file) has never been compiled on Windows, so it is kept commented
// out until it has been built and tested there. To switch: replace this file's
// active code with the commented block, add crash_report.cpp to the Windows
// SOURCES in CMakeLists.txt and link psapi.

namespace {

void handler(int sig) {
    void *stack[10];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    // Capture the call stack
    WORD frames = CaptureStackBackTrace(0, 10, stack, nullptr);

    // Print out the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    for (int i = 0; i < frames; ++i) {
        DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        SYMBOL_INFO *symbol  = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = MAX_SYM_NAME;

        if (SymFromAddr(process, address, nullptr, symbol)) {
            fprintf(stderr, "%d: %s\n", i, symbol->Name);
        } else {
            fprintf(stderr, "%d: [Unknown Symbol]\n", i);
        }
    }

    SymCleanup(process);
    exit(1);
}

} // namespace

namespace xstudio::launch {

void install_crash_handler(const std::string &, int, char **) { signal(SIGSEGV, handler); }

void set_crash_report_log_file(const std::string &) {}

} // namespace xstudio::launch

// ---------------------------------------------------------------------------
// Reworked Windows implementation, UNTESTED, never compiled. See the note at
// the top of this file.
// ---------------------------------------------------------------------------
// #include "crash_report.hpp"
//
// #include <windows.h>
//
// #include <DbgHelp.h>
// #include <psapi.h>
//
// #include <csignal>
// #include <cstdio>
// #include <cstdlib>
// #include <cstring>
// #include <fcntl.h>
// #include <io.h>
// #include <sys/stat.h>
//
// // Windows side of the crash handler.
//
// namespace xstudio::launch {
//
// namespace {
//
//     struct FatalSignal {
//         int number;
//         const char *name;
//     };
//
//     // signals that mean the process is going down
//     const FatalSignal fatal_signals[] = {
//         {SIGSEGV, "SIGSEGV"},
//         {SIGABRT, "SIGABRT"},
//         {SIGFPE, "SIGFPE"},
//         {SIGILL, "SIGILL"},
//     };
//
//     // the names the frames point at
//     char symbol_names[max_crash_frames][MAX_SYM_NAME];
//     char module_names[max_crash_frames][MAX_PATH];
//
//     void handler(int sig) {
//         // a second fault while the report is printing kills the process instead of
//         // re-entering the handler
//         for (const auto &s : fatal_signals)
//             signal(s.number, SIG_DFL);
//
//         HANDLE process = GetCurrentProcess();
//         SymInitialize(process, nullptr, TRUE);
//
//         // Capture the call stack, leaving out this handler
//         void *stack[max_crash_frames];
//         const int size = CaptureStackBackTrace(1, max_crash_frames, stack, nullptr);
//
//         CrashInfo info;
//         info.signal = sig;
//         for (const auto &s : fatal_signals) {
//             if (s.number == sig)
//                 info.signal_name = s.name;
//         }
//         info.process_id = GetCurrentProcessId();
//         info.thread_id  = GetCurrentThreadId();
//
//         CrashFrame frames[max_crash_frames];
//         for (int i = 0; i < size; ++i) {
//             CrashFrame &frame = frames[i];
//             frame.address     = stack[i];
//
//             const DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
//
//             IMAGEHLP_MODULE64 module;
//             memset(&module, 0, sizeof(module));
//             module.SizeOfStruct = sizeof(module);
//             if (SymGetModuleInfo64(process, address, &module)) {
//                 strncpy(module_names[i], module.ImageName, MAX_PATH - 1);
//                 frame.module        = module_names[i];
//                 frame.module_offset = address - module.BaseOfImage;
//             }
//
//             char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
//             SYMBOL_INFO *symbol  = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
//             symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
//             symbol->MaxNameLen   = MAX_SYM_NAME;
//             DWORD64 displacement = 0;
//             if (SymFromAddr(process, address, &displacement, symbol)) {
//                 strncpy(symbol_names[i], symbol->Name, MAX_SYM_NAME - 1);
//                 frame.symbol        = symbol_names[i];
//                 frame.symbol_offset = displacement;
//             }
//         }
//
//         write_crash_report(info, frames, size);
//
//         SymCleanup(process);
//
//         // not exit(): the normal shutdown does not survive in a process that has
//         // crashed
//         _exit(1);
//     }
//
//     // UNTESTED, never compiled. The signal() route above cannot tell where a
//     // memory fault happened. Windows delivers that through the exception
//     // record of an unhandled exception filter, which runs before the C runtime
//     // turns the exception into SIGSEGV. To use it, call
//     //     SetUnhandledExceptionFilter(exception_filter);
//     // in install_fatal_signal_handlers() and move the body of handler() into a
//     // function that takes a CrashInfo, so both paths share it.
//     //
//     // LONG WINAPI exception_filter(EXCEPTION_POINTERS *pointers) {
//     //     const EXCEPTION_RECORD *record = pointers->ExceptionRecord;
//     //     CrashInfo info;
//     //     switch (record->ExceptionCode) {
//     //     case EXCEPTION_ACCESS_VIOLATION:
//     //         info.signal            = SIGSEGV;
//     //         info.signal_name       = "SIGSEGV";
//     //         info.fault_description = record->ExceptionInformation[0] == 0
//     //                                      ? "read from unmapped address"
//     //                                      : "write to unmapped address";
//     //         info.has_fault_address = true;
//     //         info.fault_address =
//     //             reinterpret_cast<const void *>(record->ExceptionInformation[1]);
//     //         break;
//     //     case EXCEPTION_INT_DIVIDE_BY_ZERO:
//     //         info.signal            = SIGFPE;
//     //         info.signal_name       = "SIGFPE";
//     //         info.fault_description = "integer division by zero";
//     //         break;
//     //     case EXCEPTION_ILLEGAL_INSTRUCTION:
//     //         info.signal            = SIGILL;
//     //         info.signal_name       = "SIGILL";
//     //         info.fault_description = "illegal instruction";
//     //         break;
//     //     default:
//     //         return EXCEPTION_CONTINUE_SEARCH; // not a crash we report
//     //     }
//     //     // ... gather the stack as handler() does, using
//     //     // StackWalk64(pointers->ContextRecord) to start at the faulting
//     //     // instruction instead of here, then write_crash_report() and _exit(1).
//     //     return EXCEPTION_EXECUTE_HANDLER;
//     // }
//
//     // UNTESTED, never compiled. Thread names on Windows 10 and later. The call
//     // allocates the returned string, which is acceptable only because the rest
//     // of this platform's handler (DbgHelp) allocates anyway.
//     //
//     // void current_thread_name(char *name, size_t size) {
//     //     name[0]           = 0;
//     //     PWSTR description = nullptr;
//     //     if (SUCCEEDED(GetThreadDescription(GetCurrentThread(), &description)) &&
//     //         description) {
//     //         WideCharToMultiByte(
//     //             CP_UTF8, 0, description, -1, name, static_cast<int>(size),
//     //             nullptr, nullptr);
//     //         LocalFree(description);
//     //     }
//     // }
//
// } // namespace
//
// void install_fatal_signal_handlers() {
//     for (const auto &s : fatal_signals)
//         signal(s.number, handler);
// }
//
// void describe_system(char *host, size_t host_size, char *os, size_t os_size) {
//     DWORD size = static_cast<DWORD>(host_size);
//     if (!GetComputerNameA(host, &size))
//         host[0] = 0;
//     snprintf(os, os_size, "Windows");
//
//     // UNTESTED, never compiled. GetVersionEx lies to applications without a
//     // manifest, RtlGetVersion from ntdll does not. Needs #include <winternl.h>.
//     //
//     // using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
//     // if (HMODULE ntdll = GetModuleHandleA("ntdll.dll")) {
//     //     if (auto rtl_get_version =
//     //             reinterpret_cast<RtlGetVersionFn>(
//     //                 GetProcAddress(ntdll, "RtlGetVersion"))) {
//     //         RTL_OSVERSIONINFOW version;
//     //         memset(&version, 0, sizeof(version));
//     //         version.dwOSVersionInfoSize = sizeof(version);
//     //         if (rtl_get_version(&version) == 0) {
//     //             snprintf(
//     //                 os,
//     //                 os_size,
//     //                 "Windows %lu.%lu build %lu",
//     //                 version.dwMajorVersion,
//     //                 version.dwMinorVersion,
//     //                 version.dwBuildNumber);
//     //         }
//     //     }
//     // }
// }
//
// unsigned long long resident_memory_kb() {
//     PROCESS_MEMORY_COUNTERS counters;
//     memset(&counters, 0, sizeof(counters));
//     counters.cb = sizeof(counters);
//     if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
//         return 0;
//     return static_cast<unsigned long long>(counters.WorkingSetSize) / 1024;
// }
//
// int open_crash_output(const char *path) {
//     return _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
// }
//
// void write_crash_output(int output, const char *text, size_t size) {
//     _write(output, text, static_cast<unsigned int>(size));
// }
//
// void close_crash_output(int output) { _close(output); }
//
// } // namespace xstudio::launch
