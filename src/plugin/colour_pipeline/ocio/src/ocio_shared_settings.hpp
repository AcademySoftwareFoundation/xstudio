// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "ui_text.hpp"

namespace xstudio {
namespace colour_pipeline {

    /* OCIOGlobalData is used for communications to OCIOPlugin(s). */
    struct OCIOGlobalData {
        bool colour_bypass{false};
        bool global_view{true};
        bool adjust_source{true};
        std::string preferred_view;
        std::string default_config;

        bool operator==(const OCIOGlobalData &other) const {
            return colour_bypass == other.colour_bypass and global_view == other.global_view and
                   adjust_source == other.adjust_source and
                   preferred_view == other.preferred_view and
                   default_config == other.default_config;
        }

        bool operator!=(const OCIOGlobalData &other) const { return not(*this == other); }
    };

    void from_json(const nlohmann::json &j, OCIOGlobalData &d);
    void to_json(nlohmann::json &j, const OCIOGlobalData &d);

    /*  OCIOGlobalControls provides application wide colour management settings
        and facilitate cross viewport settings synchronization.

        It is instanciated once for the whole application.
    */
    class OCIOGlobalControls : public plugin::StandardPlugin {

      public:
        explicit OCIOGlobalControls(
            caf::actor_config &cfg, const utility::JsonStore &init_settings);

        void attribute_changed(const utility::Uuid &attribute_uuid, const int role) override;

        caf::message_handler message_handler_extensions() override;

        void on_exit() override;

        void connect_to_viewport(
            const std::string &viewport_name,
            const std::string &viewport_toolbar_name,
            bool connect,
            caf::actor viewport) override;

        inline static std::string NAME() { return "OCIO_GLOBAL_CONTROLS"; }

      private:
        utility::JsonStore settings_json();

        void synchronize_attributes();

      private:
        UiText ui_text_;
        bool ui_initialized_{false};

        // General settings
        module::BooleanAttribute *colour_bypass_;
        module::BooleanAttribute *global_view_;
        module::BooleanAttribute *adjust_source_;
        module::StringChoiceAttribute *preferred_view_;
        module::Attribute *user_view_display_settings_attr_;
        module::StringAttribute *user_ocio_config_;
        module::StringAttribute *default_ocio_config_;

        utility::JsonStore user_view_display_settings_;

        std::vector<caf::actor> watchers_;
        std::set<std::string> bad_configs_;
        std::map<std::string, std::string> builting_config_name_to_ui_name_;
    };

} // namespace colour_pipeline
} // namespace xstudio
