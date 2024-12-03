// SPDX-License-Identifier: Apache-2.0
#pragma once


// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

#define NO_FRAME INT_MIN


namespace xstudio {
namespace plugin {

    /**
     *  @brief HUDPluginBase class.
     *
     *  @details
     *   Subclass to create custom HUDs in xStudio - which can be activated and configured
     *   from the 'HUD' toolbar button. The HUD graphics/text overlay can be implemented
     *   either as QML or an OpenGL renderer. Refer to the PixelProbe plugin example for
     *   a reference implementation.
     */

    class HUDPluginBase : public plugin::StandardPlugin {
      public:
        HUDPluginBase(
            caf::actor_config &cfg,
            std::string name,
            const utility::JsonStore &init_settings,
            const float toolbar_position = -1.0f);
        ~HUDPluginBase() override = default;

        /**
         *  @brief Declare the name of the HUD element
         *
         *  @details This string value is used to populate the list of hud elements shown on
         * the HUD pop-up menu in the UI. Can be overridden
         */
        virtual const std::string &hud_name() const { return Module::name(); }

      protected:
        void attribute_changed(const utility::Uuid &attribute_uuid, const int role) override;

        /**
         *  @brief Make a given attribute visible in the settings panel for the HUD plugin
         *
         *  @details Calling this with an attribte will ensure that the attribute will
         * appear in the settings pop-up dialog for the plugin
         */
        void add_hud_settings_attribute(module::Attribute *attr);

        /**
         *  @brief Declare qml code that adds to the HUD. Plugins are responsible for
         * providing attribute data and linking up with the qml code etc. See pixel_probe
         * plugin for a reference example.
         */
        void hud_element_qml(
            const std::string qml_code, const HUDElementPosition position = FullScreen);

        caf::message_handler message_handler_extensions() override { return message_handler_; }

        /**
         *  @brief Determines if HUD should be drawn to viewport at given moment
         */
        bool visible() const { return globally_enabled_ && hud_data_->value(); }

        caf::message_handler message_handler_;

        module::BooleanAttribute *hud_data_;
        module::StringChoiceAttribute *hud_item_position_ = {nullptr};
        bool globally_enabled_                            = {false};
        std::string plugin_underscore_name_;

        static inline std::map<int, std::string> position_names_ = {
            {BottomLeft, "HUD Bottom Left"},
            {BottomCenter, "HUD Bottom Centre"},
            {BottomRight, "HUD Bottom Right"},
            {TopLeft, "HUD Top Left"},
            {TopCenter, "HUD Top Centre"},
            {TopRight, "HUD Top Right"}};
    };
} // namespace plugin
} // namespace xstudio
