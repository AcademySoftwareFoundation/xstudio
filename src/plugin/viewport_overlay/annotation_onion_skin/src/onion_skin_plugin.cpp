// SPDX-License-Identifier: Apache-2.0
#include "onion_skin_plugin.hpp"
#include "onion_skin_render_data.hpp"
#include "onion_skin_renderer.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/utility/blind_data.hpp"

#include <algorithm>
#include <cmath>
#include <variant>

using namespace xstudio;
using namespace xstudio::ui::viewport;

OnionSkinPlugin::OnionSkinPlugin(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::HUDPluginBase(cfg, "Annotation Onion Skin", init_settings, 10.0f) {

    frames_before_ = add_integer_attribute("Frames Before", "Before", 3, 0, 20);
    add_hud_settings_attribute(frames_before_);
    frames_before_->set_tool_tip(
        "Maximum frame distance to look back for annotations");
    frames_before_->set_redraw_viewport_on_change(true);

    frames_after_ = add_integer_attribute("Frames After", "After", 3, 0, 20);
    add_hud_settings_attribute(frames_after_);
    frames_after_->set_tool_tip(
        "Maximum frame distance to look ahead for annotations");
    frames_after_->set_redraw_viewport_on_change(true);

    base_opacity_ =
        add_float_attribute("Base Opacity", "Opacity", 0.4f, 0.05f, 1.0f, 0.05f);
    add_hud_settings_attribute(base_opacity_);
    base_opacity_->set_tool_tip("Opacity of the nearest neighboring annotation");
    base_opacity_->set_redraw_viewport_on_change(true);

    opacity_falloff_ =
        add_float_attribute("Opacity Falloff", "Falloff", 0.5f, 0.1f, 1.0f, 0.05f);
    add_hud_settings_attribute(opacity_falloff_);
    opacity_falloff_->set_tool_tip(
        "Multiplier applied per frame step further from current frame");
    opacity_falloff_->set_redraw_viewport_on_change(true);

    use_original_colours_ = add_boolean_attribute(
        "Use Original Colours", "Orig Colours", false);
    add_hud_settings_attribute(use_original_colours_);
    use_original_colours_->set_tool_tip(
        "When enabled, keep annotation colours and only reduce opacity. "
        "When disabled, tint with Previous/Next colours.");
    use_original_colours_->set_redraw_viewport_on_change(true);

    past_tint_ = add_colour_attribute(
        "Previous Tint", "Prev Tint", utility::ColourTriplet(1.0f, 0.3f, 0.3f));
    add_hud_settings_attribute(past_tint_);
    past_tint_->set_tool_tip("Tint colour for annotations from previous frames");
    past_tint_->set_redraw_viewport_on_change(true);

    future_tint_ = add_colour_attribute(
        "Next Tint", "Next Tint", utility::ColourTriplet(0.3f, 1.0f, 0.3f));
    add_hud_settings_attribute(future_tint_);
    future_tint_->set_tool_tip("Tint colour for annotations from future frames");
    future_tint_->set_redraw_viewport_on_change(true);

    add_hud_description(
        "Shows annotations from neighboring frames as semi-transparent, "
        "color-tinted overlays on the current frame.");

    frames_before_->set_preference_path("/plugin/annotation_onion_skin/frames_before");
    frames_after_->set_preference_path("/plugin/annotation_onion_skin/frames_after");
    base_opacity_->set_preference_path("/plugin/annotation_onion_skin/base_opacity");
    opacity_falloff_->set_preference_path("/plugin/annotation_onion_skin/opacity_falloff");
    use_original_colours_->set_preference_path("/plugin/annotation_onion_skin/use_original_colours");
    past_tint_->set_preference_path("/plugin/annotation_onion_skin/past_tint");
    future_tint_->set_preference_path("/plugin/annotation_onion_skin/future_tint");
}

plugin::ViewportOverlayRendererPtr
OnionSkinPlugin::make_overlay_renderer(const std::string & /*viewport_name*/) {
    return plugin::ViewportOverlayRendererPtr(new OnionSkinRenderer());
}

utility::BlindDataObjectPtr OnionSkinPlugin::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string & /*viewport_name*/,
    const utility::Uuid & /*playhead_uuid*/,
    const bool /*is_hero_image*/,
    const bool /*images_are_in_grid_layout*/) const {

    if (!visible() || !image)
        return {};

    const int current_frame   = image.playhead_logical_frame();
    const int range_before    = static_cast<int>(frames_before_->value());
    const int range_after     = static_cast<int>(frames_after_->value());
    const float base_opac     = base_opacity_->value();
    const float falloff       = opacity_falloff_->value();
    const bool orig_colours   = use_original_colours_->value();
    const auto &prev_colour   = past_tint_->value();
    const auto &next_colour   = future_tint_->value();

    if (range_before == 0 && range_after == 0)
        return {};

    // ── Update bookmark cache ──
    // image.bookmarks() carries bookmarks covering the current frame.
    // We cache them keyed by logical frame to find neighbors later.
    //
    // Invalidation: when revisiting a frame, if its bookmarks changed
    // (different UUIDs or count), we clear the entire cache. This handles
    // bookmark deletion, media changes, and bookmark additions.
    const auto &frame_bookmarks = image.bookmarks();
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto it = frame_bookmark_cache_.find(current_frame);
        if (it != frame_bookmark_cache_.end()) {
            bool changed = (it->second.size() != frame_bookmarks.size());
            if (!changed) {
                for (size_t i = 0; i < it->second.size(); ++i) {
                    if (it->second[i]->detail_.uuid_ !=
                        frame_bookmarks[i]->detail_.uuid_) {
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) {
                frame_bookmark_cache_.clear();
            }
        }

        if (!frame_bookmarks.empty()) {
            frame_bookmark_cache_[current_frame] = frame_bookmarks;
        } else {
            frame_bookmark_cache_.erase(current_frame);
        }
    }

    // Collect current frame's annotation pointers — skip these when
    // walking neighbors (same annotation spans multiple frames).
    std::set<const void *> current_annotations;
    for (const auto &bm : frame_bookmarks) {
        if (bm && bm->annotation_ && bm->annotation_->user_data())
            current_annotations.insert(bm->annotation_->user_data());
    }

    // ── Helpers ──
    auto tint_colour = [](const utility::ColourTriplet &c,
                          const utility::ColourTriplet &tint) -> utility::ColourTriplet {
        return {c.r * tint.r, c.g * tint.g, c.b * tint.b};
    };

    auto make_canvas_copy = [&](const ui::canvas::Canvas &src, float opacity,
                                const utility::ColourTriplet &tint,
                                bool keep_original) -> ui::canvas::Canvas {
        ui::canvas::Canvas out(src);
        for (auto it = out.begin(); it != out.end(); ++it) {
            auto item = *it;
            std::visit(
                [&](auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, ui::canvas::Stroke>) {
                        v.set_opacity(v.opacity() * opacity);
                        if (!keep_original)
                            v.set_colour(tint_colour(v.colour(), tint));
                    } else if constexpr (std::is_same_v<T, ui::canvas::Caption>) {
                        v.set_opacity(v.opacity() * opacity);
                        v.set_bg_opacity(v.background_opacity() * opacity);
                        if (!keep_original)
                            v.set_colour(tint_colour(v.colour(), tint));
                    } else {
                        v.opacity *= opacity;
                        if (!keep_original)
                            v.colour = tint_colour(v.colour, tint);
                    }
                },
                item);
            out.overwrite_item(it, item);
        }
        return out;
    };

    // Opacity falls off with distance: nearest = base_opac, farther = less.
    auto compute_opacity = [&](int distance) -> float {
        return base_opac * std::pow(falloff, static_cast<float>(distance - 1));
    };

    // ── Find neighbor annotations from cache (distance-bounded) ──
    struct Candidate {
        const ui::canvas::Canvas *canvas;
        int abs_distance;
        float opacity;
        utility::ColourTriplet tint;
    };
    std::vector<Candidate> candidates;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Walk backward — stop when distance exceeds range_before.
        if (range_before > 0) {
            auto it = frame_bookmark_cache_.lower_bound(current_frame);
            if (it != frame_bookmark_cache_.begin()) {
                auto pit = it;
                while (pit != frame_bookmark_cache_.begin()) {
                    --pit;
                    int dist = current_frame - pit->first;
                    if (dist > range_before)
                        break;
                    for (const auto &bm : pit->second) {
                        if (!bm || !bm->annotation_ || !bm->annotation_->user_data())
                            continue;
                        const auto *canvas = static_cast<const ui::canvas::Canvas *>(
                            bm->annotation_->user_data());
                        if (!canvas || canvas->empty())
                            continue;
                        if (current_annotations.count(canvas))
                            continue;
                        candidates.push_back(
                            {canvas, dist, compute_opacity(dist), prev_colour});
                        break;
                    }
                }
            }
        }

        // Walk forward — stop when distance exceeds range_after.
        if (range_after > 0) {
            auto it = frame_bookmark_cache_.upper_bound(current_frame);
            while (it != frame_bookmark_cache_.end()) {
                int dist = it->first - current_frame;
                if (dist > range_after)
                    break;
                for (const auto &bm : it->second) {
                    if (!bm || !bm->annotation_ || !bm->annotation_->user_data())
                        continue;
                    const auto *canvas = static_cast<const ui::canvas::Canvas *>(
                        bm->annotation_->user_data());
                    if (!canvas || canvas->empty())
                        continue;
                    if (current_annotations.count(canvas))
                        continue;
                    candidates.push_back(
                        {canvas, dist, compute_opacity(dist), next_colour});
                    break;
                }
                ++it;
            }
        }
    }

    if (candidates.empty())
        return {};

    // Render farthest first so closest onion skin draws on top.
    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) { return a.abs_distance > b.abs_distance; });

    std::vector<ui::canvas::Canvas> canvases;
    canvases.reserve(candidates.size());
    for (const auto &c : candidates) {
        canvases.push_back(make_canvas_copy(*c.canvas, c.opacity, c.tint, orig_colours));
    }

    return std::make_shared<OnionSkinRenderData>(std::move(canvases));
}


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<OnionSkinPlugin>>(
                OnionSkinPlugin::PLUGIN_UUID,
                "AnnotationOnionSkin",
                plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY |
                    plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
                true,
                "RodeoFX",
                "Annotation Onion Skinning Overlay")}));
}
}
