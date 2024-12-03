// SPDX-License-Identifier: Apache-2.0
#pragma once


#include "xstudio/ui/viewport/viewport_layout_plugin.hpp"

#define NO_FRAME INT_MIN


namespace xstudio {
namespace ui {
namespace viewport {

    /**
     *  @brief DefaultViewportLayout class.
     *
     *  @details
     *  This plugin provides the default viewport layout behviour. It simply 
     *  plots a single image to the viewport fitted to the -1.0 < x < 1.0 and
     *  centered on (0.0, 0.0). 
     *
     *  When there are multiple playhead sources selected it simply plots the 
     *  'hero' image and ignores other image streams.
     *  No compositing or multi-image layouts are available.
     *
     *  It provides the 'String' and 'Off'
     */

    class DefaultViewportLayout : public ViewportLayoutPlugin {
      public:

        DefaultViewportLayout(
            caf::actor_config &cfg,
            const utility::JsonStore &init_settings);

        ~DefaultViewportLayout() override = default;

        inline static const utility::Uuid PLUGIN_UUID = {"f7d63ed9-80ed-4ce9-8b39-1d5e5079ce4b"};

        ViewportRenderer * make_renderer(const std::string &window_id) override;

    };
} // namespace viewport
} // namespace ui
} // namespace xstudio
