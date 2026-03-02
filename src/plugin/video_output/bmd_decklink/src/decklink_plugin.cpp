// SPDX-License-Identifier: Apache-2.0
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#undef WIN32_LEAN_AND_MEAN
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include <filesystem>

#include <caf/actor_registry.hpp>

#include "decklink_audio_device.hpp"
#include "decklink_plugin.hpp"
#include "decklink_output.hpp"

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

#include <ImfRgbaFile.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::bm_decklink_plugin_1_0;

namespace {
    static std::map<std::string, BMDPixelFormat> bmd_pixel_formats(
        {
            {"8 bit YUV", bmdFormat8BitYUV},
            {"10 bit YUV", bmdFormat10BitYUV},
            {"8 bit ARGB", bmdFormat8BitARGB},
            {"8 bit BGRA", bmdFormat8BitBGRA},
            {"10 bit RGB", bmdFormat10BitRGB},
            {"12 bit RGB", bmdFormat12BitRGB},
            {"12 bit RGB-LE", bmdFormat12BitRGBLE},
            {"10 bit RGB Video Range", bmdFormat10BitRGBX},
            {"10 bit RGB-LE Video Range", bmdFormat10BitRGBXLE}
        });

    static const std::vector<std::string> hdr_metadata_value_names({
        "Red x",
        "Red y",
        "Green x",
        "Green y",
        "Blue x",
        "Blue y",
        "White Pt. x",
        "White Pt. y"});

    const std::vector<std::string> light_level_controls_names({
        "D.M. Lum. Min",
        "D.M. Lum. Max",
        "CLL Max",
        "Frm Avg Light"
    });
    
    const std::map <std::string, int64_t> eotf_modes({
        {"SDR", 0},
        {"HDR", 1},
        {"PQ", 2},
        {"HLG", 3},
        {"Auto PQ", -1},
    });

    const std::map <std::string, int64_t> colourspaces({
        {"BT. 601", bmdColorspaceRec601},
        {"BT. 709", bmdColorspaceRec709},
        {"BT. 2020", bmdColorspaceRec2020}
    });

static const std::string shelf_button_qml(R"(
import QtQuick 2.12
import BlackmagicSDI 1.0
DecklinkShelfButton {
}
)");
}

BMDecklinkPlugin::BMDecklinkPlugin(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ui::viewport::VideoOutputPlugin(cfg, init_settings, "BMDecklinkPlugin")
{

    // Here we check that the Decklink drivers are installed. If not, we log a 
    // message and disable the plugin by throwing an exception in the constructor
    // which will prevent the plugin from being created. This prevents the SDI
    // UI component from appearing in xSTUDIO and confusing the user when they
    // don't have a DeckLink device or drivers installed.
    try {

        DecklinkOutput::check_decklink_installation();

    } catch (const std::exception &e) {

		send_exit(this, caf::exit_reason::user_shutdown);
        spdlog::info("Blackmagic Decklink SDI output disabled: {}", e.what());
        return;
    }

    // add attributes used for configuring the SDI output
    pixel_formats_ = add_string_choice_attribute("Pixel Format", "Pix Fmt", "10 bit YUV", utility::map_key_to_vec(bmd_pixel_formats));
    pixel_formats_->expose_in_ui_attrs_group("Decklink Settings");
    pixel_formats_->set_preference_path("/plugin/decklink/pixel_format");

    sdi_output_is_running_ = add_boolean_attribute("Enabled", "Enabled", false);
    sdi_output_is_running_->expose_in_ui_attrs_group("Decklink Settings");

    status_message_ = add_string_attribute("Status", "Status", "Not Started");
    status_message_->expose_in_ui_attrs_group("Decklink Settings");

    is_in_error_ = add_boolean_attribute("In Error", "In Error", false);
    is_in_error_->expose_in_ui_attrs_group("Decklink Settings");

    track_main_viewport_ = add_boolean_attribute("Track Viewport", "Track Viewport", true);
    track_main_viewport_->expose_in_ui_attrs_group("Decklink Settings");

    resolutions_ = add_string_choice_attribute(
            "Output Resolution",
            "Out. Res.");

    resolutions_->expose_in_ui_attrs_group("Decklink Settings");
    resolutions_->set_preference_path("/plugin/decklink/output_resolution");

    frame_rates_ = add_string_choice_attribute(
        "Refresh Rate",
        "Hz");
    frame_rates_->expose_in_ui_attrs_group("Decklink Settings");
    frame_rates_->set_preference_path("/plugin/decklink/output_frame_rate");

    start_stop_ =
        add_boolean_attribute("Start Stop", "Start Stop", false);
    start_stop_->expose_in_ui_attrs_group("Decklink Settings");

    auto_start_ = add_boolean_attribute("Auto Start", "Auto Start", false);
    auto_start_->set_preference_path("/plugin/decklink/auto_start_sdi");

    samples_water_level_ = add_integer_attribute("Audio Samples Water Level", "Audio Samples Water Level", 4096);
    samples_water_level_->set_preference_path("/plugin/decklink/audio_samps_water_level");

    // This attr is currently not exposed to the user. It can be used to delay (or advance) the image that
    // is sent to the SDI output vs. the main xSTUDIO viewport in order to exactly sync up the image
    // that shows in xSTUDIO UI vs. the SDI display. This helps if you have a screening room with xSTUDIO
    // UI on a monitor and the SDI output shown by a projector at the front of the room ... the person
    // operating xSTUDIO may see the image on their screen is a few frames ahead of the SDI.
    // However, for this to work we have to buffer a lot more frames in the playhead and this is expensive
    // which is why I'm leaving it here but not exposing it.
    video_pipeline_delay_milliseconds_= add_integer_attribute("Video Sync Delay", "Video Sync Delay", 0);
    video_pipeline_delay_milliseconds_->set_preference_path("/plugin/decklink/video_sync_delay");
    video_pipeline_delay_milliseconds_->expose_in_ui_attrs_group("Decklink Settings");

    audio_sync_delay_milliseconds_= add_integer_attribute("Audio Sync Delay", "Audio Sync Delay", 0);
    audio_sync_delay_milliseconds_->set_preference_path("/plugin/decklink/new_audio_sync_delay");
    audio_sync_delay_milliseconds_->expose_in_ui_attrs_group("Decklink Settings");

    disable_pc_audio_when_running_ = add_boolean_attribute("Auto Disable PC Audio", "Auto Disable PC Audio", false);
    disable_pc_audio_when_running_->set_preference_path("/plugin/decklink/disable_pc_audio_when_sdi_is_running");
    disable_pc_audio_when_running_->expose_in_ui_attrs_group("Decklink Settings");

    make_hdr_attributes();

    VideoOutputPlugin::finalise();
}

// This method is called when a new image buffer is ready to be displayed
void BMDecklinkPlugin::incoming_video_frame_callback(media_reader::ImageBufPtr incoming) {
    dcl_output_->incoming_frame(incoming);
}

void BMDecklinkPlugin::exit_cleanup() {
    // the dcl_output_ has caf::actor handles pointing to this (BMDecklinkPlugin)
    // instance. The BMDecklinkPlugin will therefore never get deleted due to
    // circular dependency so we use the on_exit
    delete dcl_output_;
}

void BMDecklinkPlugin::receive_status_callback(const utility::JsonStore & status_data) {

    if (status_data.contains("status_message") && status_data["status_message"].is_string()) {
        status_message_->set_value(status_data["status_message"].get<std::string>());
    }
    if (status_data.contains("sdi_output_is_active") && status_data["sdi_output_is_active"].is_boolean()) {
        sdi_output_is_running_->set_value(status_data["sdi_output_is_active"].get<bool>());
    }
    if (status_data.contains("error_state") && status_data["error_state"].is_boolean()) {
        is_in_error_->set_value(status_data["error_state"].get<bool>());
    }

}

void BMDecklinkPlugin::attribute_changed(const utility::Uuid &attribute_uuid, const int role)
{

    if (dcl_output_) {

        if (resolutions_ && attribute_uuid == resolutions_->uuid() && role == module::Attribute::Value) {

            const auto rates = dcl_output_->get_available_refresh_rates(resolutions_->value());
            frame_rates_->set_role_data(module::Attribute::StringChoices, rates);

            // pick a sensible refresh rate, if the current rate isn't available for new resolution
            auto i = std::find(rates.begin(), rates.end(), frame_rates_->value()); // prefer current rate
            if (i == rates.end()) {
                i = std::find(rates.begin(), rates.end(), "24.0"); // otherwise prefer 24.0
                if (i == rates.end()) {
                    i = std::find(rates.begin(), rates.end(), "60.0"); // use 60.0 otherwise
                    if (i == rates.end()) {
                        i = rates.begin();
                    }
                }
            }
            if (i != rates.end()) {
                frame_rates_->set_value(*i);
            }

        } else if (attribute_uuid == start_stop_->uuid()) {

            dcl_output_->StartStop();

        }

        if (attribute_uuid == pixel_formats_->uuid() || attribute_uuid == resolutions_->uuid() || attribute_uuid == frame_rates_->uuid()) {

            try {

                if (bmd_pixel_formats.find(pixel_formats_->value()) == bmd_pixel_formats.end()) {
                    throw std::runtime_error(fmt::format("Invalid pixel format: {}", pixel_formats_->value()));
                }

                const BMDPixelFormat pix_fmt = bmd_pixel_formats[pixel_formats_->value()];


                dcl_output_->set_display_mode(
                    resolutions_->value(),
                    frame_rates_->value(),
                    pix_fmt);

            } catch (std::exception & e) {

                status_message_->set_value(e.what());
                is_in_error_->set_value(true);

            }

        } else if (attribute_uuid == track_main_viewport_->uuid()) {

            sync_geometry_to_main_viewport(track_main_viewport_->value());

        } else if (attribute_uuid == samples_water_level_->uuid()) {
            dcl_output_->set_audio_samples_water_level(samples_water_level_->value());
        } else if (attribute_uuid == audio_sync_delay_milliseconds_->uuid()) {
            dcl_output_->set_audio_sync_delay_milliseconds(audio_sync_delay_milliseconds_->value());
        } else if (attribute_uuid == video_pipeline_delay_milliseconds_->uuid()) {
            video_delay_milliseconds(video_pipeline_delay_milliseconds_->value());
        } else if (attribute_uuid == disable_pc_audio_when_running_->uuid()) {
            set_pc_audio_muting();
        } else if (attribute_uuid == sdi_output_is_running_->uuid()) {
            set_pc_audio_muting();
        } else if (attribute_uuid == hdr_presets_->uuid()) {
            
            if (hdr_presets_data_.contains(hdr_presets_->value())) {

                const auto &vals = hdr_presets_data_[hdr_presets_->value()];
                if (vals.is_array() && vals.size() == 8) {
                    int idx = 0;
                    for (auto &setting_attr: hdr_metadata_settings_) {
                        setting_attr->set_value(vals[idx++].get<float>());
                    }
                }
            }

        } else if (role == module::Attribute::Value &&
            hdr_metadata_settings_uuids_.find(attribute_uuid) != hdr_metadata_settings_uuids_.end()) {
            set_hdr_mode_and_metadata();
            if (!prefs_save_scheduled_) {
                prefs_save_scheduled_ = true;
                delayed_anon_send(
                    caf::actor_cast<caf::actor>(this),
                    std::chrono::seconds(5),
                    "save_hdr_colour_prefs"
                    );
            }
        }

    }
    StandardPlugin::attribute_changed(attribute_uuid, role);

}

audio::AudioOutputDevice * BMDecklinkPlugin::make_audio_output_device(const utility::JsonStore &prefs) {
    return static_cast<audio::AudioOutputDevice *>(new DecklinkAudioOutputDevice(prefs, dcl_output_));
}

void BMDecklinkPlugin::initialise() {

    try {

        dcl_output_ = new DecklinkOutput(this);
        set_hdr_mode_and_metadata();

        resolutions_->set_role_data(module::Attribute::StringChoices, dcl_output_->output_resolution_names());

        dcl_output_->set_audio_samples_water_level(samples_water_level_->value());
        dcl_output_->set_audio_sync_delay_milliseconds(audio_sync_delay_milliseconds_->value());

        spdlog::info("Decklink Card Initialised");

        // We register the UI here
        register_viewport_dockable_widget(
            "SDI Output Controls 2",
            shelf_button_qml,                // QML for a custom shelf button to activate the tool
            "Show/Hide SDI Output Controls", // tooltip for the button,
            10.0f,                           // button position in the buttons bar
            true,
            "", // no dockable left/right widget
            // qml code to create the top/bottom dockable widget
            R"(
                import QtQuick
                import BlackmagicSDI 1.0
                DecklinkSettingsHorizontalWidget {
                }
                )");

        // now we are set-up we can kick ourselves to fill in the refresh rate list etc.
        attribute_changed(resolutions_->uuid(), module::Attribute::Value);

        if (auto_start_->value()) {
            // start output immediately if auto_start_ is enabled (via prefs)
            dcl_output_->StartStop();
        }

        sync_geometry_to_main_viewport(track_main_viewport_->value());

        video_delay_milliseconds(video_pipeline_delay_milliseconds_->value());

        // tell our viewport what sort of display we are. This info is used by
        // the colour management system to try and pick an appropriate display
        // transform
        display_info(
            "SDI Video Output",
            "Decklink",
            "Blackmagic Design",
            "");

        // Here we add the Display attribute from the colour pipeline and the 
        // Fit mode attr from the offscreen viewport to an attribute group
        // called "decklink viewport attrs" which is exposed in the UI. 
        // Our UI bar references this attribute group to make the dropdowns to
        // control those two attributes.
        anon_mail(
            module::change_attribute_request_atom_v,
            "Display",
            int(module::Attribute::UIDataModels),
            utility::JsonStore(nlohmann::json::parse(R"(["decklink viewport attrs", "decklink ocio"])"))
        ).send(colour_pipeline());

        anon_mail(
            module::change_attribute_request_atom_v,
            "Fit (F)",
            int(module::Attribute::UIDataModels),
            utility::JsonStore(nlohmann::json::parse(R"(["decklink viewport attrs"])"))
        ).send(offscreen_viewport());

    } catch (std::exception & e) {
        spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
    }

}

void BMDecklinkPlugin::set_pc_audio_muting() {

    // we can get access to the
    auto pc_audio_output_actor =
        system().registry().template get<caf::actor>(pc_audio_output_registry);

    if (pc_audio_output_actor) {
        const bool mute = disable_pc_audio_when_running_->value() && sdi_output_is_running_->value();
        anon_send(pc_audio_output_actor, audio::set_override_volume_atom_v, mute ? 0.0f : -1.0f);
    }

}

void BMDecklinkPlugin::make_hdr_attributes()
{

    // Add the HDR mode attribute
    hdr_mode_ = add_string_choice_attribute("HDR Mode", "HDR Mode", "SDR", utility::map_key_to_vec(eotf_modes));
    hdr_mode_->expose_in_ui_attrs_group("Decklink HDR Settings");
    hdr_mode_->set_preference_path("/plugin/decklink/hdr_mode");
    hdr_metadata_settings_uuids_.insert(hdr_mode_->uuid());

    // Add the Colourspace mode attribute
    colourspace_ = add_string_choice_attribute("Colour Space", "Colour Space", "BT. 709", utility::map_key_to_vec(colourspaces));
    colourspace_->expose_in_ui_attrs_group("Decklink HDR Settings");
    colourspace_->set_preference_path("/plugin/decklink/colourspace");
    hdr_metadata_settings_uuids_.insert(colourspace_->uuid());

    // Fetch the ocio display name match string for auto HDR mode switching
    auto prefs = global_store::GlobalStoreHelper(system());
    try {
        ocio_display_hdr_match_string_ =
            utility::to_lower(prefs.value<std::string>("/plugin/decklink/ocio_display_hdr_match_string"));
    } catch (std::exception & e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    std::array<float, 8> colour_settings({0.64, 0.33, 0.30, 0.6, 0.15, 0.06, 0.3127, 0.329});
    try {
        auto colour_vals =
            prefs.value<utility::JsonStore>("/plugin/decklink/hdr_colour_metadata_values");
        if (colour_vals.is_array() && colour_vals.size() == 8) {
            int idx = 0;
            for (auto &o: colour_vals) {
                if (!o.is_number()) {
                    throw std::runtime_error("hdr_colour_metadata_values should be 8 float values.");
                }
                colour_settings[idx++] = o.get<float>();
            }
        } else {
            throw std::runtime_error("hdr_colour_metadata_values should be an array of 8 float values.");
        }
    } catch (std::exception & e) {
        spdlog::warn("{}: Failed to load perference /plugin/decklink/hdr_colour_metadata_values: {}", __PRETTY_FUNCTION__, e.what());
    }

    int idx = 0;
    for (const auto &o: hdr_metadata_value_names) {
        hdr_metadata_settings_.push_back(add_float_attribute(o, o, colour_settings[idx++]));
        hdr_metadata_settings_.back()->expose_in_ui_attrs_group("Decklink HDR Values");
        hdr_metadata_settings_uuids_.insert(hdr_metadata_settings_.back()->uuid());
    }


    std::array<float, 4> lum_settings({0.0005, 1000.0, 1000.0, 400.0});
    try {
        auto lum__vals =
            prefs.value<utility::JsonStore>("/plugin/decklink/hdr_luminance_metadata_values");
        if (lum__vals.is_array() && lum__vals.size() == 4) {
            int idx = 0;
            for (auto &o: lum__vals) {
                if (!o.is_number()) {
                    throw std::runtime_error("hdr_luminance_metadata_values should be 4 float values.");
                }
                lum_settings[idx++] = o.get<float>();
            }
        } else {
            throw std::runtime_error("hdr_luminance_metadata_values should be an array of 4 float values.");
        }
    } catch (std::exception & e) {
        spdlog::warn("{}: Failed to load perference /plugin/decklink/hdr_luminance_metadata_values: {}", __PRETTY_FUNCTION__, e.what());
    }

    idx = 0;
    for (const auto &o: light_level_controls_names) {
        hdr_metadata_lightlevel_.push_back(add_float_attribute(o, o, lum_settings[idx++]));
        hdr_metadata_lightlevel_.back()->expose_in_ui_attrs_group("Decklink HDR Values");
        hdr_metadata_settings_uuids_.insert(hdr_metadata_lightlevel_.back()->uuid());
    }

    try {

        hdr_presets_ = add_string_choice_attribute("HDR Presets", "HDR Presets", "", {});
        hdr_presets_->expose_in_ui_attrs_group("Decklink HDR Settings");

        hdr_presets_data_ =
            prefs.value<utility::JsonStore>("/plugin/decklink/hdr_presets");
        std::vector<std::string> presets_names;

        if (hdr_presets_data_.is_object()) {
            for (auto it = hdr_presets_data_.begin(); it != hdr_presets_data_.end(); it++) {
                presets_names.push_back(it.key());
            }
            hdr_presets_->set_role_data(module::Attribute::StringChoices, presets_names);
        } else {
            throw std::runtime_error("hdr_presets should a json dictionary.");
        }

    } catch (std::exception & e) {
        spdlog::warn("{}: Failed to load perference /plugin/decklink/hdr_presets: {}", __PRETTY_FUNCTION__, e.what());
    }

}

void BMDecklinkPlugin::set_hdr_mode_and_metadata() {

    if (!dcl_output_) return;
    
    HDRMetadata metadata;
    auto p = eotf_modes.find(hdr_mode_->value());
    metadata.EOTF = p != eotf_modes.end() ? p->second : 0;

    if (metadata.EOTF == -1) {
        // auto turn on HDR mode if the ocio display includes the string 'ocio_display_hdr_match_string_'
        // which was set from our preferences. So for example, if ocio_display_hdr_match_string_ is 'hdr'
        // then if the ocio display is 'Screening Room (HDR)' then HDR output is turned on
        if (utility::to_lower(get_ocio_display_name()).find(ocio_display_hdr_match_string_) != std::string::npos) {
            metadata.EOTF = 2; // PQ (Perceptual Quantization or something)
        } else {
            metadata.EOTF = 0;
        }
    }

    auto q = colourspaces.find(colourspace_->value());
    metadata.colourspace_ = q != colourspaces.end() ? q->second : bmdColorspaceRec709;

    for (size_t i = 0; i < hdr_metadata_settings_.size(); ++i) {
        metadata.referencePrimaries[i] = hdr_metadata_settings_[i]->value();
    }

    for (size_t i = 0; i < hdr_metadata_lightlevel_.size(); ++i) {
        metadata.luminanceSettings[i] = hdr_metadata_lightlevel_[i]->value();
    }

    dcl_output_->set_hdr_metadata(metadata);

}

void BMDecklinkPlugin::save_hdr_colour_prefs() {
    
    utility::JsonStore cvals(nlohmann::json::parse("[]"));
    utility::JsonStore lvals(nlohmann::json::parse("[]"));
    for (size_t i = 0; i < hdr_metadata_settings_.size(); ++i) {
        cvals.push_back(hdr_metadata_settings_[i]->value());
    }

    for (size_t i = 0; i < hdr_metadata_lightlevel_.size(); ++i) {
        lvals.push_back(hdr_metadata_lightlevel_[i]->value());
    }

    // Fetch the ocio display name match string for auto HDR mode switching
    auto prefs = global_store::GlobalStoreHelper(system());
    try {
        prefs.set_value(cvals, "/plugin/decklink/hdr_colour_metadata_values");
        prefs.set_value(lvals, "/plugin/decklink/hdr_luminance_metadata_values");
    } catch (std::exception & e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    prefs_save_scheduled_ = false;
}

std::string BMDecklinkPlugin::get_ocio_display_name() {

    if (!colour_pipeline()) return std::string();

    try {

        scoped_actor sys{system()};

        auto ocio_display = utility::request_receive<utility::JsonStore>(
            *sys,
            colour_pipeline(),
            module::attribute_value_atom_v,
            std::string("Display"));

        if (ocio_display.is_string()) {
            return ocio_display.get<std::string>();
        }

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return std::string();

}

BMDecklinkPlugin::~BMDecklinkPlugin() {
}

XSTUDIO_PLUGIN_DECLARE_BEGIN()

XSTUDIO_REGISTER_PLUGIN(
    BMDecklinkPlugin,
    BMDecklinkPlugin::PLUGIN_UUID,
    BlackMagic Decklink Viewport,
    plugin_manager::PluginFlags::PF_VIDEO_OUTPUT,
    false,
    Ted Waine,
    BlackMagic Decklink SDI Output Plugin,
    1.0.0)

XSTUDIO_PLUGIN_DECLARE_END()
