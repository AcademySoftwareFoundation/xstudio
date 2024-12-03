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

    class WipeViewportLayout : public ViewportLayoutPlugin {
      public:

        WipeViewportLayout(
            caf::actor_config &cfg,
            const utility::JsonStore &init_settings);

        ~WipeViewportLayout() override = default;

        inline static const utility::Uuid PLUGIN_UUID = {"fa41b47a-6fde-4627-bd38-6e7834e75007"};

      protected:

        virtual ViewportRenderer * make_renderer(const std::string &window_id);

        void do_layout(
            const std::string &layout_mode,
            const media_reader::ImageBufDisplaySetPtr &image_set,
            media_reader::ImageSetLayoutData &layout_data
            ) override;

        module::Vec4fAttribute * wipe_position_;


    };
} // namespace viewport
} // namespace ui
} // namespace xstudio
