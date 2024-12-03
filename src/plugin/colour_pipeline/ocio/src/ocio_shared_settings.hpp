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

        utility::JsonStore user_view_display_settings_;

        std::vector<caf::actor> watchers_;
    };

} // namespace colour_pipeline
} // namespace xstudio
