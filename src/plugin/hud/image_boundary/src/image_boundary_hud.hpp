// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/viewport/hud_plugin.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class ImageBoundaryHUD : public HUDPluginBase {
          public:
            ImageBoundaryHUD(caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~ImageBoundaryHUD();

            void attribute_changed(
                const utility::Uuid &attribute_uuid, const int /*role*/
                ) override;

          protected:
            utility::BlindDataObjectPtr prepare_overlay_data(
                const media_reader::ImageBufPtr &, const bool /*offscreen*/) const override;

            plugin::ViewportOverlayRendererPtr make_overlay_renderer(const int) override;

          private:
            module::ColourAttribute *colour_ = nullptr;
            module::IntegerAttribute *width_ = nullptr;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
