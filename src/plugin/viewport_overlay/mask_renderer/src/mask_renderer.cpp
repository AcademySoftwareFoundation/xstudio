// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "mask_renderer.hpp"
#include "xstudio/plugin_manager/plugin_base.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/blind_data.hpp"
#include "xstudio/ui/viewport/viewport_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

#ifdef __apple__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#endif

using namespace xstudio;
using namespace xstudio::ui::viewport;

namespace {
const char *vertex_shader = R"(
    #version 330 core
    layout (location = 0) in vec4 aPos;
    out vec2 normImageCoordinate;
    uniform mat4 to_coord_system;
    uniform mat4 to_canvas;
    uniform mat4 image_transform_matrix;
    uniform float extra_scale;

    void main()
    {
        vec4 rpos = vec4(aPos.x*extra_scale, aPos.y*extra_scale, aPos.zw);
        gl_Position = (rpos*image_transform_matrix*to_coord_system*to_canvas);
        normImageCoordinate = aPos.xy;
    }
    )";

const char *frag_shader = R"(
    #version 330 core
    in vec2 normImageCoordinate;
    out vec4 FragColor;
    uniform float mask_safety;
    uniform float mask_aspect_ratio;
    uniform float mask_opac;
    uniform float line_opac;
    uniform float line_thickness;
    uniform float viewport_du_dx;
    uniform float mask_left;
    uniform float mask_top;
    uniform float mask_right;
    uniform float mask_bottom;

    vec4 gen_mask_colour(vec2 uv) {

        float x_p = min(uv.x-mask_left, mask_right-uv.x);
        float y_p = min(uv.y-mask_bottom, mask_top-uv.y);
        float p_p = min(x_p, y_p);

        // compute line colour from the overlap of the line thickness with
        // the fragment - viewport_du_dx etc. tell us how 'wide' a display
        // pixel is in units of the coordinate system of the viewport
        float p_p0 = p_p-viewport_du_dx*0.5;
        float p_p1 = p_p+viewport_du_dx*0.5;

        if (line_opac > 0.0 && line_thickness > 0.0) {
            float lt = line_thickness*viewport_du_dx;

            // slight anti-aliasing for the line so it doesn't flicker when
            // it's thin and viewport is being translated
            float l = min(smoothstep(
                -viewport_du_dx*0.5-lt,
                +viewport_du_dx*0.5-lt,
                p_p), smoothstep(
                +viewport_du_dx*0.5+lt,
                -viewport_du_dx*0.5+lt,
                p_p));

            if (p_p < 0.0) {
                return l < mask_opac ? vec4(0.0,0.0,0.0, mask_opac) : vec4(line_opac, line_opac, line_opac, l*line_opac);
            } else if (l > 0.0) {
                return vec4(line_opac, line_opac, line_opac, l*line_opac);
            } else {
                discard;
            }
        } else if (p_p < 0.0) {
            return vec4(0.0,0.0,0.0, mask_opac);
        } else {
            discard;
        }

    }

    void main(void)
    {
        vec2 uv = vec2(normImageCoordinate.x, normImageCoordinate.y);
        FragColor = gen_mask_colour(uv);
    }

    )";

Imath::V2f get_transformed_point(
    Imath::V2i point,
    Imath::V2i image_dims,
    const float pixel_aspect,
    const Imath::M44f &im_transform) {
    const float aspect = float(image_dims.y) / float(image_dims.x);

    float norm_x = float(point.x) / image_dims.x;
    float norm_y = float(point.y) / image_dims.y;

    auto v =
        Imath::V4f(
            norm_x * 2.0f - 1.0f, (norm_y * 2.0f - 1.0f) * aspect / pixel_aspect, 0.0f, 1.0f) *
        im_transform;

    return Imath::V2f(v.x / v.w, v.y / v.w);
};

void make_tri_verts_for_rectangle(
    const Imath::V2f &top_left,
    const Imath::V2f &bottom_right,
    std::vector<Imath::V4f> &vertices) {
    vertices.push_back(Imath::V4f(top_left.x, top_left.y, 0.0f, 1.0f));
    vertices.push_back(Imath::V4f(top_left.x, bottom_right.y, 0.0f, 1.0f));
    vertices.push_back(Imath::V4f(bottom_right.x, bottom_right.y, 0.0f, 1.0f));
    vertices.push_back(Imath::V4f(bottom_right.x, bottom_right.y, 0.0f, 1.0f));
    vertices.push_back(Imath::V4f(bottom_right.x, top_left.y, 0.0f, 1.0f));
    vertices.push_back(Imath::V4f(top_left.x, top_left.y, 0.0f, 1.0f));
}


} // namespace

void MaskRenderer::render_mask(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame,
    const Mask &mask) {

    // bounds is the image area in union with the bounding box of the pixels that have data in
    // them. This means the mask area will be drawn correctly on overscan areas if there are
    // any.
    const auto image_dims = frame ? frame->image_size_in_pixels() : Imath::V2i(0);
    auto bounds           = frame->image_pixels_bounding_box();
    bounds.extendBy(Imath::Box2i(Imath::V2i(0, 0), frame->image_size_in_pixels()));

    // By expanding the edges of the area that the mask is drawn into we endusre
    // that the mask lines show correctly and aren't cropped off at the edges
    // of the image.
    static const Imath::V2f padding(0.01f, 0.01f);

    // If the media, mediaStream, or mediaSource has a transform (e.g. scaling
    // or re-projection etc.) we DON'T want this to apply to the mask.
    Imath::M44f im_transform = frame.frame_id().transform_matrix();

    Imath::V2f image_top_left =
        get_transformed_point(
            bounds.min, image_dims, frame.frame_id().pixel_aspect(), im_transform) -
        padding;

    Imath::V2f image_bottom_right =
        get_transformed_point(
            bounds.max, image_dims, frame.frame_id().pixel_aspect(), im_transform) +
        padding;

    utility::JsonStore shader_params;
    shader_params["image_transform_matrix"] = im_transform.inverse();
    shader_params["to_coord_system"]        = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]              = transform_window_to_viewport_space;
    shader_params["viewport_du_dx"]         = viewport_du_dpixel;

    // what happens if the top and bottom of the mask go outside the image area? This will
    // happen if the mask is 1.33 aspect, say, and the image is 1.78 aspect. The assumption is
    // that we want to mask off the left and right of the image. At this point the mask has
    // simply been width fitted to the image. So now we apply a scale such that the mask is
    // effectively height fitted to the image.
    float extra_scale = 1.0f;
    if (mask.auto_fit()) {
        const float mask_aspect = (mask.right() - mask.left()) / (mask.top() - mask.bottom());
        const float im_aspect   = image_aspect(frame);
        if (mask_aspect < im_aspect) {
            extra_scale = mask_aspect / im_aspect;
        }
    }
    shader_params["extra_scale"] = extra_scale;

    if (last_mask_hash_ != mask.hash() || bounds != last_bounds_) {

        // we want to avoid doing this work if the mask isn't changing. This
        // optimisation only really helps if there's a single mask on the
        // screen but that is generally the common use case.

        last_bounds_ = bounds;
        std::vector<Imath::V4f> vertices;

        image_bottom_right *= 1.0f / extra_scale;
        image_top_left *= 1.0f / extra_scale;

        // we only want to run the maks shader over the area of the image that
        // is acually masked off, not the unmasked area of the image. So
        // we make 4 rectangles that cover the masked off areas outside the 'letterbox' of the
        // mask

        // left/right pillars that cover masked area left and right of the image
        make_tri_verts_for_rectangle(
            image_top_left,
            Imath::V2f(mask.left() + padding.x, image_bottom_right.y),
            vertices);
        make_tri_verts_for_rectangle(
            Imath::V2f(mask.right() - padding.x, image_top_left.y),
            image_bottom_right,
            vertices);

        // top bottom areas outside the 'letterbox' of the mask area
        make_tri_verts_for_rectangle(
            Imath::V2f(mask.left() + padding.x, mask.bottom() + padding.y),
            Imath::V2f(mask.right() - padding.x, image_top_left.y),
            vertices);
        make_tri_verts_for_rectangle(
            Imath::V2f(mask.left() + padding.x, mask.top() - padding.y),
            Imath::V2f(mask.right() - padding.x, image_bottom_right.y),
            vertices);

        glBindVertexArray(vertex_array_object_);
        // 2. copy our vertices array in a buffer for OpenGL to use
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object_);
        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Imath::V4f),
            vertices.data(),
            GL_STATIC_DRAW);
        // 3. then set our vertex module pointers
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        shader_params["line_thickness"] = mask.line_thickness();
        shader_params["line_opac"]      = mask.line_opacity();
        shader_params["mask_opac"]      = mask.mask_opacity();

        shader_params["mask_left"]   = mask.left();
        shader_params["mask_top"]    = mask.top();
        shader_params["mask_right"]  = mask.right();
        shader_params["mask_bottom"] = mask.bottom();

        num_vertices_ = vertices.size();
    }

    shader_->set_shader_parameters(shader_params);

    shader_->use();
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(vertex_array_object_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(num_vertices_));
    shader_->stop_using();
    glBindVertexArray(0);

    if (mask.label().empty()) {
        last_mask_hash_ = mask.hash();
        return;
    }

    // to make the label a costant size, regardless of the viewport transformation,
    // we scale the font plotting size with 'viewport_du_dx'
    auto font_scale = mask.label_size() * 1920.0f * viewport_du_dpixel * device_pixel_ratio;

    if (font_scale != font_scale_ || last_mask_hash_ != mask.hash()) {
        font_scale_ = font_scale;
        std::ignore = text_renderer_->precompute_text_rendering_vertex_layout(
            precomputed_text_vertex_buffer_,
            mask.label(),
            Imath::V2f(
                mask.left() * extra_scale,
                mask.bottom() * extra_scale -
                    (mask.line_thickness() + 5.0f) * viewport_du_dpixel),
            1.0f,
            font_scale,
            JustifyLeft,
            1.0f);
        last_mask_hash_ = mask.hash();
    }

    text_renderer_->render_text(
        precomputed_text_vertex_buffer_,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space * im_transform,
        utility::ColourTriplet(1.0, 1.0, 1.0),
        viewport_du_dpixel,
        font_scale,
        1.0f);
}

void MaskRenderer::render_image_overlay(
    const Imath::M44f &transform_window_to_viewport_space,
    const Imath::M44f &transform_viewport_to_image_space,
    const float viewport_du_dpixel,
    const float device_pixel_ratio,
    const xstudio::media_reader::ImageBufPtr &frame) {

    const auto *data = frame.plugin_blind_data<const MaskData>(MaskRendererPlugin::PLUGIN_UUID);
    if (!data)
        return;

    if (!shader_)
        init_overlay_opengl();

    for (const auto &mask : *data) {
        render_mask(
            transform_window_to_viewport_space,
            transform_viewport_to_image_space,
            viewport_du_dpixel,
            device_pixel_ratio,
            frame,
            *mask);
    }

    // how car does the mask intrude into the edge of the image?
    /*const float edge = 1.0f-data->mask_shader_params_["mask_safety"].get<float>()/100.0f -
    (data->mask_shader_params_["line_thickness"].get<float>()/1920.0f);

    update_image_edge_vertices(
        frame,
        edge,
        edge/data->mask_shader_params_["mask_aspect_ratio"].get<float>());
            *(data->masks_.at(i)));
    }

    // how car does the mask intrude into the edge of the image?
    const float edge = 1.0f-data->mask_shader_params_["mask_safety"].get<float>()/100.0f -
    (data->mask_shader_params_["line_thickness"].get<float>()/1920.0f);

    update_image_edge_vertices(
        frame,
        edge,
        edge/data->mask_shader_params_["mask_aspect_ratio"].get<float>());

    utility::JsonStore shader_params;
    shader_params["to_coord_system"]        = transform_viewport_to_image_space.inverse();
    shader_params["to_canvas"]              = transform_window_to_viewport_space;
    shader_params["viewport_du_dx"]         = viewport_du_dpixel;
    shader_params["image_transform_matrix"] = frame.layout_transform();
    shader_->set_shader_parameters(shader_params);

    shader_->use();
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(vertex_array_object_);
    glDrawArrays(GL_TRIANGLES, 0, 24);
    shader_->stop_using();
    glBindVertexArray(0);

    auto aspect = data->mask_shader_params_["mask_aspect_ratio"].get<float>();

    // to make the label a costant size, regardless of the viewport transformation,
    // we scale the font plotting size with 'viewport_du_dx'
    auto font_scale = data->label_size_ * 1920.0f * viewport_du_dpixel * device_pixel_ratio;

    auto mask_safety = data->mask_shader_params_["mask_safety"].get<float>();
    auto line_thickness =
        data->mask_shader_params_["line_thickness"].get<float>() * device_pixel_ratio;
    auto ma = mask_safety + aspect + line_thickness;

    if (text_ != data->mask_caption_ || font_scale != font_scale_ || ma != ma_) {
        font_scale_ = font_scale;
        text_       = data->mask_caption_;

        const Imath::V2f text_position =
            Imath::V2f(
                -1.0 - (line_thickness * viewport_du_dpixel),
                -1.0 / aspect - (line_thickness * viewport_du_dpixel) -
                    (viewport_du_dpixel * 24.0f)) *
            ((100.0f - mask_safety) / 100.0f);

        std::ignore = text_renderer_->precompute_text_rendering_vertex_layout(
            precomputed_text_vertex_buffer_,
            text_,
            text_position,
            1.0f,
            font_scale,
            JustifyLeft,
            1.0f);
    }

    text_renderer_->render_text(
        precomputed_text_vertex_buffer_,
        transform_window_to_viewport_space,
        transform_viewport_to_image_space,
        utility::ColourTriplet(1.0, 1.0, 1.0),
        viewport_du_dpixel,
        font_scale,
        data->mask_shader_params_["line_opac"].get<float>());*/
}

void MaskRenderer::init_overlay_opengl() {

    glGenBuffers(1, &vertex_buffer_object_);
    glGenVertexArrays(1, &vertex_array_object_);

    shader_ = std::make_unique<ui::opengl::GLShaderProgram>(vertex_shader, frag_shader);

    text_renderer_ = std::make_unique<ui::opengl::OpenGLTextRendererSDF>(
        utility::xstudio_resources_dir("fonts/Overpass-Regular.ttf"), 96);
}

MaskRendererPlugin::MaskRendererPlugin(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "MaskRenderer", init_settings) {

    globally_enabled_ = add_boolean_attribute("Globally Enabled", "Globally Enabled", false);

    // register the plugin so that other plugins can find it and send it mask data to render
    system().registry().put(mask_renderer_registry, this);
}

MaskRendererPlugin::~MaskRendererPlugin() = default;

void MaskRendererPlugin::on_exit() {
    embedded_python_actor_ = caf::actor();
    system().registry().erase(mask_renderer_registry);
}

caf::message_handler MaskRendererPlugin::message_handler_extensions() {

    // this is where we get details on masks to render from other plugins.

    return caf::message_handler(
               {[=](ui::viewport::hud_settings_atom, bool hud_enabled) {
                    globally_enabled_->set_value(hud_enabled);
                    redraw_viewport();
                },
                /*[=](ui::viewport::hud_settings_atom,
                    const std::string qml_code,
                    HUDElementPosition position) -> bool {
                    return false;
                },*/
                [=](viewport_mask_atom,
                    const utility::Uuid &plugin_uuid,
                    const std::string plugin_name,
                    caf::actor plugin_actor,
                    const bool masks_different_per_media) {
                    // This message is sent from the constructor of the MaskPlugin pythgon
                    // base class.

                    if (plugin_actor && masks_different_per_media) {
                        // we only need to have handles on maks plugins that define
                        // potentially different masks for different bits of media.
                        // For mask plugins that define only a static mask the plugin
                        // will send us the Mask details when it changes.
                        media_masks_[plugin_uuid] = std::map<utility::Uuid, MaskPtrVecPtr>();
                    }

                    if (!embedded_python_actor_) {
                        embedded_python_actor_ = system().registry().template get<caf::actor>(
                            xstudio::embedded_python_registry);
                    }

                    if (!masks_different_per_media && embedded_python_actor_) {
                        // fetch static mask(s) from the plugin
                        mail(
                            embedded_python::python_exec_atom_v,
                            ui::viewport::viewport_mask_atom_v,
                            plugin_uuid,
                            caf::actor())
                            .request(embedded_python_actor_, infinite)
                            .then(
                                [=](const std::vector<Mask> &masks) {
                                    static_masks_[plugin_uuid] = MaskPtrVecPtr(masks);
                                },
                                [=](caf::error &err) {

                                });
                    }
                },

                [=](viewport_mask_atom, const utility::Uuid &plugin_uuid, const bool enabled) {
                    if (!enabled) {
                        redraw_viewport();
                    }
                },

                [=](viewport_mask_atom,
                    utility::change_atom,
                    const utility::Uuid &plugin_uuid) {
                    // this message is sent by a MaskPlugin python plugin when one of it's
                    // attributes changes. we need to redraw the viewport because the mask
                    // details may have changed.
                    auto p = media_masks_.find(plugin_uuid);
                    if (p != media_masks_.end()) {
                        // if this is a plugin that defines different masks for different media
                        // sources we need to clear the cache of masks we have for each media
                        // source because they may now be out of date.
                        p->second.clear();
                    }
                    redraw_viewport();
                },

                [=](viewport_mask_atom,
                    const utility::Uuid &plugin_uuid,
                    const std::vector<Mask> &static_masks) {
                    static_masks_[plugin_uuid] = MaskPtrVecPtr(static_masks);
                    redraw_viewport();
                }})
        .or_else(StandardPlugin::message_handler_extensions());
}

void MaskRendererPlugin::on_screen_media_changed(
    const utility::UuidActor &media,
    const utility::UuidActor &media_source,
    const std::string &viewport_name,
    const int playhead_idx,
    const bool is_main_playhead) {

    if (is_main_playhead && globally_enabled_->value() && playhead_idx == 0) {
        // mask plugins that define different masks for different media sources
        // might want to know which media source is now on screen so it can
        // modify it's configuration options accordingly.
        for (auto &mp : media_masks_) {
            // mp.first is the UUID of the Python plugin that provides masks
            // that may vary per media source.
            mail(
                embedded_python::python_exec_atom_v,
                ui::viewport::viewport_mask_atom_v,
                playhead::show_atom_v,
                media_source.actor(),
                mp.first)
                .send(embedded_python_actor_);
        }
    }
}

void MaskRendererPlugin::media_due_on_screen_soon(
    const media::AVFrameIDsAndTimePoints &frame_ids) {

    // this callback is made during playback - it gives us a list of frame IDs, one for each
    // individual piece of media that is expected to be put on the screen in the immediate
    // future

    // there is one entry in media_masks_ PER PLUGIN (if the mask plugin defines
    // different masks for different media sources). As such, there is only
    // likely to be one of these plugins as most VFX/ANIM studios would have
    // a single plugin implementation to define masks for the xSTUDIO viewport.
    for (auto &mp : media_masks_) {

        const auto plugin_id = mp.first;

        utility::UuidActorVector media_source_actors;

        // loop over the incoming frameIds ... find the ones that we haven't
        // already requested masks for
        for (const auto &p : frame_ids) {

            if (!mp.second[p.second->source_uuid()] &&
                !mp.second[p.second->source_uuid()].in_flight) {
                // add an entry for this media source so we don't re-attempt to
                // fetch the mask data.
                mp.second[p.second->source_uuid()].in_flight = true;
                media_source_actors.push_back(p.second->media_source_actor());
            }
        }

        // we haven't got mask data for this media source yet. Request it from the python
        // plugin(s).
        mail(
            embedded_python::python_exec_atom_v,
            ui::viewport::viewport_mask_atom_v,
            mp.first,
            media_source_actors)
            .request(embedded_python_actor_, std::chrono::milliseconds(1000))
            .then(
                [=](const std::vector<std::vector<Mask>> &masks) mutable {
                    if (masks.size() != media_source_actors.size()) {
                        spdlog::warn(
                            "{} {}",
                            __PRETTY_FUNCTION__,
                            "Number of masks returned from python plugin doesn't match number "
                            "of media sources requested");
                        for (auto &p2 : media_source_actors) {
                            media_masks_[plugin_id][p2.uuid()].in_flight = false;
                        }
                        return;
                    }
                    auto p2 = media_source_actors.begin();
                    for (const auto &mask_vec : masks) {
                        MaskPtrVec mvec;
                        for (const auto &m : mask_vec) {
                            mvec.emplace_back(std::make_shared<const Mask>(m));
                        }
                        media_masks_[plugin_id][(*p2).uuid()] = MaskPtrVecPtr(mvec);
                        p2++;
                    }
                },
                [=](caf::error &err) mutable {
                    for (auto &p2 : media_source_actors) {
                        media_masks_[plugin_id][p2.uuid()].in_flight = false;
                    }
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                });
    }
}


utility::BlindDataObjectPtr MaskRendererPlugin::onscreen_render_data(
    const media_reader::ImageBufPtr &image,
    const std::string &viewport_name,
    const utility::Uuid &playhead_uuid,
    const bool is_hero_image,
    const bool images_are_in_grid_layout) const {

    auto data = new MaskData();

    for (const auto &p : static_masks_) {
        if (p.second) {
            data->insert(data->end(), p.second->begin(), p.second->end());
        }
    }

    for (auto &mp : media_masks_) {

        MaskPtrVecPtr &media_masks = mp.second[image.frame_id().source_uuid()];

        if (!media_masks) {
            // we haven't got mask data for this media source yet. Request it from the python
            // plugin(s). This is a synchronous request and remember that onscreen_render_data
            // is called in the critical path of rendering the viewport, so we want to avoid
            // doing this if possible. That's why we also do this request in
            // media_due_on_screen_soon - ideally we will have the data by the time we get here.
            // If we need to do this we use a short timeout s if the python plugin is slow.
            try {

                const auto masks = utility::request_receive_wait<std::vector<Mask>>(
                    *(scoped_actor{system()}),
                    embedded_python_actor_,
                    std::chrono::milliseconds(10),
                    embedded_python::python_exec_atom_v,
                    ui::viewport::viewport_mask_atom_v,
                    mp.first,
                    image.frame_id().media_source_actor().actor());

                media_masks = MaskPtrVecPtr(masks);

            } catch (std::exception &e) {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }

        if (media_masks) {
            data->insert(data->end(), media_masks->begin(), media_masks->end());
        }
    }

    return utility::BlindDataObjectPtr(data);
}

XSTUDIO_PLUGIN_DECLARE_BEGIN()

XSTUDIO_REGISTER_PLUGIN(
    MaskRendererPlugin,
    MaskRendererPlugin::PLUGIN_UUID,
    Mask Renderer Plugin,
    plugin_manager::PluginFlags::PF_HEAD_UP_DISPLAY | plugin_manager::PluginFlags::PF_VIEWPORT_OVERLAY,
    true,
    Ted Waine,
    Plugin that renders masks in the viewport - this can be used by other plugins that provide mask info for media sources going on-screen,
    1.0.0)

XSTUDIO_PLUGIN_DECLARE_END()
