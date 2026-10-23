// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>

// The contract between the generic crash report code (crash_report.cpp) and the
// platform code (platform/<name>/crash_handler.cpp). The platform code catches
// the fatal signal and gathers the facts, the generic code writes the report.

namespace xstudio::launch {

const int max_crash_frames = 32;

// What the platform knows about the crash. Anything it cannot provide is left
// at its default and is left out of the report.
struct CrashInfo {
    int signal                    = 0;
    const char *signal_name       = nullptr;
    const char *fault_description = nullptr; // e.g. "address not mapped"
    bool has_fault_address        = false;
    const void *fault_address     = nullptr;
    unsigned long long process_id = 0;
    unsigned long long thread_id  = 0;
    const char *thread_name       = nullptr;
};

// One stack frame. module and symbol are null when they are not known.
struct CrashFrame {
    const void *address              = nullptr;
    const char *module               = nullptr; // full path
    unsigned long long module_offset = 0;
    const char *symbol               = nullptr; // as stored in the module
    unsigned long long symbol_offset = 0;
};

// Generic. Called by the platform signal handler. Must not allocate memory.
void write_crash_report(const CrashInfo &info, const CrashFrame *frames, int frame_count);

// Implemented once per platform.

void install_fatal_signal_handlers();

// Called at startup, may allocate.
void describe_system(char *host, size_t host_size, char *os, size_t os_size);

// Called from the handler, must not allocate. Returns 0 when unknown.
unsigned long long resident_memory_kb();

// Open a file for writing, returns a negative value on failure. Must not
// allocate memory.
int open_crash_output(const char *path);
void write_crash_output(int output, const char *text, size_t size);
void close_crash_output(int output);

} // namespace xstudio::launch
