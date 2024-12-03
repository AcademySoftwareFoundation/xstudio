// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"

namespace xstudio {
namespace shotbrowser {

    class ShotBrowser : public plugin::StandardPlugin {
      public:
        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("8c0f06a8-cf43-44f3-94e2-0428bf7d150c");

        ShotBrowser(caf::actor_config &cfg, const utility::JsonStore &init_settings);
        virtual ~ShotBrowser();

      protected:
        caf::message_handler message_handler_extensions() override;

        void attribute_changed(
            const utility::Uuid &attribute_uuid, const int /*role*/
            ) override;

        void register_hotkeys() override;
        void hotkey_pressed(
            const utility::Uuid &uuid,
            const std::string &context,
            const std::string &window) override;
        void hotkey_released(const utility::Uuid &uuid, const std::string &context) override;

        void images_going_on_screen(
            const media_reader::ImageBufDisplaySetPtr &images,
            const std::string viewport_name,
            const bool playhead_playing) override;

        void menu_item_activated(const utility::Uuid &menu_item_uuid) override;

      private:
        void setup_menus();

        // Example attributes .. you might not need to use these
        module::StringChoiceAttribute *some_multichoice_attribute_{nullptr};
        module::IntegerAttribute *some_integer_attribute_{nullptr};
        module::BooleanAttribute *some_bool_attribute_{nullptr};
        module::ColourAttribute *some_colour_attribute_{nullptr};

        utility::Uuid demo_hotkey_;
        utility::Uuid my_menu_item_;
    };

} // namespace shotbrowser
} // namespace xstudio