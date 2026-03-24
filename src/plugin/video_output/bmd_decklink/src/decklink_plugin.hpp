// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/viewport/video_output_plugin.hpp"

CAF_PUSH_WARNINGS
#include <QQmlExtensionPlugin>
CAF_POP_WARNINGS

namespace xstudio {
namespace bm_decklink_plugin_1_0 {

    class DecklinkOutput;

    class BMDecklinkPlugin : public ui::viewport::VideoOutputPlugin {

      public:
        BMDecklinkPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings);
        virtual ~BMDecklinkPlugin();

        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("432c2c46-e77e-43b0-b0c0-117669d488bf");

        // This method should be implemented to allow cleanup of any/all resources
        // relating to the video output
        void exit_cleanup() override;

        // This method is called when a new image buffer is ready to be displayed
        void incoming_video_frame_callback(media_reader::ImageBufPtr incoming) override;

        // Allocate your resources needed for video output, initialise hardware etc. in
        // this function
        void initialise() override;

        audio::AudioOutputDevice *
        make_audio_output_device(const utility::JsonStore &prefs) override;

        // This is used to communicate between the DecklinkOutput, which executes callbacks
        // in a thread managed by the Decklink driver, and the xstudio threads
        void receive_status_callback(const utility::JsonStore &status_data) override;

      private:
      protected:
        void
        attribute_changed(const utility::Uuid &attribute_uuid, const int /*role*/) override;

        void set_pc_audio_muting();

        DecklinkOutput *dcl_output_ = nullptr;

        module::StringChoiceAttribute *pixel_formats_{nullptr};
        module::StringChoiceAttribute *resolutions_{nullptr};
        module::StringChoiceAttribute *frame_rates_{nullptr};
        module::StringAttribute *status_message_{nullptr};
        module::BooleanAttribute *is_in_error_{nullptr};
        module::BooleanAttribute *sdi_output_is_running_{nullptr};
        module::BooleanAttribute *start_stop_{nullptr};
        module::BooleanAttribute *track_main_viewport_{nullptr};
        module::BooleanAttribute *auto_start_{nullptr};
        module::BooleanAttribute *disable_pc_audio_when_running_{nullptr};
        module::IntegerAttribute *samples_water_level_{nullptr};
        module::IntegerAttribute *audio_sync_delay_milliseconds_{nullptr};
        module::IntegerAttribute *video_pipeline_delay_milliseconds_{nullptr};

        /* HDR settings */
        module::StringChoiceAttribute *hdr_mode_{nullptr};
        module::StringChoiceAttribute *colourspace_{nullptr};
        module::StringChoiceAttribute *hdr_presets_{nullptr};
        std::vector<module::FloatAttribute *> hdr_metadata_settings_;
        std::vector<module::FloatAttribute *> hdr_metadata_lightlevel_;
        std::set<utility::Uuid> hdr_metadata_settings_uuids_;

        std::string ocio_display_hdr_match_string_;
        utility::JsonStore hdr_presets_data_;
        bool prefs_save_scheduled_ = {false};

        void make_hdr_attributes();
        void set_hdr_mode_and_metadata();
        void save_hdr_colour_prefs();
        std::string get_ocio_display_name();
    };

    // We require this boiler-plate to register our custom class as a QML
    // item:

    //![plugin]
    class DecklinkQML : public QQmlExtensionPlugin {
        Q_OBJECT

        Q_PLUGIN_METADATA(IID "xstudio-project.decklink.ui")
        void registerTypes(const char *uri) override {}
    };
    //![plugin]

} // namespace bm_decklink_plugin_1_0
} // namespace xstudio
