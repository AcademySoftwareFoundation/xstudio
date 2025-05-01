// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/viewport/viewport_layout_plugin.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        /**
         *  @brief GridViewportLayout class.
         *
         *  @details
         *  This plugin provides a layout plugin for arranging multiple images
         *  in a grid
         */

        class CompositeViewportLayout : public ViewportLayoutPlugin {
          public:
            CompositeViewportLayout(
                caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~CompositeViewportLayout() override = default;

            inline static const utility::Uuid PLUGIN_UUID = {
                "fdb9204f-f00f-4345-8616-b7bfbedc5829"};

          protected:
            virtual ViewportRenderer *
            make_renderer(const std::string &window_id, const utility::JsonStore &prefs);

            void do_layout(
                const std::string &layout_mode,
                const media_reader::ImageBufDisplaySetPtr &image_set,
                media_reader::ImageSetLayoutData &layout_data) override;

            module::FloatAttribute *blend_ratio_;
            module::FloatAttribute *difference_boost_;
            module::BooleanAttribute *monochrome_;
        };
    } // namespace viewport
} // namespace ui
} // namespace xstudio
