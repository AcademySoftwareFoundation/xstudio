// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <mutex>
#include <atomic>
#include <deque>
#include <string>
#include <vector>

#include "extern/decklink_compat.h"
#include "extern/DeckLinkAPI.h"

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#include "xstudio/media_reader/image_buffer.hpp"
#include "pixel_swizzler.hpp"

namespace xstudio {
namespace bm_decklink_plugin_1_0 {

    class AVOutputCallback;

    struct HDRMetadata {
        int64_t EOTF                             = {0};
        int32_t colourspace_                     = {bmdColorspaceRec709};
        std::array<double, 8> referencePrimaries = {
            0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290};
        std::array<double, 4> luminanceSettings = {
            1000.0, // HDRMaxDisplayMasteringLuminance
            0.0005, // HDRMinDisplayMasteringLuminance
            1000.0, // HDRMaximumContentLightLevel
            400.0   // HDRMaximumFrameAverageLightLevel
        };
    };

    class BMDecklinkPlugin;

    class MockDecklinkOutput {

      public:
        MockDecklinkOutput(BMDecklinkPlugin *decklink_xstudio_plugin) {}

        bool init_decklink() { return true; }

        bool start_sdi_output() { return true; }
        void set_preroll() {}
        bool stop_sdi_output(const std::string &error = std::string()) { return true; }
        void StartStop() {}

        void fill_decklink_video_frame(IDeckLinkVideoFrame *decklink_video_frame) {}
        void copy_audio_samples_to_decklink_buffer(const bool preroll) {}
        void receive_samples_from_xstudio(int16_t *samples, unsigned long num_samps) {}
        long num_samples_in_buffer() { return 0; }
        void set_display_mode(
            const std::string &resolution,
            const std::string &refresh_rate,
            const BMDPixelFormat pix_format) {}
        void set_audio_samples_water_level(const int w) {}
        void set_audio_sync_delay_milliseconds(const long ms_delay) {}

        void incoming_frame(const media_reader::ImageBufPtr &frame) {}

        [[nodiscard]] int frameWidth() const { return static_cast<int>(1920); }
        [[nodiscard]] int frameHeight() const { return static_cast<int>(1080); }

        std::vector<std::string>
        get_available_refresh_rates(const std::string &output_resolution) const {
            return std::vector<std::string>(
                {"23.976", "24", "25", "29.97", "30", "50", "59.94", "60"});
        }

        std::vector<std::string> output_resolution_names() const {
            return std::vector<std::string>({"1920x1080", "3840x2160"});
        }

        void set_hdr_metadata(const HDRMetadata &) {}
    };

    class DecklinkOutput {

      public:
        DecklinkOutput(BMDecklinkPlugin *decklink_xstudio_plugin);
        ~DecklinkOutput();

        static void check_decklink_installation();

        bool init_decklink();
        void make_intermediate_frame();
        bool start_sdi_output();
        void set_preroll();
        bool stop_sdi_output(const std::string &error = std::string());
        void StartStop();

        void fill_decklink_video_frame(IDeckLinkVideoFrame *decklink_video_frame);
        void update_frame_metadata(IDeckLinkMutableVideoFrame *displayFrame);
        void copy_audio_samples_to_decklink_buffer(const bool preroll);
        void receive_samples_from_xstudio(int16_t *samples, unsigned long num_samps);
        long num_samples_in_buffer();
        void set_display_mode(
            const std::string &resolution,
            const std::string &refresh_rate,
            const BMDPixelFormat pix_format);
        void set_audio_samples_water_level(const int w) { samples_water_level_ = (uint32_t)w; }
        void set_audio_sync_delay_milliseconds(const long ms_delay) {
            audio_sync_delay_milliseconds_ = ms_delay;
        }

        void incoming_frame(const media_reader::ImageBufPtr &frame);

        [[nodiscard]] int frameWidth() const { return static_cast<int>(frame_width_); }
        [[nodiscard]] int frameHeight() const { return static_cast<int>(frame_height_); }

        std::vector<std::string>
        get_available_refresh_rates(const std::string &output_resolution) const;

        std::vector<std::string> output_resolution_names() const {
            std::vector<std::string> result;
            for (const auto &p : refresh_rate_per_output_resolution_) {
                result.push_back(p.first);
            }
            std::sort(result.begin(), result.end());
            return result;
        }

        void set_hdr_metadata(const HDRMetadata &o) {
            hdr_metadata_mutex_.lock();
            hdr_metadata_ = o;
            hdr_metadata_mutex_.unlock();
        }

        [[nodiscard]] bool is_available() const { return is_available_; }
        [[nodiscard]] const std::string &last_error() const { return last_error_; }
        [[nodiscard]] const std::string &runtime_info() const { return runtime_info_; }

      private:
        void release_resources();
        void detect_runtime_info();
        void log_runtime_info() const;

        AVOutputCallback *output_callback_ = {nullptr};
        std::mutex mutex_;

        GLenum glStatus = {0};
        GLuint idFrameBuf = {0}, idColorBuf = {0}, idDepthBuf = {0};
        char *pFrameBuf   = {nullptr};

        // DeckLink
        uint32_t frame_width_  = {0};
        uint32_t frame_height_ = {0};

        IDeckLink *decklink_interface_              = {nullptr};
        IDeckLinkOutput *decklink_output_interface_ = {nullptr};
        IDeckLinkVideoConversion *frame_converter_  = {nullptr};

        BMDTimeValue frame_duration_  = {0};
        BMDTimeScale frame_timescale_ = {0};
        uint32_t uiFPS                = {0};
        uint32_t uiTotalFrames        = {0};

        media_reader::ImageBufPtr current_frame_;
        std::mutex frames_mutex_;
        bool running_ = {false};

        void query_display_modes();

        void report_status(const std::string &status_message, bool is_running);

        void report_error(const std::string &status_message);

        std::map<std::string, std::vector<std::string>> refresh_rate_per_output_resolution_;
        std::map<std::pair<std::string, std::string>, BMDDisplayMode> display_modes_;

        BMDPixelFormat current_pix_format_;
        BMDDisplayMode current_display_mode_;
        std::string display_mode_name_;

        IDeckLinkMutableVideoFrame *intermediate_frame_ = {nullptr};

        BMDecklinkPlugin *decklink_xstudio_plugin_;

        std::vector<int16_t> audio_samples_buffer_;
        std::mutex audio_samples_buf_mutex_, audio_samples_cv_mutex_, bmd_mutex_;
        std::condition_variable audio_samples_cv_;
        bool fetch_more_samples_from_xstudio_ = {false};
        unsigned long samples_delivered_      = {0};
        uint32_t samples_water_level_         = {4096};
        long audio_sync_delay_milliseconds_   = {0};
        bool video_output_enabled_            = {false};
        bool audio_output_enabled_            = {false};
        bool scheduled_playback_started_      = {false};
        PixelSwizzler pixel_swizzler_;

        HDRMetadata hdr_metadata_;
        std::mutex hdr_metadata_mutex_;
        bool is_available_             = {false};
        std::string last_error_        = {};
        std::string runtime_info_      = {};
        std::string output_interface_info_ = {};
    };

    class AVOutputCallback : public IDeckLinkVideoOutputCallback,
                             public IDeckLinkAudioOutputCallback {
      private:
        struct RefCt {

            int fetchAndAddAcquire(const int delta) {

                std::lock_guard<std::mutex> l(m);
                int old = count;
                count += delta;
                return old;
            }
            std::atomic<int> count = 1;
            std::mutex m;
        } ref_count_;

        DecklinkOutput *owner_;

      public:
        AVOutputCallback(DecklinkOutput *pOwner);

        // IUnknown
        HRESULT QueryInterface(REFIID /*iid*/, LPVOID * /*ppv*/) override;
        ULONG AddRef() override;
        ULONG Release() override;

        // IDeckLinkAudioOutputCallback
        HRESULT RenderAudioSamples(BOOL preroll) override;

        // IDeckLinkVideoOutputCallback
        HRESULT ScheduledFrameCompleted(
            IDeckLinkVideoFrame *completedFrame,
            BMDOutputFrameCompletionResult result) override;
        HRESULT ScheduledPlaybackHasStopped() override;
    };

} // namespace bm_decklink_plugin_1_0
} // namespace xstudio
