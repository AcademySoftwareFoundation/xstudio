// SPDX-License-Identifier: Apache-2.0
#include "crash_report.hpp"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

// Linux and MacOS side of the crash handler.

namespace xstudio::launch {

namespace {

    struct FatalSignal {
        int number;
        const char *name;
    };

    // signals that mean the process is going down
    const FatalSignal fatal_signals[] = {
        {SIGSEGV, "SIGSEGV"},
        {SIGABRT, "SIGABRT"},
        {SIGFPE, "SIGFPE"},
        {SIGILL, "SIGILL"},
        {SIGBUS, "SIGBUS"},
    };

    unsigned long long current_thread_id() {
#ifdef __linux__
        return static_cast<unsigned long long>(syscall(SYS_gettid));
#else
        uint64_t id = 0;
        pthread_threadid_np(nullptr, &id);
        return static_cast<unsigned long long>(id);
#endif
    }

    char sent_by[64] = "";

    // what the signal code says about the fault, null when it says nothing
    const char *fault_description(const siginfo_t *siginfo) {
        // a code that is not positive means the signal was sent by a process,
        // which for SIGABRT is this one calling abort()
        if (siginfo->si_code <= 0) {
            if (siginfo->si_pid == getpid())
                return nullptr;
            snprintf(
                sent_by,
                sizeof(sent_by),
                "sent by process %lld",
                static_cast<long long>(siginfo->si_pid));
            return sent_by;
        }
        switch (siginfo->si_signo) {
        case SIGSEGV:
            if (siginfo->si_code == SEGV_MAPERR)
                return "address not mapped";
            if (siginfo->si_code == SEGV_ACCERR)
                return "address not accessible";
            return "invalid memory access";
        case SIGBUS:
            if (siginfo->si_code == BUS_ADRALN)
                return "misaligned address";
            return "bus error";
        case SIGFPE:
            if (siginfo->si_code == FPE_INTDIV)
                return "integer division by zero";
            if (siginfo->si_code == FPE_FLTDIV)
                return "floating point division by zero";
            if (siginfo->si_code == FPE_INTOVF)
                return "integer overflow";
            return "arithmetic error";
        case SIGILL:
            return "illegal instruction";
        }
        return nullptr;
    }

    void handler(int sig, siginfo_t *siginfo, void *) {
        // a second fault while the report is printing kills the process instead of
        // re-entering the handler
        for (const auto &s : fatal_signals)
            signal(s.number, SIG_DFL);

        // the first two frames are this handler and the signal trampoline
        const int skip = 2;
        void *addresses[max_crash_frames + skip];
        const int size = backtrace(addresses, max_crash_frames + skip);

        char thread_name[64] = "";
        pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));

        CrashInfo info;
        info.signal = sig;
        for (const auto &s : fatal_signals) {
            if (s.number == sig)
                info.signal_name = s.name;
        }
        info.fault_description = fault_description(siginfo);
        if ((sig == SIGSEGV || sig == SIGBUS) && siginfo->si_code > 0) {
            info.has_fault_address = true;
            info.fault_address     = siginfo->si_addr;
        }
        info.process_id  = static_cast<unsigned long long>(getpid());
        info.thread_id   = current_thread_id();
        info.thread_name = thread_name;

        CrashFrame frames[max_crash_frames];
        int frame_count = 0;
        for (int i = skip; i < size; ++i) {
            CrashFrame &frame = frames[frame_count++];
            frame.address     = addresses[i];

            Dl_info dl;
            if (dladdr(addresses[i], &dl) && dl.dli_fname) {
                const auto address  = static_cast<const char *>(addresses[i]);
                frame.module        = dl.dli_fname;
                frame.module_offset = static_cast<unsigned long long>(
                    address - static_cast<const char *>(dl.dli_fbase));
                if (dl.dli_sname) {
                    frame.symbol        = dl.dli_sname;
                    frame.symbol_offset = static_cast<unsigned long long>(
                        address - static_cast<const char *>(dl.dli_saddr));
                }
            }
        }

        write_crash_report(info, frames, frame_count);

        // not exit(): the normal shutdown does not survive in a process that has
        // crashed, it ends in an abort and a core dump
        _exit(1);
    }

} // namespace

void install_fatal_signal_handlers() {
    // the first call to backtrace can load a library, which allocates memory, so
    // it is made here and not in the handler
    void *address = nullptr;
    backtrace(&address, 1);

    struct sigaction action;
    action.sa_sigaction = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    for (const auto &s : fatal_signals)
        sigaction(s.number, &action, nullptr);
}

void describe_system(char *host, size_t host_size, char *os, size_t os_size) {
    host[0] = 0;
    os[0]   = 0;
    struct utsname name;
    if (uname(&name) == 0) {
        snprintf(host, host_size, "%s", name.nodename);
        snprintf(os, os_size, "%s %s %s", name.sysname, name.release, name.machine);
    }
}

unsigned long long resident_memory_kb() {
#ifdef __linux__
    // /proc/self/statm: size resident shared text lib data dt, in pages
    const int fd = open("/proc/self/statm", O_RDONLY);
    if (fd < 0)
        return 0;
    char text[128]  = "";
    const auto size = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (size <= 0)
        return 0;
    unsigned long long pages = 0;
    const char *c            = text;
    while (*c && *c != ' ')
        ++c; // skip the first field
    for (++c; *c >= '0' && *c <= '9'; ++c)
        pages = pages * 10 + static_cast<unsigned long long>(*c - '0');
    return pages * static_cast<unsigned long long>(sysconf(_SC_PAGESIZE)) / 1024;
#else
    // UNTESTED, written for MacOS but never compiled there. Enable once it has
    // been built on a Mac; it needs #include <mach/mach.h> at the top.
    //
    // mach_task_basic_info task;
    // mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    // if (task_info(
    //         mach_task_self(),
    //         MACH_TASK_BASIC_INFO,
    //         reinterpret_cast<task_info_t>(&task),
    //         &count) == KERN_SUCCESS)
    //     return static_cast<unsigned long long>(task.resident_size) / 1024;
    return 0;
#endif
}

int open_crash_output(const char *path) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

void write_crash_output(int output, const char *text, size_t size) {
    while (size > 0) {
        const ssize_t written = write(output, text, size);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return;
        text += written;
        size -= static_cast<size_t>(written);
    }
}

void close_crash_output(int output) { close(output); }

} // namespace xstudio::launch
