// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "viewport_layout_grid.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio;

GridViewportLayout::GridViewportLayout(
    caf::actor_config &cfg,
    const utility::JsonStore &init_settings)
    : ViewportLayoutPlugin(cfg, init_settings) {

    spacing_ = add_float_attribute("Spacing", "Spacing", 0.0f, 0.0f, 50.0f, 0.5f);
    spacing_->set_redraw_viewport_on_change(true);
    spacing_->set_role_data(module::Attribute::ToolTip, "Spacing between images in grid layout as a % of image size.");
    add_layout_settings_attribute(spacing_, "Grid");

    aspect_mode_ = add_string_choice_attribute(
        "Cell Aspect",
        "Cell Aspect",
        "Auto",
        std::vector<std::string>({"Auto", "Hero", "16:9", "1.89"}));
    aspect_mode_->set_redraw_viewport_on_change(true);
    aspect_mode_->set_role_data(
        module::Attribute::ToolTip,
R"(Set how the cell aspect is set. Auto means the most common image aspect in the 
set of images being displayed is used. Hero means that the current 'hero' image
sets the aspect of all cells in the layout. 16:9 or 1.89 forces equivalent aspect.)");
    add_layout_settings_attribute(aspect_mode_, "Grid");
    aspect_mode_->set_preference_path("/plugin/viewport_layout/grid_layout_aspect_mode");

    add_layout_mode("Grid", playhead::AssemblyMode::AM_ALL);
    add_layout_mode("Horizontal", playhead::AssemblyMode::AM_ALL);
    add_layout_mode("Vertical", playhead::AssemblyMode::AM_ALL);

}

void GridViewportLayout::do_layout(
    const std::string &layout_mode,
    const media_reader::ImageBufDisplaySetPtr &image_set,
    media_reader::ImageSetLayoutData &layout_data
    )
{
    const int num_images = image_set->num_onscreen_images();
    if (!num_images) {
        return;
    }

    if (!image_set) return;

    const auto hero_image = image_set->hero_image();
    
    float hero_aspect = hero_image ? hero_image->image_aspect() : 16.0f/9.0f;
    if (aspect_mode_->value() == "Auto") {
        std::map<float, int> aspects;
        for (int i = 0; i < num_images; ++i) {
            const auto & im = image_set->onscreen_image(i);
            if (im) {
                float a = im->image_aspect();
                if (aspects.count(a)) {
                    aspects[a]++;
                } else {
                    aspects[a] = 1;
                }
            }
        }
        int c = 0;
        for (const auto &p: aspects) {
            if (p.second > c) {
                c = p.second;
                hero_aspect = p.first;
            }
        }
    } else if (hero_aspect && aspect_mode_->value() == "Hero") {
    } else if (aspect_mode_->value() == "16:9") {
        hero_aspect = 16.0f/9.0f;
    } else if (aspect_mode_->value() == "1.89") {
        hero_aspect = 2048.0f/1080.0f;
    }

    layout_data.image_transforms_.resize(image_set->num_onscreen_images());

    int num_rows = layout_mode == "Grid" ? (int)round(sqrt(float(num_images))) : layout_mode == "Vertical" ? num_images : 1;
    int num_cols = layout_mode == "Grid" ? (int)ceil(float(num_images) / float(num_rows)) : layout_mode == "Vertical" ? 1 : num_images;

    layout_data.layout_aspect_ = (hero_aspect)*float(num_cols)/float(num_rows);

    float scale = (1.0f-spacing_->value()/100.0f)/float(num_cols);

    float x_off = -1.0f + 1.0f/num_cols;
    float y_off = (-1.0f + 1.0f/num_rows)/layout_data.layout_aspect_;
    float x_step = num_cols > 1 ? 2.0f/(num_cols) : 0.0f;
    float y_step = num_rows > 1 ? 2.0f/(layout_data.layout_aspect_*num_rows) : 0.0f;

    for (int i = 0; i < num_images; ++i) {

        const auto & im = image_set->onscreen_image(i);
        float xs = 1.0f;
        if (im) {
            if (im->image_aspect() < hero_aspect) {
                xs = im->image_aspect()/hero_aspect;
            }
        }
        int row = i / num_cols;
        int col = i % num_cols;
        Imath::M44f & m = layout_data.image_transforms_[i];
        m.makeIdentity();
        m.translate(Imath::V3f(x_off+col*x_step, y_off+row*y_step, 0.0f));
        m.scale(Imath::V3f(scale*xs, scale*xs, 1.0f));

    }

    // we draw all images. It doesn't matter the order that the are drawn
    // in so we will just respect their order in the ImageSet.
    layout_data.image_draw_order_hint_.resize(num_images);
    for (int i = 0; i < num_images; ++i) {
        layout_data.image_draw_order_hint_[i] = i;
    }

}

ViewportRenderer * GridViewportLayout::make_renderer(const std::string &window_id) {
    return new opengl::OpenGLViewportRenderer(window_id);
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<GridViewportLayout>>(
                GridViewportLayout::PLUGIN_UUID,
                "GridViewportLayout",
                plugin_manager::PluginFlags::PF_VIEWPORT_RENDERER,
                true,
                "Ted Waine",
                "Grid Viewport Layout Plugin")}));
}
}
