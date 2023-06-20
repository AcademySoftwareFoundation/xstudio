// SPDX-License-Identifier: Apache-2.0
#pragma once


// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"

#define NO_FRAME INT_MIN


namespace xstudio {
namespace ui {
    namespace viewport {

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
            enum HUDElementPosition {
                BottomLeft,
                BottomCenter,
                BottomRight,
                TopLeft,
                TopCenter,
                TopRight
            };

            HUDPluginBase(
                caf::actor_config &cfg,
                std::string name,
                const utility::JsonStore &init_settings);
            ~HUDPluginBase() override = default;

            /**
             *  @brief Declare the name of the HUD element
             *
             *  @details This string value is used to populate the list of hud elements shown on
             * the HUD pop-up menu in the UI. Can be overridden
             */
            virtual const std::string &hud_name() const { return Module::name(); }

          protected:
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
            void hud_element_qml(const std::string qml_code, const HUDElementPosition position);

            caf::message_handler message_handler_extensions() override {
                return message_handler_;
            }

            /**
             *  @brief Determines if HUD should be drawn to viewport at given moment
             */
            bool visible() const { return globally_enabled_ && enabled_->value(); }

            caf::message_handler message_handler_;

            module::BooleanAttribute *enabled_;
            bool globally_enabled_ = {true};
        };
    } // namespace viewport
} // namespace ui
} // namespace xstudio
