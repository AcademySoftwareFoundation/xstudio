// EXR Sequence Loading Benchmark for xSTUDIO
// Standalone binary - no CAF/xSTUDIO dependencies
// Mirrors the reading approach in src/plugin/media_reader/openexr/src/openexr.cpp

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <new>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <Imath/ImathBox.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>
#include <ImfThreading.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start_;
    Timer() : start_(clock::now()) {}
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - start_).count();
    }
};

// ---------------------------------------------------------------------------
// Memory info (Windows)
// ---------------------------------------------------------------------------
struct MemInfo {
    size_t working_set_mb = 0;
    size_t peak_working_set_mb = 0;
};

MemInfo get_mem_info() {
    MemInfo m;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m.working_set_mb = pmc.WorkingSetSize / (1024 * 1024);
        m.peak_working_set_mb = pmc.PeakWorkingSetSize / (1024 * 1024);
    }
#endif
    return m;
}

// ---------------------------------------------------------------------------
// Aligned buffer (mirrors xSTUDIO's Buffer with 1024-byte alignment)
// ---------------------------------------------------------------------------
struct AlignedBuffer {
    void* data = nullptr;
    size_t size = 0;

    void allocate(size_t sz) {
        free();
        data = operator new[](sz, std::align_val_t(1024));
        size = sz;
    }
    void free() {
        if (data) {
            operator delete[](data, std::align_val_t(1024));
            data = nullptr;
            size = 0;
        }
    }
    ~AlignedBuffer() { free(); }
    AlignedBuffer() = default;
    AlignedBuffer(AlignedBuffer&& o) noexcept : data(o.data), size(o.size) {
        o.data = nullptr; o.size = 0;
    }
    AlignedBuffer& operator=(AlignedBuffer&& o) noexcept {
        free(); data = o.data; size = o.size; o.data = nullptr; o.size = 0; return *this;
    }
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
};

// ---------------------------------------------------------------------------
// Frame result
// ---------------------------------------------------------------------------
struct FrameResult {
    int frame_number = 0;
    double read_ms = 0.0;
    size_t file_size = 0;
    size_t decoded_size = 0;
    int width = 0;
    int height = 0;
    int num_channels = 0;
    bool success = false;
    std::string error;
};

// ---------------------------------------------------------------------------
// EXR Reader - mirrors xSTUDIO's OpenEXRMediaReader::image()
// ---------------------------------------------------------------------------
constexpr int EXR_READ_BLOCK_HEIGHT = 256;

FrameResult read_exr_frame_imf(const fs::path& filepath) {
    FrameResult result;
    result.frame_number = 0; // caller sets this

    try {
        result.file_size = fs::file_size(filepath);
    } catch (...) {
        result.file_size = 0;
    }

    Timer t;
    try {
        Imf::MultiPartInputFile input(filepath.string().c_str());
        int part_idx = 0; // use first part

        const Imf::Header& header = input.header(part_idx);
        Imf::InputPart in(input, part_idx);

        Imath::Box2i data_window = header.dataWindow();
        Imath::Box2i display_window = header.displayWindow();

        // Find RGBA channels (same logic as xSTUDIO)
        const auto& channels = header.channels();
        struct ChanInfo { std::string name; Imf::PixelType type; };
        std::vector<ChanInfo> rgba_chans;

        // Look for standard RGBA channel names
        auto try_add = [&](const char* name) {
            auto it = channels.findChannel(name);
            if (it) {
                rgba_chans.push_back({name, it->type});
                return true;
            }
            return false;
        };

        // Try common naming conventions
        if (!try_add("R")) try_add("r");
        if (!try_add("G")) try_add("g");
        if (!try_add("B")) try_add("b");
        if (!try_add("A")) try_add("a");

        // Fallback: if no RGBA found, take first 4 channels
        if (rgba_chans.empty()) {
            for (auto it = channels.begin(); it != channels.end() && rgba_chans.size() < 4; ++it) {
                rgba_chans.push_back({it.name(), it.channel().type});
            }
        }

        if (rgba_chans.empty()) {
            result.error = "No channels found";
            result.read_ms = t.elapsed_ms();
            return result;
        }

        // Compute buffer size (same as xSTUDIO openexr.cpp:283-302)
        const size_t width = data_window.size().x + 1;
        const size_t height = data_window.size().y + 1;
        size_t bytes_per_pixel = 0;
        for (const auto& ch : rgba_chans) {
            bytes_per_pixel += (ch.type == Imf::PixelType::HALF) ? 2 : 4;
        }
        const size_t buf_size = width * height * bytes_per_pixel;
        const size_t line_stride = width * bytes_per_pixel;

        AlignedBuffer buf;
        buf.allocate(buf_size);

        // Set up framebuffer - same approach as xSTUDIO non-cropped path (lines 413-447)
        char* base = static_cast<char*>(buf.data)
                     - data_window.min.x * bytes_per_pixel
                     - data_window.min.y * line_stride;

        Imf::FrameBuffer fb;
        char* chan_ptr = base;
        for (const auto& ch : rgba_chans) {
            fb.insert(
                ch.name.c_str(),
                Imf::Slice(ch.type, chan_ptr, bytes_per_pixel, line_stride, 1, 1, 0));
            chan_ptr += (ch.type == Imf::PixelType::HALF) ? 2 : 4;
        }

        in.setFrameBuffer(fb);
        in.readPixels(data_window.min.y, data_window.max.y);

        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.num_channels = static_cast<int>(rgba_chans.size());
        result.decoded_size = buf_size;
        result.success = true;

    } catch (const std::exception& e) {
        result.error = e.what();
    }

    result.read_ms = t.elapsed_ms();
    return result;
}

// ---------------------------------------------------------------------------
// Collect sequence files
// ---------------------------------------------------------------------------
std::vector<fs::path> collect_exr_sequence(const fs::path& dir) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".exr") {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
struct Stats {
    double min_ms = 0, max_ms = 0, mean_ms = 0, median_ms = 0, p95_ms = 0, p99_ms = 0;
    double total_ms = 0;
    double fps = 0;
    double throughput_mbps = 0; // decoded MB/s
    double io_mbps = 0;        // compressed MB/s
    size_t total_decoded = 0;
    size_t total_file_size = 0;
    int frame_count = 0;
};

Stats compute_stats(const std::vector<FrameResult>& results) {
    Stats s;
    if (results.empty()) return s;

    std::vector<double> times;
    for (const auto& r : results) {
        if (r.success) {
            times.push_back(r.read_ms);
            s.total_decoded += r.decoded_size;
            s.total_file_size += r.file_size;
        }
    }
    s.frame_count = static_cast<int>(times.size());
    if (times.empty()) return s;

    std::sort(times.begin(), times.end());
    s.min_ms = times.front();
    s.max_ms = times.back();
    s.median_ms = times[times.size() / 2];
    s.p95_ms = times[static_cast<size_t>(times.size() * 0.95)];
    s.p99_ms = times[std::min(static_cast<size_t>(times.size() * 0.99), times.size() - 1)];
    s.mean_ms = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    s.total_ms = std::accumulate(times.begin(), times.end(), 0.0);

    double total_s = s.total_ms / 1000.0;
    s.fps = (total_s > 0) ? s.frame_count / total_s : 0;
    s.throughput_mbps = (total_s > 0) ? (s.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
    s.io_mbps = (total_s > 0) ? (s.total_file_size / (1024.0 * 1024.0)) / total_s : 0;

    return s;
}

// ---------------------------------------------------------------------------
// Benchmark: Sequential read
// ---------------------------------------------------------------------------
std::vector<FrameResult> bench_sequential(const std::vector<fs::path>& files) {
    std::vector<FrameResult> results;
    results.reserve(files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        auto r = read_exr_frame_imf(files[i]);
        r.frame_number = static_cast<int>(i);
        results.push_back(std::move(r));
    }
    return results;
}

// ---------------------------------------------------------------------------
// Benchmark: Thread pool read
// ---------------------------------------------------------------------------
std::vector<FrameResult> bench_threaded(const std::vector<fs::path>& files, int num_threads) {
    std::vector<FrameResult> results(files.size());
    std::atomic<size_t> next_idx{0};

    auto worker = [&]() {
        while (true) {
            size_t idx = next_idx.fetch_add(1);
            if (idx >= files.size()) break;
            auto r = read_exr_frame_imf(files[idx]);
            r.frame_number = static_cast<int>(idx);
            results[idx] = std::move(r);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }
    return results;
}

// ---------------------------------------------------------------------------
// Benchmark: Pre-allocated buffer pool
// ---------------------------------------------------------------------------
std::vector<FrameResult> bench_threaded_pooled(const std::vector<fs::path>& files, int num_threads) {
    // Pre-read one frame to get buffer size
    auto probe = read_exr_frame_imf(files[0]);
    if (!probe.success) return {probe};
    size_t frame_buf_size = probe.decoded_size;

    // Pre-allocate buffers for all threads
    struct ThreadBuf {
        AlignedBuffer buf;
    };
    std::vector<ThreadBuf> thread_bufs(num_threads);
    for (auto& tb : thread_bufs) {
        tb.buf.allocate(frame_buf_size);
    }

    std::vector<FrameResult> results(files.size());
    std::atomic<size_t> next_idx{0};

    auto worker = [&](int thread_id) {
        while (true) {
            size_t idx = next_idx.fetch_add(1);
            if (idx >= files.size()) break;

            FrameResult result;
            result.frame_number = static_cast<int>(idx);
            try {
                result.file_size = fs::file_size(files[idx]);
            } catch (...) {}

            Timer t;
            try {
                Imf::MultiPartInputFile input(files[idx].string().c_str());
                const Imf::Header& header = input.header(0);
                Imf::InputPart in(input, 0);

                Imath::Box2i data_window = header.dataWindow();
                const auto& channels = header.channels();

                struct ChanInfo { std::string name; Imf::PixelType type; };
                std::vector<ChanInfo> rgba_chans;
                auto try_add = [&](const char* name) {
                    auto it = channels.findChannel(name);
                    if (it) { rgba_chans.push_back({name, it->type}); return true; }
                    return false;
                };
                if (!try_add("R")) try_add("r");
                if (!try_add("G")) try_add("g");
                if (!try_add("B")) try_add("b");
                if (!try_add("A")) try_add("a");
                if (rgba_chans.empty()) {
                    for (auto it = channels.begin(); it != channels.end() && rgba_chans.size() < 4; ++it)
                        rgba_chans.push_back({it.name(), it.channel().type});
                }

                size_t w = data_window.size().x + 1;
                size_t h = data_window.size().y + 1;
                size_t bpp = 0;
                for (const auto& ch : rgba_chans) bpp += (ch.type == Imf::PixelType::HALF) ? 2 : 4;
                size_t line_stride = w * bpp;

                // Use pre-allocated buffer - ensure we don't underflow
                // The base pointer trick places buffer origin at (0,0) so OpenEXR
                // can index by data_window coordinates directly
                size_t needed = w * h * bpp;
                if (needed > thread_bufs[thread_id].buf.size) {
                    thread_bufs[thread_id].buf.allocate(needed);
                }
                // Compute offset so that data_window.min maps to start of buffer
                char* buf_start = static_cast<char*>(thread_bufs[thread_id].buf.data);
                char* base = buf_start - data_window.min.x * static_cast<ptrdiff_t>(bpp)
                             - data_window.min.y * static_cast<ptrdiff_t>(line_stride);

                Imf::FrameBuffer fb;
                char* chan_ptr = base;
                for (const auto& ch : rgba_chans) {
                    fb.insert(ch.name.c_str(),
                        Imf::Slice(ch.type, chan_ptr, bpp, line_stride, 1, 1, 0));
                    chan_ptr += (ch.type == Imf::PixelType::HALF) ? 2 : 4;
                }

                in.setFrameBuffer(fb);
                in.readPixels(data_window.min.y, data_window.max.y);

                result.width = static_cast<int>(w);
                result.height = static_cast<int>(h);
                result.num_channels = static_cast<int>(rgba_chans.size());
                result.decoded_size = w * h * bpp;
                result.success = true;
            } catch (const std::exception& e) {
                result.error = e.what();
            }
            result.read_ms = t.elapsed_ms();
            results[idx] = std::move(result);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& th : threads) {
        th.join();
    }
    return results;
}

// ---------------------------------------------------------------------------
// Print helpers
// ---------------------------------------------------------------------------
void print_separator() {
    std::cout << std::string(120, '-') << "\n";
}

void print_header() {
    printf("%-30s %7s %7s %9s %7s %8s %8s %8s %8s %8s %8s\n",
        "Strategy", "ExtThr", "ExrThr", "Total(s)", "FPS", "Dec MB/s", "IO MB/s", "Min(ms)", "Med(ms)", "P95(ms)", "PeakMem");
    print_separator();
}

void print_result(const std::string& name, int ext_threads, int exr_threads, const Stats& s) {
    auto mem = get_mem_info();
    printf("%-30s %7d %7d %9.3f %7.1f %8.1f %8.1f %8.1f %8.1f %8.1f %6lluMB\n",
        name.c_str(), ext_threads, exr_threads,
        s.total_ms / 1000.0, s.fps, s.throughput_mbps, s.io_mbps,
        s.min_ms, s.median_ms, s.p95_ms,
        (unsigned long long)mem.peak_working_set_mb);
}

void print_frame_details(const std::vector<FrameResult>& results) {
    int errors = 0;
    for (const auto& r : results) {
        if (!r.success) {
            errors++;
            fprintf(stderr, "  Frame %4d: ERROR - %s\n", r.frame_number, r.error.c_str());
        }
    }
    if (errors > 0) {
        fprintf(stderr, "  %d errors out of %zu frames\n", errors, results.size());
    }
}

// ---------------------------------------------------------------------------
// Benchmark: Simulate xSTUDIO precache pipeline
// Models the effect of Fix 2 (parallel precache: 1 vs N in-flight)
// In xSTUDIO, the precache pipeline serializes frame reads per playhead.
// Fix 2 changes this from 1 in-flight to max_in_flight concurrent reads.
// ---------------------------------------------------------------------------
std::vector<FrameResult> bench_precache_sim(const std::vector<fs::path>& files,
                                             int max_in_flight, int exr_threads) {
    // Set EXR thread count before any reads
    Imf::setGlobalThreadCount(exr_threads);

    if (max_in_flight <= 1) {
        // Serialized: read one frame at a time (old xSTUDIO behavior)
        return bench_sequential(files);
    }

    // Simulate the precache pipeline: dispatch up to max_in_flight frames at a time
    // This models xSTUDIO's 5 ReaderHelper actors with max_in_flight cap per playhead
    std::vector<FrameResult> results(files.size());
    std::atomic<size_t> next_idx{0};

    auto worker = [&]() {
        while (true) {
            size_t idx = next_idx.fetch_add(1);
            if (idx >= files.size()) break;
            auto r = read_exr_frame_imf(files[idx]);
            r.frame_number = static_cast<int>(idx);
            results[idx] = std::move(r);
        }
    };

    // Use max_in_flight threads (simulating concurrent reader actors)
    std::vector<std::thread> threads;
    for (int i = 0; i < max_in_flight; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }
    return results;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Force line-buffered stdout for real-time output
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    std::string exr_dir;
    int iterations = 1;
    bool verbose = false;
    bool fix_bench = false;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            exr_dir = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = std::stoi(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--fix-bench") {
            fix_bench = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: exr_benchmark --dir <path_to_exr_sequence> [options]\n"
                      << "Options:\n"
                      << "  --dir <path>       Path to directory containing EXR sequence\n"
                      << "  --iterations <n>   Number of iterations per test (default: 1)\n"
                      << "  --fix-bench        Run per-fix comparison benchmarks\n"
                      << "  --verbose, -v      Show per-frame details\n"
                      << "  --help, -h         Show this help\n";
            return 0;
        } else if (exr_dir.empty()) {
            exr_dir = arg;
        }
    }

    if (exr_dir.empty()) {
        // Default test path
        exr_dir = "L:/tdm/shots/fw/9119/comp/images/FW9119_comp_v001/exr";
    }

    fs::path dir(exr_dir);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        std::cerr << "Error: Directory does not exist: " << exr_dir << "\n";
        return 1;
    }

    auto files = collect_exr_sequence(dir);
    if (files.empty()) {
        std::cerr << "Error: No EXR files found in: " << exr_dir << "\n";
        return 1;
    }

    // Probe first frame for info
    auto probe = read_exr_frame_imf(files[0]);
    if (!probe.success) {
        std::cerr << "Error: Failed to read first frame: " << probe.error << "\n";
        return 1;
    }

    // Compute total file sizes
    size_t total_file_size = 0;
    for (const auto& f : files) {
        try { total_file_size += fs::file_size(f); } catch (...) {}
    }

    std::cout << "\n";
    std::cout << "EXR Sequence Loading Benchmark\n";
    std::cout << "==============================\n";
    printf("Directory:   %s\n", exr_dir.c_str());
    printf("Frames:      %zu (%s - %s)\n",
        files.size(), files.front().filename().string().c_str(), files.back().filename().string().c_str());
    printf("Resolution:  %dx%d, %d channels\n",
        probe.width, probe.height, probe.num_channels);
    printf("Frame size:  %.1f MB decoded, %.1f MB avg compressed\n",
        probe.decoded_size / (1024.0 * 1024.0),
        (total_file_size / files.size()) / (1024.0 * 1024.0));
    printf("Total data:  %.1f MB decoded, %.1f MB compressed\n",
        (probe.decoded_size * files.size()) / (1024.0 * 1024.0),
        total_file_size / (1024.0 * 1024.0));
    printf("HW threads:  %u\n", std::thread::hardware_concurrency());
    printf("Iterations:  %d\n", iterations);
    std::cout << "\n";

    // ===================================================================
    // Per-fix benchmark mode
    // ===================================================================
    if (fix_bench) {
        unsigned hw = std::thread::hardware_concurrency();
        int dynamic_exr = std::clamp(static_cast<int>(hw) / 2, 1, 16);

        std::cout << "\n";
        std::cout << "Per-Fix Benchmark Comparison\n";
        std::cout << "============================\n";
        printf("HW threads: %u, Dynamic EXR threads (hw/2): %d\n\n", hw, dynamic_exr);

        // ---------------------------------------------------------------
        // Fix 3: Dynamic EXR thread count (was hardcoded to 16)
        // ---------------------------------------------------------------
        std::cout << "=== Fix 3: Dynamic EXR Thread Count ===\n";
        std::cout << "Compares hardcoded exr_threads=16 vs dynamic hw/2\n\n";
        print_header();

        // Old behavior: hardcoded 16
        {
            Imf::setGlobalThreadCount(16);
            Timer wall;
            auto results = bench_sequential(files);
            auto stats = compute_stats(results);
            print_result("OLD: exr_threads=16", 1, 16, stats);
        }

        // New behavior: dynamic hw/2
        {
            Imf::setGlobalThreadCount(dynamic_exr);
            Timer wall;
            auto results = bench_sequential(files);
            auto stats = compute_stats(results);
            print_result("NEW: exr_threads=hw/2", 1, dynamic_exr, stats);
        }

        // For comparison: optimal exr thread count (4 from baseline data)
        {
            Imf::setGlobalThreadCount(4);
            Timer wall;
            auto results = bench_sequential(files);
            auto stats = compute_stats(results);
            print_result("REF: exr_threads=4", 1, 4, stats);
        }

        print_separator();
        std::cout << "\n";

        // ---------------------------------------------------------------
        // Fix 2: Parallel precache (1 in-flight vs 4 in-flight)
        // ---------------------------------------------------------------
        std::cout << "=== Fix 2: Parallel Precache Pipeline ===\n";
        std::cout << "Simulates xSTUDIO precache: 1 vs 4 concurrent reads per playhead\n";
        printf("Using exr_threads=%d (from Fix 3)\n\n", dynamic_exr);
        print_header();

        // Old behavior: 1 in-flight (serialized precache)
        for (int exr_thr : {0, dynamic_exr}) {
            Timer wall;
            auto results = bench_precache_sim(files, 1, exr_thr);
            double wall_ms = wall.elapsed_ms();
            auto stats = compute_stats(results);
            stats.total_ms = wall_ms;
            double total_s = wall_ms / 1000.0;
            stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
            stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
            stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;
            print_result("OLD: 1 in-flight", 1, exr_thr, stats);
        }

        // New behavior: 4 in-flight (uses 4 threads with 0 EXR internal threads)
        {
            Timer wall;
            auto results = bench_precache_sim(files, 4, 0);
            double wall_ms = wall.elapsed_ms();
            auto stats = compute_stats(results);
            stats.total_ms = wall_ms;
            double total_s = wall_ms / 1000.0;
            stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
            stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
            stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;
            print_result("NEW: 4 in-flight", 4, 0, stats);
        }

        // Aggressive: 8 in-flight
        {
            Timer wall;
            auto results = bench_precache_sim(files, 8, 0);
            double wall_ms = wall.elapsed_ms();
            auto stats = compute_stats(results);
            stats.total_ms = wall_ms;
            double total_s = wall_ms / 1000.0;
            stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
            stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
            stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;
            print_result("AGG: 8 in-flight", 8, 0, stats);
        }

        print_separator();
        std::cout << "\n";

        // ---------------------------------------------------------------
        // Combined: All fixes together (optimal configuration)
        // ---------------------------------------------------------------
        std::cout << "=== Combined: All Fixes Applied ===\n";
        std::cout << "Fix 1: Precache stall recovery (architectural - not benchmarkable standalone)\n";
        std::cout << "Fix 2: 4 concurrent reads per playhead\n";
        printf("Fix 3: Dynamic EXR threads = %d\n\n", dynamic_exr);
        print_header();

        // Best config: pooled buffers, 4 in-flight, dynamic EXR threads
        for (int in_flight : {1, 2, 4, 8}) {
            Imf::setGlobalThreadCount(0);  // 0 exr threads = best with ext threading
            Timer wall;
            auto results = bench_precache_sim(files, in_flight, 0);
            double wall_ms = wall.elapsed_ms();
            auto stats = compute_stats(results);
            stats.total_ms = wall_ms;
            double total_s = wall_ms / 1000.0;
            stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
            stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
            stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;

            char label[64];
            snprintf(label, sizeof(label), "Combined: %d in-flight", in_flight);
            print_result(label, in_flight, 0, stats);
        }

        print_separator();
        std::cout << "\n";

        // ---------------------------------------------------------------
        // Thread Sweep: find optimal EXR internal threads for each ext count
        // ---------------------------------------------------------------
        std::cout << "=== Thread Sweep: ext_threads x exr_threads ===\n";
        std::cout << "Finding optimal EXR internal thread count for each concurrency level\n\n";
        print_header();

        for (int ext : {1, 2, 4, 8}) {
            for (int exr : {0, 1, 2, 4, 8, 16}) {
                Timer wall;
                auto results = bench_precache_sim(files, ext, exr);
                double wall_ms = wall.elapsed_ms();
                auto stats = compute_stats(results);
                stats.total_ms = wall_ms;
                double total_s = wall_ms / 1000.0;
                stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
                stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
                stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;

                char label[64];
                snprintf(label, sizeof(label), "Sweep: ext=%d exr=%d", ext, exr);
                print_result(label, ext, exr, stats);
            }
            print_separator();
        }

        auto final_mem = get_mem_info();
        printf("\nPeak memory: %llu MB\n", (unsigned long long)final_mem.peak_working_set_mb);
        std::cout << "Done.\n";
        return 0;
    }

    // ===================================================================
    // Full benchmark mode (default)
    // ===================================================================

    // Thread counts and EXR internal thread counts to test
    std::vector<int> ext_thread_counts = {1, 2, 4, 8};
    std::vector<int> exr_thread_counts = {0, 1, 4, 8, 16};

    // Remove thread counts that exceed hardware
    unsigned hw = std::thread::hardware_concurrency();
    ext_thread_counts.erase(
        std::remove_if(ext_thread_counts.begin(), ext_thread_counts.end(),
            [hw](int n) { return static_cast<unsigned>(n) > hw; }),
        ext_thread_counts.end());

    for (int iter = 0; iter < iterations; ++iter) {
        if (iterations > 1) {
            printf("\n=== Iteration %d / %d ===\n\n", iter + 1, iterations);
        }

        print_header();

        // --- Test 1: Sequential, varying EXR internal threads ---
        for (int exr_thr : exr_thread_counts) {
            Imf::setGlobalThreadCount(exr_thr);

            Timer wall;
            auto results = bench_sequential(files);
            double wall_ms = wall.elapsed_ms();

            auto stats = compute_stats(results);
            // For sequential, wall time == sum of frame times (approximately)
            print_result("Sequential", 1, exr_thr, stats);

            if (verbose) print_frame_details(results);
        }

        print_separator();

        // --- Test 2: Threaded (new alloc per frame), varying ext threads x EXR threads ---
        for (int ext_thr : ext_thread_counts) {
            if (ext_thr <= 1) continue; // already tested sequential

            for (int exr_thr : {0, 4, 8, 16}) {
                Imf::setGlobalThreadCount(exr_thr);

                Timer wall;
                auto results = bench_threaded(files, ext_thr);
                double wall_ms = wall.elapsed_ms();

                // For threaded, use wall time for throughput (frames run in parallel)
                auto stats = compute_stats(results);
                // Override total_ms with wall time for accurate FPS
                double total_s = wall_ms / 1000.0;
                stats.total_ms = wall_ms;
                stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
                stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
                stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;

                print_result("Threaded (fresh alloc)", ext_thr, exr_thr, stats);

                if (verbose) print_frame_details(results);
            }
        }

        print_separator();

        // --- Test 3: Threaded with pre-allocated buffer pool ---
        for (int ext_thr : ext_thread_counts) {
            if (ext_thr <= 1) continue;

            for (int exr_thr : {0, 4, 16}) {
                Imf::setGlobalThreadCount(exr_thr);

                Timer wall;
                auto results = bench_threaded_pooled(files, ext_thr);
                double wall_ms = wall.elapsed_ms();

                auto stats = compute_stats(results);
                double total_s = wall_ms / 1000.0;
                stats.total_ms = wall_ms;
                stats.fps = (total_s > 0) ? stats.frame_count / total_s : 0;
                stats.throughput_mbps = (total_s > 0) ? (stats.total_decoded / (1024.0 * 1024.0)) / total_s : 0;
                stats.io_mbps = (total_s > 0) ? (stats.total_file_size / (1024.0 * 1024.0)) / total_s : 0;

                print_result("Threaded (pooled buf)", ext_thr, exr_thr, stats);

                if (verbose) print_frame_details(results);
            }
        }

        print_separator();
    }

    auto final_mem = get_mem_info();
    printf("\nPeak memory: %llu MB\n", (unsigned long long)final_mem.peak_working_set_mb);
    std::cout << "Done.\n";

    return 0;
}
