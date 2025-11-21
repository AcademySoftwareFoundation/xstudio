// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "audio_waveform_overlay.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

#ifdef __apple__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {
const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in float ypos;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform float hscale;
    uniform float vscale;
    uniform float v_pos;
    uniform int offset;

    void main()
    {
        vec4 rpos = vec4(-1.0 + float(gl_VertexID-offset)*hscale, v_pos+ypos*vscale*10.0, vec2(0.0, 1.0));
        gl_Position = rpos*to_canvas;
    }
    )";

const char *frag_shader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 line_colour;

    void main(void)
    {
        FragColor = vec4(line_colour, 1.0);
    }

    )";
} // namespace

AudioWaveformOverlayRenderer::~AudioWaveformOverlayRenderer() {
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteBuffers(1, &vao_);
}

void AudioWaveformOverlayRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame,
    const bool have_alpha_buffer) {

    if (!shader_)
        init_overlay_opengl();

    auto render_data =
        frame.plugin_blind_data(utility::Uuid("873c508b-276b-44e3-82d0-15db2f039aa7"));
    if (!render_data)
        return;

    const auto *data = dynamic_cast<const WaveFormData *>(render_data.get());
    if (!data)
        return;

    glBindVertexArray(vao_);
    // 2. copy our vertices array in a buffer for OpenGL to use
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        data->verts_.size() * sizeof(float),
        data->verts_.data(),
        GL_STREAM_DRAW);
    // 3. then set our vertex module pointers
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const int n_samps = data->verts_.size() / data->num_chans;

    utility::JsonStore shader_params;
    shader_params["to_canvas"]   = transform_window_to_viewport_space;
    shader_params["hscale"]      = 2.0f / float(n_samps);
    shader_params["vscale"]      = data->vscale * device_pixel_ratio;
    shader_params["line_colour"] = data->line_colour;
    shader_->set_shader_parameters(shader_params);
    shader_->use();
    glEnableVertexAttribArray(0);

    for (int c = 0; c < data->num_chans; ++c) {
        utility::JsonStore es;
        es["v_pos"]  = data->v_pos + data->chan_spacing * c;
        es["offset"] = c * n_samps;
        shader_->set_shader_parameters(es);
        // the actual draw!
        glDrawArrays(GL_LINE_STRIP, c * n_samps, n_samps);
    }
    shader_->stop_using();
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}

void AudioWaveformOverlayRenderer::init_overlay_opengl() {

    glGenBuffers(1, &vbo_);
    glGenVertexArrays(1, &vao_);

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);
}


AudioWaveformOverlay::AudioWaveformOverlay(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "Audio Waveform", init_settings, 0.0f) {

    vertical_scale_ =
        add_float_attribute("Vertical Scale", "Vertical Scale", 0.1f, 0.01f, 1.0f, 0.01f);
    add_hud_settings_attribute(vertical_scale_);
    vertical_scale_->set_tool_tip("Sets the vertical scaling of the waveform");

    horizontal_scale_ =
        add_float_attribute("Horizontal Scale", "Horizontal Scale", 50.0f, 10.0f, 100.0f, 1.0f);
    add_hud_settings_attribute(horizontal_scale_);
    horizontal_scale_->set_tool_tip("Sets the horizontal scaling of the waveform - the units "
                                    "are milliseconds of audio shown on the screen");

    chan_position_spacing_ = add_float_attribute(
        "Chan Position Spacing", "Chan Position Spacing", 0.05f, 0.0f, 1.0f, 0.01f);
    add_hud_settings_attribute(chan_position_spacing_);
    chan_position_spacing_->set_tool_tip("Vertical spacing between channels");

    vertical_position_ = add_float_attribute(
        "Vertical Position", "Vertical Position", -0.8f, -1.0f, 1.0f, 0.01f);
    add_hud_settings_attribute(vertical_position_);
    vertical_position_->set_tool_tip("Vertical position for drawing the waveform");

    separate_channels_ =
        add_boolean_attribute("Show Channels Separately", "Show Channels Separately", false);
    add_hud_settings_attribute(separate_channels_);
    separate_channels_->set_tool_tip(
        "Shows the waveforms of each channel, or combine channels if not selected.");

    line_colour_ = add_colour_attribute(
        "Line Colour", "Line Colour", utility::ColourTriplet(1.0f, 1.0f, 0.0f));
    add_hud_settings_attribute(line_colour_);
    line_colour_->set_tool_tip("The colour of the waveform line");

    // Registering preference path allows these values to persist between sessions
    vertical_scale_->set_preference_path("/plugin/audio_waveform/vertical_scale");
    horizontal_scale_->set_preference_path("/plugin/audio_waveform/horizontal_scale");
    chan_position_spacing_->set_preference_path("/plugin/audio_waveform/chan_position_spacing");
    vertical_position_->set_preference_path("/plugin/audio_waveform/vertical_position");
    line_colour_->set_preference_path("/plugin/audio_waveform/line_colour");

    // get the global audio output actor and join its event group. This means we
    // receive the broadcasted Audiobuffers
    auto global_audio_actor =
        system().registry().template get<caf::actor>(audio_output_registry);
    utility::join_event_group(this, global_audio_actor);

    message_handler_ext_ = {
        [=](utility::event_atom,
            module::change_attribute_event_atom,
            const float volume,
            const bool muted,
            const bool repitch,
            const bool scrubbing) {},
        [=](utility::event_atom,
            playhead::sound_audio_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            const utility::Uuid &sub_playhead,
            const bool scrubbing,
            const timebase::flicks) {},
        [=](utility::event_atom,
            playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed) {},
        [=](utility::event_atom,
            playhead::position_atom,
            const timebase::flicks playhead_position,
            const bool forward,
            const float velocity,
            const bool playing,
            utility::time_point when_position_changed) {},
        [=](utility::event_atom,
            audio::audio_samples_atom,
            const std::vector<media_reader::AudioBufPtr> &audio_buffers,
            timebase::flicks playhead_position,
            const utility::Uuid &playhead_uuid) {
            latest_audio_buffers_[playhead_uuid] = audio_buffers;
        }};

    make_behavior();
    // we need to keep track of which playhead is driving which viewport
    listen_to_playhead_events();
}

AudioWaveformOverlay::~AudioWaveformOverlay() = default;

void AudioWaveformOverlay::attribute_changed(
    const utility::Uuid &attribute_uuid, const int /*role*/
) {

    redraw_viewport();
}

utility::BlindDataObjectPtr AudioWaveformOverlay::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid &playhead_uuid,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    auto r = utility::BlindDataObjectPtr();
    if (!visible())
        return r;

    auto p = latest_audio_buffers_.find(playhead_uuid);
    if (p == latest_audio_buffers_.end())
        return r;
    const auto &latest_audio_buffers = p->second;

    // check our sample buffers to get sample rate & num channels
    int nc               = 0;
    uint64_t sample_rate = 0;
    for (const auto &aud_buf : latest_audio_buffers) {

        if (aud_buf && !sample_rate && !nc) {
            nc          = aud_buf->num_channels();
            sample_rate = aud_buf->sample_rate();
        }
    }

    if (!sample_rate)
        return r;

    float millisecs  = horizontal_scale_->value();
    int samps_needed = int(round(millisecs * float(sample_rate) / 1000.0f));

    std::vector<float> verts(samps_needed * (separate_channels_->value() ? nc : 1));

    // this gives us the ref timestamp for the start of the window of samples that
    // we will draw to the screen
    timebase::flicks tt =
        image.timeline_timestamp() -
        timebase::to_flicks((millisecs / 1000.0 - image.frame_id().rate().to_seconds()) * 0.5);

    for (const auto &aud_buf : latest_audio_buffers) {

        if (aud_buf) {
            // reference timeline timestamp for first sample
            timebase::flicks when_samples_play =
                aud_buf.timeline_timestamp() + std::chrono::duration_cast<timebase::flicks>(
                                                   aud_buf->time_delta_to_video_frame());
            const int nsamp = aud_buf->num_samples();

            if (separate_channels_->value()) {

                // offset *into* the samples that we're generating
                for (int c = 0; c < nc; ++c) {
                    int offset = timebase::to_seconds(when_samples_play - tt) * sample_rate;
                    int n      = 0;
                    if (offset < 0) {
                        n      = -offset;
                        offset = 0;
                    }
                    int16_t *samp_data = (int16_t *)aud_buf->buffer();
                    samp_data += n * nc + c;
                    while (offset < samps_needed && n < nsamp) {

                        verts[offset + samps_needed * c] = float(*samp_data) * 0.000030518f;
                        n++;
                        offset++;
                        samp_data += nc;
                    }
                }

            } else {

                int offset = timebase::to_seconds(when_samples_play - tt) * sample_rate;
                int n      = 0;
                if (offset < 0) {
                    n      = -offset;
                    offset = 0;
                }
                int16_t *samp_data = (int16_t *)aud_buf->buffer();
                samp_data += n * nc;
                while (offset < samps_needed && n < nsamp) {

                    float f = 0.0f;
                    int c   = nc;
                    while (c--) {
                        f += *(samp_data++);
                    }
                    verts[offset] = f * 0.000030518f;
                    n++;
                    offset++;
                }
            }
        }
    }

    r.reset(new WaveFormData(
        verts,
        separate_channels_->value() ? nc : 1,
        vertical_scale_->value(),
        chan_position_spacing_->value(),
        vertical_position_->value(),
        line_colour_->value()));

    return r;
}

/*void AudioWaveformOverlay::viewport_playhead_changed(const std::string &viewport_name,
caf::actor playhead) { if (playhead) { request(playhead, infinite, utility::uuid_atom_v).then(
            [=](utility::Uuid &playhead_uuid) {
            },
            [=](caf::error &err) {});
    }
}*/

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<AudioWaveformOverlay>>(
                utility::Uuid("873c508b-276b-44e3-82d0-15db2f039aa7"),
                "AudioWaveformOverlay",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "Ted Waine",
                "Audio Waveform Overlay")}));
}
}
