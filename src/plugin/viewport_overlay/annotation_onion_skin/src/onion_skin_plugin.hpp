// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>
#include <mutex>
#include <set>

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"
#include "xstudio/ui/canvas/canvas.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class OnionSkinRenderer;

        class OnionSkinPlugin : public plugin::HUDPluginBase {
          public:
            inline static const utility::Uuid PLUGIN_UUID{
                "b7e3a1c0-5d4f-4e8b-9a2c-1f6d8e0b3c5a"};

            OnionSkinPlugin(
                caf::actor_config &cfg, const utility::JsonStore &init_settings);

            ~OnionSkinPlugin() override = default;

          protected:
            utility::BlindDataObjectPtr onscreen_render_data(
                const media_reader::ImageBufPtr &image,
                const std::string &viewport_name,
                const utility::Uuid &playhead_uuid,
                const bool is_hero_image,
                const bool images_are_in_grid_layout) const override;

            plugin::ViewportOverlayRendererPtr
            make_overlay_renderer(const std::string &viewport_name) override;

          private:
            module::IntegerAttribute *frames_before_;
            module::IntegerAttribute *frames_after_;
            module::FloatAttribute *base_opacity_;
            module::FloatAttribute *opacity_falloff_;
            module::BooleanAttribute *use_original_colours_;
            module::ColourAttribute *past_tint_;
            module::ColourAttribute *future_tint_;

            // Bookmark cache: built from image.bookmarks() as user scrubs.
            // Invalidated when bookmarks change (detected by comparing
            // bookmark UUIDs for revisited frames).
            mutable std::mutex cache_mutex_;
            mutable std::map<int, bookmark::BookmarkAndAnnotations> frame_bookmark_cache_;
        };

    } // namespace viewport
} // namespace ui
} // namespace xstudio
