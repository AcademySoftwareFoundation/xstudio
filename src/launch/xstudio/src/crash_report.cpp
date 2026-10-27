// SPDX-License-Identifier: Apache-2.0
#include "crash_report.hpp"
#include "crash_handler.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

#ifndef XSTUDIO_GIT_HASH
#define XSTUDIO_GIT_HASH "unknown"
#endif
#ifndef XSTUDIO_BUILD_TYPE
#define XSTUDIO_BUILD_TYPE "unknown"
#endif

// Everything that is written when xstudio crashes is formatted in this file,
// for every platform. The facts that are known before the crash are gathered
// at startup into fixed storage, the signal handler must not touch the heap.

namespace xstudio::launch {

namespace {

    const int crash_output_stderr = 2;

    char report_dir[1024]   = "";
    char command_line[4096] = "";
    char log_file_path[1024] = "";
    char host_name[256]     = "";
    char os_name[256]       = "";
    long long start_time    = 0;
    long long utc_offset    = 0; // seconds, local time minus UTC

    struct Time {
        long long year, month, day, hour, minute, second;
    };

    // Formatted by hand, the C library functions can allocate.
    Time local_time() {
        const long long now  = static_cast<long long>(std::time(nullptr)) + utc_offset;
        const long long secs = now % 86400;
        const long long z    = now / 86400 + 719468;
        const long long era  = z / 146097;
        const long long doe  = z - era * 146097;
        const long long yoe  = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const long long doy  = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const long long mp   = (5 * doy + 2) / 153;
        Time t;
        t.day    = doy - (153 * mp + 2) / 5 + 1;
        t.month  = mp < 10 ? mp + 3 : mp - 9;
        t.year   = yoe + era * 400 + (t.month <= 2 ? 1 : 0);
        t.hour   = secs / 3600;
        t.minute = secs / 60 % 60;
        t.second = secs % 60;
        return t;
    }

    unsigned long long as_number(const void *address) {
        return static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address));
    }

    // Writes one line to stderr and, when open, to the report file.
    struct Report {
        int file = -1;

        void print(const char *format, ...) {
            char line[2048];
            va_list args;
            va_start(args, format);
            int size = vsnprintf(line, sizeof(line), format, args);
            va_end(args);
            if (size < 0)
                return;
            if (size >= static_cast<int>(sizeof(line))) {
                // too long, keep the line break
                size           = static_cast<int>(sizeof(line)) - 1;
                line[size - 1] = '\n';
            }
            write_crash_output(crash_output_stderr, line, static_cast<size_t>(size));
            if (file >= 0)
                write_crash_output(file, line, static_cast<size_t>(size));
        }
    };

} // namespace

void write_crash_report(const CrashInfo &info, const CrashFrame *frames, int frame_count) {

    const Time t = local_time();

    char name[160]                               = "";
    char path[sizeof(report_dir) + sizeof(name)] = "";
    Report report;
    if (report_dir[0]) {
        snprintf(
            name,
            sizeof(name),
            "xstudio-crash-%04d%02d%02d-%02d%02d%02d-%llu.txt",
            static_cast<int>(t.year),
            static_cast<int>(t.month),
            static_cast<int>(t.day),
            static_cast<int>(t.hour),
            static_cast<int>(t.minute),
            static_cast<int>(t.second),
            info.process_id);
        snprintf(path, sizeof(path), "%s/%s", report_dir, name);
        report.file = open_crash_output(path);
    }

    char fault[96] = "";
    if (info.fault_description && info.has_fault_address) {
        snprintf(
            fault,
            sizeof(fault),
            ", %s at 0x%llx",
            info.fault_description,
            as_number(info.fault_address));
    } else if (info.fault_description) {
        snprintf(fault, sizeof(fault), ", %s", info.fault_description);
    }

    char thread_name[96] = "";
    if (info.thread_name && info.thread_name[0])
        snprintf(thread_name, sizeof(thread_name), " \"%s\"", info.thread_name);

    const long long uptime = static_cast<long long>(std::time(nullptr)) - start_time;

    report.print(
        "\nxSTUDIO v" PROJECT_VERSION " (git " XSTUDIO_GIT_HASH ", " XSTUDIO_BUILD_TYPE
        ") crashed: %s (signal %d)%s\n",
        info.signal_name ? info.signal_name : "unknown signal",
        info.signal,
        fault);
    report.print(
        "time %04lld-%02lld-%02lld %02lld:%02lld:%02lld, pid %llu, thread %llu%s, uptime "
        "%lldh%02lldm%02llds, resident memory %llu MB\n",
        t.year,
        t.month,
        t.day,
        t.hour,
        t.minute,
        t.second,
        info.process_id,
        info.thread_id,
        thread_name,
        uptime / 3600,
        uptime / 60 % 60,
        uptime % 60,
        resident_memory_kb() / 1024);
    report.print("host %s, %s\n", host_name, os_name);
    report.print("command %s\n", command_line);
    report.print("log file %s\n", log_file_path[0] ? log_file_path : "none");
    report.print("stack of the crashing thread:\n");

    for (int i = 0; i < frame_count; ++i) {
        const CrashFrame &frame = frames[i];
        if (frame.module && frame.symbol) {
            report.print(
                "%2d: %s+0x%llx %s+0x%llx\n",
                i,
                frame.module,
                frame.module_offset,
                frame.symbol,
                frame.symbol_offset);
        } else if (frame.module) {
            report.print("%2d: %s+0x%llx\n", i, frame.module, frame.module_offset);
        } else if (frame.symbol) {
            report.print("%2d: %s+0x%llx\n", i, frame.symbol, frame.symbol_offset);
        } else {
            report.print("%2d: [0x%llx]\n", i, as_number(frame.address));
        }
    }

    if (report.file >= 0) {
        close_crash_output(report.file);
        report.file = -1;
        report.print("Crash report saved to %s\n", path);
    } else if (report_dir[0]) {
        report.print("Could not save the crash report to %s\n", report_dir);
    }
}

void set_crash_report_log_file(const std::string &path) {
    if (path.size() < sizeof(log_file_path))
        std::memcpy(log_file_path, path.c_str(), path.size() + 1);
}

void install_crash_handler(const std::string &dir, int argc, char **argv) {

    if (dir.size() < sizeof(report_dir)) {
        std::error_code ignored;
        std::filesystem::create_directories(dir, ignored);
        std::memcpy(report_dir, dir.c_str(), dir.size() + 1);
    }

    size_t used = 0;
    for (int i = 0; i < argc && argv[i]; ++i) {
        const size_t size = std::strlen(argv[i]);
        if (used + size + 2 > sizeof(command_line))
            break;
        if (i > 0)
            command_line[used++] = ' ';
        std::memcpy(command_line + used, argv[i], size + 1);
        used += size;
    }

    describe_system(host_name, sizeof(host_name), os_name, sizeof(os_name));

    const std::time_t now = std::time(nullptr);
    start_time            = static_cast<long long>(now);
    std::tm local         = *std::localtime(&now);
    std::tm utc           = *std::gmtime(&now);
    utc.tm_isdst          = local.tm_isdst;
    utc_offset =
        static_cast<long long>(std::difftime(std::mktime(&local), std::mktime(&utc)));

    install_fatal_signal_handlers();
}

} // namespace xstudio::launch
