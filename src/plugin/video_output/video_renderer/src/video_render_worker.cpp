// SPDX-License-Identifier: Apache-2.0

#include <filesystem>
#include <chrono>

#include <caf/actor_registry.hpp>

#include "video_render_worker.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/bookmark/bookmarks_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"

#include <ImfRgbaFile.h>
#include <vector>

#ifdef _WIN32
#include "Windows.h"
#else
#include <dlfcn.h>
#endif

using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::video_render_plugin_1_0;

namespace {

template <typename T> void flop_rgba_image(const media_reader::ImageBufPtr &image) {

    size_t line_size       = image->image_size_in_pixels().x * 4 * sizeof(T);
    uint8_t *buf           = reinterpret_cast<uint8_t *>(image->buffer());
    uint8_t *other_end_buf = buf + (image->image_size_in_pixels().y - 1) * line_size;
    std::vector<uint8_t> line(line_size);
    int y = image->image_size_in_pixels().y / 2;
    while (y--) {
        memcpy(line.data(), buf, line_size);
        memcpy(buf, other_end_buf, line_size);
        memcpy(other_end_buf, line.data(), line_size);
        buf += line_size;
        other_end_buf -= line_size;
    }
}
    
#ifdef _WIN32    

std::string last_error() {

    std::string rt;
    LPVOID lpMsgBuf = nullptr;
    DWORD dw = GetLastError(); 

    if (FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL) == 0) {
        rt = "Couldn't fetch error message";
    }

    rt = std::string((LPCSTR)lpMsgBuf);
    LocalFree(lpMsgBuf);

    return rt;
}

HANDLE open_named_pipe(std::string name) {

    std::wstring stemp = std::wstring(name.begin(), name.end());
    HANDLE result = CreateNamedPipe(
        stemp.c_str(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        2048,
        2048,
        0,
        NULL);

    if (result == INVALID_HANDLE_VALUE) {
            throw std::runtime_error(
            fmt::format(
                "{} failed to open pipe {} with error: {}",
                __PRETTY_FUNCTION__,
                name,
                last_error())
                .c_str());
    }
    return result;
}

// WINDOWS solution for streaming data out to a fifo/pipe

class RenderPipeActor : public caf::event_based_actor {

  public:
    RenderPipeActor(
        caf::actor_config &cfg,
        const utility::Uuid &uuid,
        std::string output_pipe_name,
        int bytes_per_pixel,
        HANDLE server_pipe
    )
        : caf::event_based_actor(cfg),
          output_pipe_name_(output_pipe_name),
          bytes_per_pixel_(bytes_per_pixel),
          output_file_desc_(server_pipe) {}

    void on_exit() override {
        // close the output file
        CloseHandle(output_file_desc_);
    }

    std::ofstream the_file;
    int num_images = 0;

    void write_data(media_reader::byte *data, size_t size) {

        if (ConnectNamedPipe(output_file_desc_, NULL) != 0) {
            std::wcout << "Client connected." << std::endl;
        }

        DWORD bytesWritten = 0;
        bool rt = WriteFile(output_file_desc_, reinterpret_cast<const char *>(data), size, &bytesWritten, NULL);
        if (!rt || bytesWritten != size) {
            throw std::runtime_error(
                fmt::format(
                    "{} failed to write {} bytes to file {} with error: {}",
                    __PRETTY_FUNCTION__,
                    size,
                    output_pipe_name_,
                    last_error())
                    .c_str());
        }

    }

    caf::behavior make_behavior() override {
        return {
            [=](const media_reader::ImageBufPtr &image) -> result<bool> {
                try {
                    write_data(
                        const_cast<media_reader::byte *>(image->buffer()),
                        image->image_size_in_pixels().x * image->image_size_in_pixels().y *
                            bytes_per_pixel_);
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
                return true;
            },
            [=](const media_reader::AudioBufPtr &audio_buf) -> result<bool> {
                try {
                    write_data(
                        const_cast<media_reader::byte *>(audio_buf->buffer()),
                        audio_buf->actual_sample_data_size());
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
                return true;
            },
            [=](utility::user_stop_action_atom) {

            }};
    }

    HANDLE output_file_desc_ = nullptr;
    const int bytes_per_pixel_;
    const std::string output_pipe_name_;

};

#else

// UNIX solution for streaming data out to a fifo/pipe
class RenderPipeActor : public caf::event_based_actor {

  public:
    RenderPipeActor(
        caf::actor_config &cfg,
        const utility::Uuid &uuid,
        std::string output_audio_file_path,
        int bytes_per_pixel
    )
        : caf::event_based_actor(cfg),
          output_file_path_(output_audio_file_path),
          bytes_per_pixel_(bytes_per_pixel) {}

    void on_exit() override {
        // close the output file
        if (output_file_desc_) {
            close(output_file_desc_);
            unlink(output_file_path_.c_str());
            output_file_desc_ = 0;
        }
    }

    std::ofstream the_file;
    int num_images = 0;

    void write_data(media_reader::byte *data, size_t size) {

        if (!output_file_desc_) {
            output_file_desc_ = open(output_file_path_.c_str(), O_WRONLY);
            if (!output_file_desc_) {
                throw std::runtime_error(
                    fmt::format(
                        "{} failed to open fifo with name {} for writing, errno error: {}",
                        __PRETTY_FUNCTION__,
                        output_file_path_,
                        strerror(errno))
                        .c_str());
            }
        }

        if (write(output_file_desc_, reinterpret_cast<const char *>(data), size) != size) {

            throw std::runtime_error(
                fmt::format(
                    "{} failed to write {} bytes to file {}, errno error: {}",
                    __PRETTY_FUNCTION__,
                    size,
                    output_file_path_,
                    strerror(errno))
                    .c_str());
        }
        /*if (!the_file.is_open()) {
            std::cerr << "Opening file " << output_file_path_ << "\n";
            the_file.open(output_file_path_.c_str(), std::ios::binary);
        }
        the_file.write(reinterpret_cast<char*>(data), size); // binary output*/
    }

    caf::behavior make_behavior() override {
        return {
            [=](const media_reader::ImageBufPtr &image) -> result<bool> {
                try {
                    write_data(
                        const_cast<media_reader::byte *>(image->buffer()),
                        image->image_size_in_pixels().x * image->image_size_in_pixels().y *
                            bytes_per_pixel_);
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
                return true;
            },
            [=](const media_reader::AudioBufPtr &audio_buf) -> result<bool> {
                try {
                    write_data(
                        const_cast<media_reader::byte *>(audio_buf->buffer()),
                        audio_buf->actual_sample_data_size());
                } catch (std::exception &e) {
                    return caf::make_error(xstudio_error::error, e.what());
                }
                return true;
            },
            [=](utility::user_stop_action_atom) {

            }};
    }

    const std::string output_file_path_;
    int output_file_desc_ = {0};
    const int bytes_per_pixel_;
};

#endif

} // namespace

VideoRenderWorker::VideoRenderWorker(
    caf::actor_config &cfg,
    const utility::Uuid &job_id,
    const utility::JsonStore &render_options,
    caf::actor offscreen_viewport,
    caf::actor colour_pipeline,
    caf::actor renderer_plugin)
    : caf::event_based_actor(cfg),
      job_uuid_(job_id),
      offscreen_viewport_(offscreen_viewport),
      colour_pipeline_(colour_pipeline),
      renderer_plugin_(renderer_plugin) {

#ifdef _WIN32
#else        
    // we are using named pipes (fifos) to stream image and audio data to ffmpeg.
    // If ffmpeg exits early we get a SIGPIPE signal, which would kill the app
    // if we don't do this:
    signal(SIGPIPE, SIG_IGN);
#endif

    // The render_options json array is built in the UI code, see
    // VideoRenderDialog.qml.
    // We match up the types & values to the array that is built.

    utility::Uuid parent_playlist_item_id;
    utility::Uuid target_render_item_id;
    double frame_rate;

    try {
        unpack_json_array(
            render_options,
            render_item_name_,
            parent_playlist_item_id,
            target_render_item_id,
            output_file_path_,
            resolution_,
            in_point_,
            out_point_,
            frame_rate,
            video_codec_opts_,
            video_render_bit_depth_,
            audio_codec_opts_,
            video_preset_name_,
            ocio_display_,
            ocio_view_,
            auto_check_output_);
    } catch (std::exception &e) {
        spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
        send_exit(caf::actor_cast<caf::actor>(this), caf::exit_reason::user_shutdown);
        return;
    }

    render_format_ = video_render_bit_depth_.find("16") == 0 ? viewport::ImageFormat::RGBA_16
                                                             : viewport::ImageFormat::RGBA_8;

    auto out_uri = caf::make_uri(output_file_path_);
    if (out_uri) {
        output_file_path_ = utility::uri_to_posix_path(*out_uri);
    }

    rate_ = utility::FrameRate(1.0 / frame_rate);

    try {
        auto prefs = global_store::GlobalStoreHelper(system());
        // we need to know the global soundard sample rate, which is the rate
        //
#ifdef _WIN32
        soundcard_sample_rate_ =
            prefs.value<int>("/core/audio/windows_audio_prefs/sample_rate");
#else
        soundcard_sample_rate_ = prefs.value<int>("/core/audio/pulse_audio_prefs/sample_rate");
#endif
    } catch (std::exception &e) {
        spdlog::warn("Failed to get global audio sample rate: {}", e.what());
    }

    behavior_.assign(
        [=](const utility::Uuid &parent_playlist_item_id,
            const utility::Uuid &target_render_item_id) {
            clone_render_target(parent_playlist_item_id, target_render_item_id);
        },
        [=](utility::user_start_action_atom) { start_render_task(); },
        [=](utility::event_atom,
            const utility::Uuid &job_id,
            const std::string &ffmpeg_terminal_output,
            bool linefeed) {
            // this message is sent directly from the Python component of our plugin. The python
            // plugin starts a thread that reads the stdout/stderr pipe from the ffmpeg
            // subprocess and forwards it to us here so that we can display it in the xSTUDIO UI
            if (linefeed && ffmpeg_stdout_.size() > 2) {
                auto p = ffmpeg_stdout_.rfind("\n", ffmpeg_stdout_.size() - 2);
                if (p != std::string::npos) {
                    ffmpeg_stdout_.resize(p + 1);
                }
            }
            ffmpeg_stdout_ += ffmpeg_terminal_output;
        },
        [=](utility::event_atom, const utility::Uuid &job_id, const int return_code) {
            // Also sent directly from Python plugin thread monitoring the ffmpeg subprocess.
            // We get this when the FFMPEG process exits

            ffmpeg_process_running_ = false;
            if (return_code) {
                update_status(
                    fmt::format("FFMpeg failed with code {}. Check log.", return_code), Failed);
            } else {
                update_status("Complete", Complete);
                if (auto_check_output_) {
                    add_output_to_session();
                    return;
                }
            }
            mail(utility::user_start_action_atom_v).send(renderer_plugin_);
            send_exit(caf::actor_cast<caf::actor>(this), caf::exit_reason::user_shutdown);
        },
        [=](playhead::step_atom) {
            // main render step funcition
            render_step();
        });

    update_status("Queued", Queued);

    // this will trigger an async call to clone_render_target. Calling that
    // function directly here is also possible but it might be slow to execute,
    // causing the parent plugin to pause when it spawns the VideoRenderWorker
    anon_mail(parent_playlist_item_id, target_render_item_id)
        .send(caf::actor_cast<caf::actor>(this));
}

void VideoRenderWorker::on_exit() {

    if (audio_out_pipe_)
        send_exit(audio_out_pipe_, caf::exit_reason::user_shutdown);
    if (video_out_pipe_)
        send_exit(video_out_pipe_, caf::exit_reason::user_shutdown);

    audio_out_pipe_ = caf::actor();
    video_out_pipe_ = caf::actor();

    if (ffmpeg_process_running_) {
        // try and get Python interpreter to kill the subprocess
        utility::JsonStore ffmpeg_args;
        ffmpeg_args["job_id"] = job_uuid_;
        auto python_interp =
            system().registry().template get<caf::actor>(embedded_python_registry);

        mail(
            embedded_python::python_exec_atom_v,
            "FFMpegEncoderPlugin",
            "stop_ffmpeg_encode",
            ffmpeg_args)
            .request(python_interp, infinite)
            .then(
                [=](const utility::JsonStore &) {
                    // all good.
                    if (percent_complete_ == -1) {
                        update_status("Job Cancelled");
                    }
                },
                [=](caf::error &err) {
                    update_status(
                        fmt::format(
                            "Failed to clean-up with error: {} {}",
                            __PRETTY_FUNCTION__,
                            to_string(err)),
                        Failed);
                });
    }
}

void VideoRenderWorker::update_status(
    const std::string &status_string, const RenderStatus render_status) {

    utility::JsonStore new_status;
    new_status["job_id"]           = job_uuid_;
    new_status["output_file"]      = output_file_path_;
    new_status["render_item"]      = render_item_name_;
    new_status["resolution"]       = fmt::format("{} x {}", resolution_.x, resolution_.y);
    new_status["status"]           = status_string;
    new_status["percent_complete"] = percent_complete_;
    new_status["video_preset"]     = video_preset_name_;
    new_status["visible"]          = true;

    static const std::map<RenderStatus, std::string> status_icons({
        {Queued, "qrc:/icons/pending.svg"},
        {InProgress, "qrc:/icons/directions_run.svg"},
        {Failed, "qrc:/icons/error.svg"},
        {Complete, "qrc:/icons/play_circle.svg"},
    });

    static const std::map<RenderStatus, std::string> status_colours({
        {Queued, "lightgrey"},
        {InProgress, "orange"},
        {Failed, "red"},
        {Complete, "green"},
    });

    if (render_status != DontChange) {
        status_ = render_status;
    }

    // these values are used by VideoRenderJobsQueueDialog.qml to drive the
    // status indicator
    new_status["status_icon"]   = status_icons.find(status_)->second;
    new_status["status_colour"] = status_colours.find(status_)->second;
    new_status["status_num"]    = int(render_status);

    mail(utility::event_atom_v, new_status, ffmpeg_stdout_).send(renderer_plugin_);

    if (render_status == Failed) {
        // does cleanup etc.
        send_exit(caf::actor_cast<caf::actor>(this), caf::exit_reason::user_shutdown);
    }
}

void VideoRenderWorker::start_render_task() {

    update_status("Starting", InProgress);

    mail(playlist::create_playhead_atom_v)
        .request(renderable_, infinite)
        .then(
            [=](utility::UuidActor playhead) {
                try {
                    update_status("Got playhead");

                    playhead_ = playhead.actor();

                    scoped_actor sys{system()};
                    const auto type = utility::request_receive<std::string>(
                        *sys, renderable_, utility::type_atom_v);

                    if (type == "Subset" || type == "Playlist") {
                        // switch the playhead to 'String' mode
                        utility::request_receive<bool>(
                            *sys, playhead_, playhead::compare_mode_atom_v, "String");
                    }

                    // this call, as well as getting the overall duration of the playable
                    // target, will fully intialise the playhead so that it computes all
                    // the frames in the timeline ready for playback/rendering
                    const timebase::flicks dur = utility::request_receive<timebase::flicks>(
                        *sys, playhead_, playhead::duration_flicks_atom_v, true);

                    if (in_point_ == -1) {
                        playhead_position_ = timebase::flicks(0);
                    } else {
                        playhead_position_ = utility::request_receive<timebase::flicks>(
                            *sys,
                            playhead_,
                            playhead::logical_frame_to_flicks_atom_v,
                            in_point_);
                    }

                    if (out_point_ == -1) {
                        end_position_ = dur;
                    } else {
                        end_position_ = utility::request_receive<timebase::flicks>(
                            *sys,
                            playhead_,
                            playhead::logical_frame_to_flicks_atom_v,
                            out_point_);
                    }

                    utility::request_receive<bool>(
                        *sys,
                        offscreen_viewport_,
                        viewport::viewport_playhead_atom_v,
                        playhead_);

                    if (end_position_ < playhead_position_) {
                        update_status(
                            fmt::format(
                                "Invalid render frame range {}-{}", in_point_, out_point_),
                            Failed);
                    } else {

                        // num_audio_samples_delivered_ tracks how many samples we have
                        // written to the output ... but it needs to be offset by the
                        // starting playhead_position_ so we can accurately compute
                        // how many samples we need to deliver on each frame from the
                        // change in the playhead position
                        num_audio_samples_delivered_ =
                            (std::chrono::duration_cast<std::chrono::microseconds>(
                                 playhead_position_)
                                 .count() *
                             soundcard_sample_rate_) /
                            1000000;

                        // next step ... start FFMpeg
                        start_ffmpeg_process();
                    }

                } catch (std::exception &e) {

                    update_status(
                        fmt::format("Failed with error: {} {}", __PRETTY_FUNCTION__, e.what()),
                        Failed);
                }
            },
            [=](caf::error &err) mutable {
                update_status(
                    fmt::format(
                        "Failed with error: {} {}", __PRETTY_FUNCTION__, to_string(err)),
                    Failed);
            });
}

void VideoRenderWorker::continue_render_loop() {

    if (completing_) {

        // If completing_ is true it means we have got to the end of the frame
        // range to be rendered and we've got all the image buffers and audio
        // buffers and queued them up to be piped to FFMPEG via the FIFOS.
        //
        // We need to check if the buffers have been fully streamed out to the
        // FIFOs ... if they have, we need to close the FIFO by making the
        // video_out_pipe_ or audio_out_pipe_ to exit.
        if (video_out_pipe_ && video_bufs_in_flight_ == 0) {

            send_exit(video_out_pipe_, caf::exit_reason::user_shutdown);
            video_out_pipe_ = caf::actor();
        }

        if (audio_out_pipe_ && audio_bufs_in_flight_ == 0) {

            send_exit(audio_out_pipe_, caf::exit_reason::user_shutdown);
            audio_out_pipe_ = caf::actor();
        }

    } else if (
        video_bufs_in_flight_ < max_frames_in_flight_ ||
        (audio_out_pipe_ && (audio_bufs_in_flight_ < max_frames_in_flight_))) {

        // continue loop
        anon_mail(playhead::step_atom_v).send(caf::actor_cast<caf::actor>(this));

        int cp = int(round(
            timebase::to_seconds(playhead_position_) * 100.0 /
            timebase::to_seconds(end_position_)));
        if (cp != percent_complete_) {
            percent_complete_ = int(round(
                timebase::to_seconds(playhead_position_) * 100.0 /
                timebase::to_seconds(end_position_)));
            update_status(fmt::format("In Progress ({}%)", percent_complete_));
        }
    }
}

void VideoRenderWorker::encode_frame(const media_reader::ImageBufPtr &image) {

    // TLDR: we need to stream data to FFMpeg at either the max rate that
    // FFMpeg can encode at or the max the xSTUDIO can deliver the video/audio
    // frames at. When encoding video AND audio, we also need to keep the FIFOs
    // topped up so FFMpeg can grab either video or audio samples as per the
    // requirements of the container format.

    // Note: when we have a new image buffer to encode, we can send it to the
    // video_out_pipe_ actor that will try to write the buffer into the fifo
    // that ffmpeg is reading from. If ffmpeg is going slowly, then the write
    // that the video_out_pipe_ wants to make will block (until ffmpeg catches
    // up and consumes more data from the fifo). That doesn't stop us from sending
    // more frames to the video_out_pipe_ actor, though, as CAF will queue up
    // those messages for us.
    // To keep track of how many video frames are queued for delivery to ffmpeg
    // via the fifo we increment video_bufs_in_flight_ here.

    // The audio stream is delivered via a separate fifo. FFmpeg consumes audio
    // samples and video frames from the FIFOs in a 'lumpy' manner which we
    // can't predict, so we need to make sure that we are feeding sufficient
    // amounts of audio and video to keep the FIFOs topped up and keep FFMpeg
    // happy (or we get into a deadlock where FFMPeg might want more video
    // frames, say, and we've decided to stop because we think the queue of
    // audio sample buffers on our side is too big).

    // At the same tiome we don't want to be queueing up TOO MUCH video/audio
    // data either in the FIFOs or in the CAF message queue as either situation
    // will start using up system RAM. For example, if xstudio can deliver
    // video/audio at a rate faster than FFMpeg can encode we could end up with
    // many video/audio buffers queued up which will consume a lot of system
    // RAM.

    video_bufs_in_flight_++;
    mail(image)
        .request(video_out_pipe_, std::chrono::seconds(30))
        .then(
            [=](bool) {
                // getting a result here mans a buffer was written into the fifo
                // so we decrement video_bufs_in_flight_
                video_bufs_in_flight_--;
                continue_render_loop();
            },
            [=](caf::error &err) {
                // update_status(fmt::format("Failed with error: {} {}", __PRETTY_FUNCTION__,
                // to_string(err)), true);
            });
}

void VideoRenderWorker::encode_audio(const media_reader::AudioBufPtr &audio) {

    // see notes in encode_frame for details on what's happening here.
    audio_bufs_in_flight_++;
    mail(audio)
        .request(audio_out_pipe_, std::chrono::seconds(30))
        .then(
            [=](bool) {
                audio_bufs_in_flight_--;
                continue_render_loop();
            },
            [=](caf::error &err) {
                // update_status(fmt::format("Failed with error: {} {}", __PRETTY_FUNCTION__,
                // to_string(err)), true);
            });
}

void VideoRenderWorker::start_ffmpeg_process() {

    // the ffmpeg subprocess is managed by the python component of this pluing.
    // We do this because python subprocess submodule is a lot easier than
    // doing in C/C++ and also better for cross platform support (especially
    // Windows) than managing subprocesses on the C++ side.

    update_status("Starting FFMPEG");

#ifdef _WIN32

    output_yuv_filename_ = fmt::format(
        "\\\\.\\pipe\\xSTUDIO_Pipe_{}.yuv",
        to_string(job_uuid_));

    HANDLE video_pipe = open_named_pipe(output_yuv_filename_);
    HANDLE audio_pipe = NULL;

    if (!audio_codec_opts_.empty()) {
        output_audio_filename_ = fmt::format(
            "\\\\.\\pipe\\xSTUDIO_Pipe_{}.raw",
            to_string(job_uuid_));
        audio_pipe = open_named_pipe(output_audio_filename_);
    }

#else

    auto tmpdir = utility::get_env("TMPDIR");
    if (!tmpdir) {
        throw std::runtime_error("Failed to read $TMPDIR env var.");
    }

    output_yuv_filename_ = fmt::format(
        "{}/xstdio_render_image_pipe_{}.yuv",
        std::string(*tmpdir),
        to_string(job_uuid_)); // utility::temp_file("test.yuv");
    if (mkfifo(output_yuv_filename_.c_str(), 0666) == -1) {
        throw std::runtime_error(fmt::format(
                                     "{} failed to create fifo with name {}, errno error: {}",
                                     __PRETTY_FUNCTION__,
                                     output_yuv_filename_,
                                     strerror(errno))
                                     .c_str());
    }
    if (!audio_codec_opts_.empty()) {
        output_audio_filename_ = fmt::format(
            "{}/xsutdio_render_audio_pipe_{}.raw", std::string(*tmpdir), to_string(job_uuid_));
        if (mkfifo(output_audio_filename_.c_str(), 0666) == -1) {
            throw std::runtime_error(
                fmt::format(
                    "{} failed to create fifo with name {}, errno error: {}",
                    __PRETTY_FUNCTION__,
                    output_audio_filename_,
                    strerror(errno))
                    .c_str());
        }
    }

#endif

    utility::JsonStore ffmpeg_args;
    ffmpeg_args["image_fifo_name"]          = output_yuv_filename_;
    ffmpeg_args["width"]                    = resolution_.x;
    ffmpeg_args["height"]                   = resolution_.y;
    ffmpeg_args["frame_rate"]               = rate_.to_fps();
    ffmpeg_args["audio_sample_rate"]        = 48000;
    ffmpeg_args["is_16_bit"]                = video_render_bit_depth_.find("16") == 0;
    ffmpeg_args["video_encoder_parameters"] = utility::split(video_codec_opts_, ' ');
    ffmpeg_args["video_output_plugin"] =
        utility::actor_to_string(system(), caf::actor_cast<caf::actor>(this));
    ffmpeg_args["output_file"] = output_file_path_;
    ffmpeg_args["job_id"]      = job_uuid_;

    if (!audio_codec_opts_.empty()) {
        ffmpeg_args["audio_fifo_name"]          = output_audio_filename_;
        ffmpeg_args["audio_encoder_parameters"] = utility::split(audio_codec_opts_, ' ');
    }

    auto python_interp = system().registry().template get<caf::actor>(embedded_python_registry);

    mail(
        embedded_python::python_exec_atom_v,
        "FFMpegEncoderPlugin",
        "start_ffmpeg_encode",
        ffmpeg_args)
        .request(python_interp, std::chrono::seconds(10))
        .then(
            [=](utility::JsonStore &result) {
                update_status("Started FFMPEG");
                percent_complete_       = 0;
                ffmpeg_process_running_ = true;

                audio_bufs_in_flight_ = 0;
                video_bufs_in_flight_ = 0;

                // Open FIFO for write only
                if (!audio_codec_opts_.empty()) {
                    audio_out_pipe_ =
                        spawn<RenderPipeActor>(
                            job_uuid_,
                            output_audio_filename_,
                            0
#ifdef _WIN32                    
                            ,audio_pipe
#endif                    
                        );
                }
                video_out_pipe_ = spawn<RenderPipeActor>(
                    job_uuid_,
                    output_yuv_filename_,
                    render_format_ == viewport::ImageFormat::RGBA_16 ? 8 : 4
#ifdef _WIN32                    
                    ,video_pipe
#endif                    
                    );

                // start the rendering after initialising the colour settings
                set_colour_params_and_start();
            },
            [=](caf::error &err) {
                update_status(
                    fmt::format(
                        "Failed with error: {} {}", __PRETTY_FUNCTION__, to_string(err)),
                    Failed);
            });
}

void VideoRenderWorker::clone_render_target(
    const utility::Uuid &parent_playlist_item_id, const utility::Uuid &target_render_item_id) {

    try {

        // When the user wishes to render a playlist/subset/contact sheet/timeline
        // we need to do a deep copy of that object. The reason is that after the
        // user requests the render, they could then go and edit or delete the
        // object in question which will have disasterous results for the render
        // if it is in progress. So here we fully duplicate the object and use
        // that duplicate as the target to render from.

        scoped_actor sys{system()};

        auto session = utility::request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(studio_registry),
            session::session_atom_v);

        auto bookmarks =
            utility::request_receive<caf::actor>(*sys, session, bookmark::get_bookmark_atom_v);

        auto playlist_item = utility::request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, parent_playlist_item_id);

        // serialise all bookmarks
        const auto bookmarks_serialisation = utility::request_receive<utility::JsonStore>(
            *sys, bookmarks, utility::serialise_atom_v);

        // serialise the parent playlist of the item that we're going to render.
        // Pass in the UUID of the timeline/subset/contact sheet so this is the
        // only container that gets serialised, though
        const auto playlist_serialisation = utility::request_receive<utility::JsonStore>(
            *sys, playlist_item, utility::serialise_atom_v, target_render_item_id);

        // use the serialisation to clone the playlist and the container that
        // we are going to render. Note that bookmarks are managed by an instance
        // of 'BookmarksActor' so we need a separate duplicate of this.
        auto bookmarks_duplicate = spawn<bookmark::BookmarksActor>(bookmarks_serialisation);
        // link_to(bookmarks_duplicate);

        auto playlist_duplicate =
            spawn<playlist::PlaylistActor>(playlist_serialisation, caf::actor());
        // link_to(playlist_duplicate);

        // link up the playlist clone to the bookmarks clone
        auto media = utility::request_receive<utility::UuidActorVector>(
            *sys, playlist_duplicate, playlist::get_media_atom_v);
        utility::request_receive<int>(
            *sys, bookmarks_duplicate, bookmark::associate_bookmark_atom_v, media);

        if (parent_playlist_item_id == target_render_item_id) {

            renderable_ = playlist_duplicate;

        } else {

            auto playlist_children = utility::request_receive<utility::UuidActorVector>(
                *sys, playlist_duplicate, playlist::get_container_atom_v, true);
            for (auto &c : playlist_children) {
                if (c.uuid() == target_render_item_id)
                    renderable_ = c.actor();
            }
        }
        link_to(renderable_);


        if (!renderable_) {
            throw std::runtime_error("Failed to get to playlist child needed for rendering");
        }

        const auto type =
            utility::request_receive<std::string>(*sys, renderable_, utility::type_atom_v);

        if (type == "Subset" || type == "Playlist") {
            // the render behaviour for Subsets and Playlists is that all items in the container
            // are strung together.
            const auto playhead_selection = utility::request_receive<caf::actor>(
                *sys, renderable_, playlist::selection_actor_atom_v);
            mail(playlist::select_all_media_atom_v).send(playhead_selection);
        }

    } catch (std::exception &e) {
        update_status(
            fmt::format("Failed with error: {} {}", __PRETTY_FUNCTION__, e.what()), Failed);
    }
}

void VideoRenderWorker::set_colour_params_and_start() {

    // Force render first frame - this will trigger the colour pipeline to initialise
    // itself for the first media item to be rendererd. It will load the ocio
    // config and intialise the OCIO Display & View attributes.

    auto handle_fail = [=](caf::error &err) {
        update_status(
            fmt::format("Failed with error: {} {}", __PRETTY_FUNCTION__, to_string(err)),
            Failed);
    };

    // set out position first
    mail(playhead::jump_atom_v, playhead_position_)
        .request(playhead_, infinite)
        .then(
            [=](bool) {
                // now render the viewport to an image buffer
                mail(
                    viewport::render_viewport_to_image_atom_v,
                    resolution_.x,
                    resolution_.y,
                    render_format_)
                    .request(offscreen_viewport_, infinite)
                    .then(
                        [=](const media_reader::ImageBufPtr &) {
                            // now we can try and set OCIO View / Display
                            mail(
                                module::change_attribute_request_atom_v,
                                "Display",
                                int(module::Attribute::Value),
                                utility::JsonStore(ocio_display_))
                                .request(colour_pipeline_, infinite)
                                .then(
                                    [=](bool) {
                                        mail(
                                            module::change_attribute_request_atom_v,
                                            "View",
                                            int(module::Attribute::Value),
                                            utility::JsonStore(ocio_view_))
                                            .request(colour_pipeline_, infinite)
                                            .then(
                                                [=](bool) {
                                                    // this self-message starts the actual
                                                    // rendering loop
                                                    anon_mail(playhead::step_atom_v)
                                                        .send(
                                                            caf::actor_cast<caf::actor>(this));
                                                },
                                                handle_fail);
                                    },
                                    handle_fail);
                        },
                        handle_fail);
            },
            handle_fail);
}

void VideoRenderWorker::render_step() {

    if (playhead_position_ > end_position_) {
        // we have encoding the frame range.
        percent_complete_ = 100;
        completing_       = true;
        continue_render_loop();
        return;
    }

    // we've asked ourselves to do a render step, but the last render
    // step has not completed (i.e. the image and/or audio buffers requested
    // below last time we entered this function haven't been received yet).
    //
    // No problem, just return the render loop will be perpetuated from the
    // reciever functions below.
    if (waiting_for_buffers_)
        return;

    waiting_for_buffers_ = true;
    // set playhead position
    mail(playhead::jump_atom_v, playhead_position_)
        .request(playhead_, infinite)
        .then(
            [=](bool) {
                // now render the viewport to an image buffer
                mail(
                    viewport::render_viewport_to_image_atom_v,
                    resolution_.x,
                    resolution_.y,
                    render_format_)
                    .request(offscreen_viewport_, infinite)
                    .then(
                        [=](const media_reader::ImageBufPtr &image) {
                            if (render_format_ == viewport::ImageFormat::RGBA_16) {
                                flop_rgba_image<uint16_t>(image);
                            } else {
                                flop_rgba_image<uint8_t>(image);
                            }

                            // send the image to ffmpeg
                            encode_frame(image);

                            if (audio_out_pipe_) {

                                // now make an empty audio buffer - we send this to the playhead
                                // to fill with samples.
                                auto audio_buffer =
                                    media_reader::AudioBufPtr(new media_reader::AudioBuffer());

                                // calculate duration of audio we need - in order to avoid
                                // losing samples due to rounding (playhead position is in
                                // timebase::flicks, whereas soundcard sample rate might not
                                // factor into flicks exactly), our reference is
                                // num_audio_samples_delivered_ which has to keep up with
                                // playhead_position_ according to soundcard sample rate
                                auto p_plus = playhead_position_ + rate_;
                                auto microsecs =
                                    std::chrono::duration_cast<std::chrono::microseconds>(
                                        p_plus)
                                        .count() -
                                    (num_audio_samples_delivered_ * 1000000) /
                                        int64_t(soundcard_sample_rate_);
                                const int64_t num_samples =
                                    microsecs * int64_t(soundcard_sample_rate_) / 1000000;

                                audio_buffer->allocate(
                                    soundcard_sample_rate_,    // sample rate
                                    2,                         // num channels
                                    num_samples,               // samples
                                    audio::SampleFormat::INT16 // format
                                );
                                memset(audio_buffer->buffer(), 0, audio_buffer->size());

                                num_audio_samples_delivered_ += num_samples;

                                // now request an audio buffer corresponding to the duration of
                                // the video frame
                                mail(
                                    playhead::audio_buffer_atom_v,
                                    playhead_position_,
                                    audio_buffer)
                                    .request(playhead_, infinite)
                                    .then(
                                        [=](const media_reader::AudioBufPtr &audio_buf) {
                                            waiting_for_buffers_ = false;

                                            // advance the playhead position to the next frame
                                            playhead_position_ += rate_;

                                            // send the audio to ffmpeg
                                            encode_audio(audio_buf);
                                            continue_render_loop();
                                        },
                                        [=](caf::error &err) mutable {
                                            update_status(
                                                fmt::format(
                                                    "Failed with error: {} {}",
                                                    __PRETTY_FUNCTION__,
                                                    to_string(err)),
                                                Failed);
                                        });

                            } else {

                                // advance the playhead position to the next frame
                                waiting_for_buffers_ = false;
                                playhead_position_ += rate_;
                                continue_render_loop();
                            }
                        },
                        [=](caf::error &err) mutable {
                            update_status(
                                fmt::format(
                                    "Failed with error: {} {}",
                                    __PRETTY_FUNCTION__,
                                    to_string(err)),
                                Failed);
                        });
            },
            [=](caf::error &err) mutable {
                update_status(
                    fmt::format(
                        "Failed with error: {} {}", __PRETTY_FUNCTION__, to_string(err)),
                    Failed);
            });
}

void VideoRenderWorker::add_output_to_session() {

    scoped_actor sys{system()};

    auto session = utility::request_receive<caf::actor>(
        *sys,
        system().registry().template get<caf::actor>(studio_registry),
        session::session_atom_v);

    // UuidUuidActor ... use of this in various places makes code confusing.
    // It's a way of holding an actor, the actor's uuid and also the uuid of
    // the 'Container' which is wrapped by the actor. Why containers and
    // actors need different uuids I don't know.

    auto do_send = [=](utility::UuidUuidActor playlist) {
        anon_mail(
            playlist::add_media_atom_v,
            fmt::format("{} - Render", render_item_name_),
            utility::posix_path_to_uri(output_file_path_),
            utility::Uuid())
            .send(playlist.second.actor());
        mail(utility::user_start_action_atom_v).send(renderer_plugin_);
        send_exit(caf::actor_cast<caf::actor>(this), caf::exit_reason::user_shutdown);
    };

    auto handle_error = [=](caf::error &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        mail(utility::user_start_action_atom_v).send(renderer_plugin_);
        send_exit(caf::actor_cast<caf::actor>(this), caf::exit_reason::user_shutdown);
    };

    // get or make a playlist called "Render Outputs" and add the rendered
    // movie to the playlist
    mail(session::get_playlist_atom_v, "Render Outputs")
        .request(session, infinite)
        .then(
            [=](caf::actor check_playlist) {
                if (!check_playlist) {
                    mail(session::add_playlist_atom_v, "Render Outputs")
                        .request(session, infinite)
                        .then(do_send, handle_error);
                } else {
                    do_send(utility::UuidUuidActor(
                        utility::Uuid(), utility::UuidActor(utility::Uuid(), check_playlist)));
                }
            },
            handle_error);
}
