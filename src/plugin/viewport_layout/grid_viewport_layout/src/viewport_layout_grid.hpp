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

    class GridViewportLayout : public ViewportLayoutPlugin {
      public:

        GridViewportLayout(
            caf::actor_config &cfg,
            const utility::JsonStore &init_settings);

        ~GridViewportLayout() override = default;

        inline static const utility::Uuid PLUGIN_UUID = {"b2f442c8-a185-4267-af98-66af43fdce68"};

      protected:

        ViewportRenderer * make_renderer(const std::string &window_id) override;

        void do_layout(
            const std::string &layout_mode,
            const media_reader::ImageBufDisplaySetPtr &image_set,
            media_reader::ImageSetLayoutData &layout_data) override;

        module::FloatAttribute * spacing_;
        module::StringChoiceAttribute * aspect_mode_;


    };
} // namespace viewport
} // namespace ui
} // namespace xstudio
