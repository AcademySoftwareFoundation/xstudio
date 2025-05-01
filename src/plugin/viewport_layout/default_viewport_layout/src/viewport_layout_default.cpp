// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "viewport_layout_default.hpp"
#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio;

DefaultViewportLayout::DefaultViewportLayout(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : ViewportLayoutPlugin(cfg, init_settings) {

    // Note: the base class takes care of making the actual layout data. The
    // default layout is to just display the 'hero' image in the ImageDisplaySet
    // with no transform applied.

    // String mode will trigger the playhead's logic for AM_STRING AssemblyMode
    // where it takes N input sources and 'strings' them togoether so that they
    // play in sequence.

    // this is awkward - to avoid circular build dependency between openglviewport and viewport
    // I have to use this DefaultViewportLayout for backing python plugins that
    // are a ViewportLayoutPlugin. We don't want to add these layout modes if
    // this is an instance that is backing a python plugin.
    if (!init_settings.value("is_python", false)) {
        add_layout_mode("Off", 102.0, playhead::AssemblyMode::AM_ONE);
        add_layout_mode(
            "A/B",
            101.0,
            playhead::AssemblyMode::AM_TEN,
            playhead::AutoAlignMode::AAM_ALIGN_FRAMES);
        add_layout_mode("String", 100.0, playhead::AssemblyMode::AM_STRING);
    }
}

ViewportRenderer *DefaultViewportLayout::make_renderer(
    const std::string &window_id, const utility::JsonStore &prefs) {
    return new opengl::OpenGLViewportRenderer(window_id, prefs);
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<DefaultViewportLayout>>(
                DefaultViewportLayout::PLUGIN_UUID,
                "DefaultViewportLayout",
                plugin_manager::PluginFlags::PF_VIEWPORT_RENDERER,
                true,
                "Ted Waine",
                "Basic Viewport Layout Plugin")}));
}
}
