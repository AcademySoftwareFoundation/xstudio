// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/ui/opengl/shader_program_base.hpp"
#include "xstudio/ui/opengl/opengl_text_rendering.hpp"
#include "xstudio/ui/viewport/mask.hpp"
#include "xstudio/plugin_manager/hud_plugin.hpp"

namespace xstudio::ui::viewport {

typedef std::vector<std::shared_ptr<const Mask>> MaskPtrVec;
class MaskPtrVecPtr : public std::shared_ptr<MaskPtrVec> {
  public:
    MaskPtrVecPtr() = default;

    MaskPtrVecPtr(const std::vector<Mask> &masks) {
        reset(new MaskPtrVec);
        for (const auto &m : masks) {
            (*this)->emplace_back(std::make_shared<const Mask>(m));
        }
    }
    MaskPtrVecPtr(const MaskPtrVec &mvec)
        : std::shared_ptr<MaskPtrVec>(std::make_shared<MaskPtrVec>(mvec)) {}
    MaskPtrVecPtr(const MaskPtrVecPtr &o)            = default;
    MaskPtrVecPtr &operator=(const MaskPtrVecPtr &o) = default;

    bool in_flight = false;
};

class MaskData : public utility::BlindDataObject, public MaskPtrVec {
  public:
    MaskData()  = default;
    ~MaskData() = default;
};

class MaskRenderer : public plugin::ViewportOverlayRenderer {

  public:
    void render_image_overlay(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float viewport_du_dpixel,
        const float device_pixel_ratio,
        const xstudio::media_reader::ImageBufPtr &frame) override;

  private:
    void render_mask(
        const Imath::M44f &transform_window_to_viewport_space,
        const Imath::M44f &transform_viewport_to_image_space,
        const float viewport_du_dpixel,
        const float device_pixel_ratio,
        const xstudio::media_reader::ImageBufPtr &frame,
        const Mask &mask);

    void init_overlay_opengl();

    std::unique_ptr<xstudio::ui::opengl::GLShaderProgram> shader_;
    GLuint vertex_buffer_object_;
    GLuint vertex_array_object_;
    std::unique_ptr<xstudio::ui::opengl::OpenGLTextRendererSDF> text_renderer_;
    std::vector<float> precomputed_text_vertex_buffer_;
    size_t last_mask_hash_ = 0;
    Imath::Box2i last_bounds_;
    float font_scale_    = 0.0f;
    size_t num_vertices_ = 0;
};

class MaskRendererPlugin : public plugin::StandardPlugin {

  public:
    inline static const utility::Uuid PLUGIN_UUID =
        utility::Uuid("d932fa83-2559-485b-9846-1f4e6c614fea");

    MaskRendererPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings);

    ~MaskRendererPlugin();

    void on_exit() override;

    caf::message_handler message_handler_extensions() override;

  protected:
    utility::BlindDataObjectPtr onscreen_render_data(
        const media_reader::ImageBufPtr &,
        const std::string & /*viewport_name*/,
        const utility::Uuid &playhead_uuid,
        const bool is_hero_image,
        const bool images_are_in_grid_layout) const override;

    plugin::ViewportOverlayRendererPtr
    make_overlay_renderer(const std::string &viewport_name) override {
        return plugin::ViewportOverlayRendererPtr(new MaskRenderer());
    }

    void media_due_on_screen_soon(const media::AVFrameIDsAndTimePoints &) override;

    void on_screen_media_changed(
        const utility::UuidActor &media,
        const utility::UuidActor &media_source,
        const std::string &viewport_name,
        const int playhead_idx,
        const bool is_main_playhead) override;

  private:
    std::map<utility::Uuid, utility::BlindDataObjectPtr> masks_per_media_cache_;

    module::BooleanAttribute *globally_enabled_ = {nullptr};

    caf::actor embedded_python_actor_;

    std::map<utility::Uuid, MaskPtrVecPtr> static_masks_;

    // key is the UUID of the plugin that provides the masks.
    // The key of the sub-map is the media source UUID and the value is the mask(s) for
    // that media source provided by that plugin.
    mutable std::map<utility::Uuid, std::map<utility::Uuid, MaskPtrVecPtr>> media_masks_;
};

} // namespace xstudio::ui::viewport
