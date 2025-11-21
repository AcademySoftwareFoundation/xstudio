// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_config.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>
#include <fstream>
#include "xstudio/utility/json_store.hpp"
#include "xstudio/media_reader/image_buffer.hpp"

namespace xstudio {

namespace video_render_plugin_1_0 {

    inline void unpack_json_array(utility::JsonStore &json) {
        if (not json.empty()) {
            throw std::runtime_error(
                "unpack_json_array, array has more elements than expected");
        }
    }

    template <typename T, typename... Args>
    void unpack_json_array(utility::JsonStore &json, T &f, Args &...args) {
        if (!json.is_array()) {
            throw std::runtime_error(
                fmt::format("unpack_json_array, json is not an array: {}", json.dump()));
        } else if (json.empty()) {
            throw std::runtime_error(
                "unpack_json_array, array has fewer elements than expected");
        }
        f = json[0].get<T>();
        json.erase(json.begin()); // pop-front
        if (json.size())
            unpack_json_array(json, args...);
    }

    template <typename... Args>
    void unpack_json_array(const utility::JsonStore &json, Args &...args) {

        utility::JsonStore data_cp = json;
        unpack_json_array(data_cp, args...);
    }

    class VideoRenderWorker : public caf::event_based_actor {

      public:
        VideoRenderWorker(
            caf::actor_config &cfg,
            const utility::Uuid &job_id,
            const utility::JsonStore &render_options,
            caf::actor offscreen_viewport,
            caf::actor colour_pipeline,
            caf::actor renderer_plugin);

        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      protected:
        enum RenderStatus { Queued, InProgress, Failed, Complete, DontChange };

        void update_status(
            const std::string &status_string, const RenderStatus render_status = DontChange);

        void encode_frame(const media_reader::ImageBufPtr &image);
        void encode_audio(const media_reader::AudioBufPtr &audio);

        void start_render_task();
        void continue_render_loop();
        void start_ffmpeg_process();
        void set_colour_params_and_start();
        void stop_ffmpeg_process();
        void render_step();
        void add_output_to_session();

        void clone_render_target(
            const utility::Uuid &parent_playlist_item_id,
            const utility::Uuid &target_render_item_id);

        std::string output_file_path_;
        utility::FrameRate rate_;
        Imath::V2i resolution_;
        int in_point_  = {-1};
        int out_point_ = {-1};
        std::string render_item_name_;
        std::string video_codec_opts_;
        std::string video_render_bit_depth_;
        std::string audio_codec_opts_;
        std::string video_preset_name_;
        std::string ocio_display_;
        std::string ocio_view_;
        bool auto_check_output_;
        ui::viewport::ImageFormat render_format_;

        caf::actor offscreen_viewport_;
        caf::actor renderable_;
        const utility::Uuid job_uuid_;
        caf::actor renderer_plugin_;

        caf::actor playhead_, colour_pipeline_;
        caf::actor video_out_pipe_, audio_out_pipe_;
        caf::behavior behavior_;

        std::string output_yuv_filename_, output_audio_filename_;
        std::string ffmpeg_stdout_;

        const std::string renderer_plugin_actor_address_;
        timebase::flicks playhead_position_, end_position_;
        int audio_bufs_in_flight_            = {0};
        int video_bufs_in_flight_            = {0};
        const int max_frames_in_flight_      = 32;
        bool ffmpeg_process_running_         = {false};
        bool waiting_for_buffers_            = {false};
        bool completing_                     = {false};
        int soundcard_sample_rate_           = {48000};
        int percent_complete_                = {-1};
        int64_t num_audio_samples_delivered_ = {0};
        RenderStatus status_                 = {Queued};
    };

} // namespace video_render_plugin_1_0
} // namespace xstudio