// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>

#include <chrono>

#include "xstudio/ui/viewport/viewport_layout_plugin.hpp"
#include "xstudio/ui/viewport/viewport_renderer_base.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::viewport;

ViewportLayoutPlugin::ViewportLayoutPlugin(
    caf::actor_config &cfg,
    const utility::JsonStore &init_settings) : 
    plugin::StandardPlugin(cfg, init_settings.value("name", "ViewportLayoutPlugin"), init_settings), is_python_plugin_(init_settings.value("is_python", false)) 
{
    init();
}

void ViewportLayoutPlugin::init() {
    
    handler_ = {
        [=](utility::get_event_group_atom) -> caf::actor {
            if (!event_group_) {
                event_group_ = spawn<broadcast::BroadcastActor>(this);
                link_to(event_group_);
            }
            return event_group_;
        },
        [=](viewport_renderer_atom, const std::string window_id) -> ViewportRendererPtr {
            return ViewportRendererPtr(make_renderer(window_id));
        },
        [=](viewport_layout_atom,
            const std::string &layout_mode,
            const JsonStore &python_plugin_layout) {        

            // this comes in from Python - we can't get exchange size_t between
            // C++ and python, PyBind and caf get their knickers in a twist trying
            // to infer the integer type, so the hash is stringified at both
            // ends.
            try {
                
                size_t hash;
                auto h = python_plugin_layout["hash"].get<std::string>();
                sscanf(h.c_str(), "%zu", &hash);
                delegate(caf::actor_cast<caf::actor>(this), viewport_layout_atom_v, layout_mode, python_plugin_layout, hash);

            } catch ( std::exception & e ) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
            
        },
        [=](viewport_layout_atom,
            const std::string &layout_mode,
            const JsonStore &python_plugin_layout,
            size_t hash) {        

            media_reader::ImageSetLayoutDataPtr layout_data = python_layout_data_to_ours(python_plugin_layout);
            if (layout_data) {
                layouts_cache_[layout_mode][hash] = layout_data;
                auto p = pending_responses_.find(hash);
                if (p != pending_responses_.end()) {
                    for(auto &rp: p->second) {
                        rp.deliver(layout_data);
                    }
                    pending_responses_.erase(p);
                }
            }
            
        },
        [=](viewport_layout_atom,
            const std::string &layout_mode,
            const media_reader::ImageBufDisplaySetPtr &image_set) -> result<media_reader::ImageSetLayoutDataPtr> {
            auto rp = make_response_promise<media_reader::ImageSetLayoutDataPtr>();
            auto p = layouts_cache_[layout_mode].find(image_set->images_layout_hash());
            if (p == layouts_cache_[layout_mode].end()) {
                __do_layout(layout_mode, image_set, rp);
            } else {
                rp.deliver(p->second);
            }
            return rp;
        },
        [=](viewport_layout_atom, const std::string &layout_name, const xstudio::playhead::AssemblyMode mode, const xstudio::playhead::AutoAlignMode auto_align) {
            // used by Python ViewportLayoutPlugin api
            add_layout_mode(layout_name, mode, auto_align);
        },
        };

    make_behavior();

    settings_toggle_ = add_boolean_attribute(module::Module::name(), module::Module::name(), true);
    settings_toggle_->expose_in_ui_attrs_group("layout_plugins");

    // this tells the pop-up menu for Compare/Layouts that this plugin doesn't
    // have any settings, so the 'Settings' button is disabled
    settings_toggle_->set_role_data(module::Attribute::DisabledValue, true);

    connect_to_ui();
                
    layouts_manager_ =
                    system().registry().template get<caf::actor>(viewport_layouts_manager);
    gobal_playhead_events_ = system().registry().template get<caf::actor>(global_playhead_events_actor);
    anon_send(layouts_manager_, ui::viewport::viewport_layout_atom_v, caf::actor_cast<caf::actor>(this), Module::name());

}

void ViewportLayoutPlugin::on_exit() {
    layouts_manager_ = caf::actor();
    gobal_playhead_events_ = caf::actor();
    plugin::StandardPlugin::on_exit();
}

void ViewportLayoutPlugin::do_layout(
    const std::string &layout_mode,
    const media_reader::ImageBufDisplaySetPtr &image_set,
    media_reader::ImageSetLayoutData &layout_data
    )
{
    // For the default layout, the image(s) are not transformed in any way.
    // We just need to set the aspect of the layout, which is the same as 
    // the image aspect for the hero image
    const media_reader::ImageBufPtr &hero_image = image_set->onscreen_image(image_set->hero_sub_playhead_index());
    if (hero_image) {
        layout_data.layout_aspect_ = hero_image->pixel_aspect()*hero_image->image_size_in_pixels().x/hero_image->image_size_in_pixels().y;
    } else {
        layout_data.layout_aspect_ = 16.0/9.0f;
    }

    // this fills image_transform_matrices with unity matrices
    layout_data.image_transforms_.resize(image_set->num_onscreen_images());

    // we only draw the 'hero' image. No compositing or any other layout stuff
    // to do.
    layout_data.image_draw_order_hint_ = std::vector<int>(1, image_set->hero_sub_playhead_index());

}

void ViewportLayoutPlugin::__do_layout(
    const std::string &layout_mode,
    const media_reader::ImageBufDisplaySetPtr &image_set,
    caf::typed_response_promise<media_reader::ImageSetLayoutDataPtr> rp
    ) 
{

    // if we have an event_group_ actor, this means there is a Python
    // plugin for doing viewport layouts. We send a message to the event_group_
    // which calls the do_layout' function of
    if (is_python_plugin_ && event_group_) {
        // We need python side to receive the hash for the image layout inputs (image sizes and pixel aspects)
        // but the hash is size_t which we can't interchange directly with python as it handles integers
        // differently, so we stringify the hash. Python plugin then sends back the layout data
        // with the hash as a string which we need to decode to size_t again.
        send(
            event_group_,
            viewport_layout_atom_v,
            layout_mode,
            image_set->as_json(),
            fmt::format("{}",
            image_set->images_layout_hash())
            );
        pending_responses_[image_set->images_layout_hash()].push_back(rp);
        return;
    }

    auto layout_data = new media_reader::ImageSetLayoutData;

    // this will callback to the plugins' implementation (or our default 
    // implementation) above.
    do_layout(
        layout_mode,
        image_set,
        *layout_data
        );

    auto r = media_reader::ImageSetLayoutDataPtr(layout_data);
    rp.deliver(r);
    layouts_cache_[layout_mode][image_set->images_layout_hash()] = r;

}

/*void HUDPluginBase::hud_element_qml(
    const std::string qml_code, const HUDElementPosition position) {

    hud_data_->set_role_data(module::Attribute::QmlCode, qml_code);

    if (position == FullScreen) {
        hud_data_->expose_in_ui_attrs_group("hud_elements_fullscreen");
        return;
    } else if (!hud_item_position_) {
        // add settings attribute for the position of the HUD element
        hud_item_position_ = add_string_choice_attribute(
            "Position On Screen",
            "Position On Screen",
            position_names_[position],
            utility::map_value_to_vec(position_names_));
        add_hud_settings_attribute(hud_item_position_);
        hud_item_position_->set_preference_path(
            fmt::format("/plugin/{}/hud_item_position", plugin_underscore_name_));
    }

    if (hud_item_position_) {
        attribute_changed(hud_item_position_->uuid(), module::Attribute::Value);
    }
}*/

media_reader::ImageSetLayoutDataPtr ViewportLayoutPlugin::python_layout_data_to_ours(
    const utility::JsonStore &python_data
) const {
    auto result = new media_reader::ImageSetLayoutData;

    try {



        if (python_data.contains("layout_aspect_ratio")) {
            result->layout_aspect_ = python_data["layout_aspect_ratio"].get<float>();
        }

        if (python_data.contains("image_draw_order")) {
            if (python_data["image_draw_order"].is_array()) {
                for (const auto &v: python_data["image_draw_order"]) {
                    result->image_draw_order_hint_.push_back(v.get<int>());
                }
            } else {
                throw std::runtime_error("image_draw_order entry must be an array");
            }
        } else {
            throw std::runtime_error("expected image_draw_order entry");
        }

        if (python_data.contains("transforms")) {
            if (python_data["transforms"].is_array()) {
                for (const auto &v: python_data["transforms"]) {
                    if (v.is_array() && v.size() == 3) {
                        float tx = v[0].get<float>();
                        float ty = v[1].get<float>();
                        float s = v[2].get<float>();
                        Imath::M44f m;
                        m.translate(Imath::V3f(tx, ty, 0.0f));
                        m.scale(Imath::V3f(s, s, 1.0f));
                        result->image_transforms_.push_back(m);
                    } else {
                    throw std::runtime_error("Elements of transforms entry must be an array of 3 floats.");
                    }
                }
            } else {
                throw std::runtime_error("transforms entry must be an array");
            }
        } else {
            throw std::runtime_error("expected transforms entry");
        }

    } catch (std::exception & e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return media_reader::ImageSetLayoutDataPtr(result);
}

void ViewportLayoutPlugin::add_layout_settings_attribute(module::Attribute *attr, const std::string &layout_name) {
    attr->expose_in_ui_attrs_group(layout_name + " Settings");
    // this means the 'settings' button WILL be visible!
    settings_toggle_->set_role_data(module::Attribute::DisabledValue, false);
}

void ViewportLayoutPlugin::add_viewport_layout_qml_overlay(
        const std::string &layout_name,
        const std::string &qml_code) {

    auto attr = add_boolean_attribute(layout_name, layout_name, true);
    attr->set_role_data(module::Attribute::QmlCode, qml_code);
    attr->expose_in_ui_attrs_group("viewport_overlay_plugins");

}


void ViewportLayoutPlugin::add_layout_mode(
          const std::string &name, 
          const playhead::AssemblyMode mode,
          const playhead::AutoAlignMode default_auto_align
          ) 
{

    request(
        layouts_manager_,
        infinite,
        viewport_layout_atom_v,
        caf::actor_cast<caf::actor>(this),
        name,
        mode,
        default_auto_align).then(
            [=](bool accepted) {
                if (accepted) {
                    auto layout_toggle = add_string_attribute(name, name, "");
                    layout_toggle->expose_in_ui_attrs_group("viewport_layouts");
                    layout_names_[layout_toggle->uuid()] = layout_toggle;
                }
            },
            [=](caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void ViewportLayoutPlugin::attribute_changed(
    const utility::Uuid &attribute_uuid, const int role
    ) 
{
    // this forces re-computation of the layout geometry
    layouts_cache_.clear();
    // now force viewports to redraw
    anon_send(gobal_playhead_events_, playhead::redraw_viewport_atom_v);
    StandardPlugin::attribute_changed(attribute_uuid, role);
}

ViewportLayoutManager::ViewportLayoutManager(caf::actor_config &cfg) : caf::event_based_actor(cfg)
{
    spdlog::debug("Created ViewportLayoutManager");
    print_on_exit(this, "ViewportLayoutManager");

    system().registry().put(viewport_layouts_manager, this);
    event_group_             = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);

    behavior_ = {

        [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](const group_down_msg & /*msg*/) {},
        [=](utility::get_event_group_atom) -> caf::actor { return event_group_; },
        [=](broadcast::join_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::join_broadcast_atom_v, joiner);
        },
        [=](broadcast::leave_broadcast_atom, caf::actor joiner) {
            delegate(event_group_, broadcast::leave_broadcast_atom_v, joiner);
        },
        [=](ui::viewport::viewport_layout_atom,
            caf::actor layout_actor,
            const std::string &name) {
            // this message is sent by instances of ViewportLayoutPlugin
            // on construction
            viewport_layout_plugins_[name] = layout_actor;
            link_to(layout_actor);
        },
        [=](ui::viewport::viewport_layout_atom,
            caf::actor layout_actor,
            const std::string &layout_name,
            const playhead::AssemblyMode mode,
            const playhead::AutoAlignMode default_align_mode) -> result<bool> {
            // Here a layout actor is registering a layout with us
            if (viewport_layouts_.contains(layout_name)) {
                return make_error(
                    xstudio_error::error,
                    fmt::format(
                        "A viewport layout name \"{}\" is already registered.", layout_name));
            }
            viewport_layouts_[layout_name] = std::make_pair(layout_actor,std::make_pair(default_align_mode, mode));
            return true;
        },
        [=](viewport_layout_atom,
            const std::string &layout_name,
            const bool /*shared_instance*/,
            const std::string /*viewport_name*/) -> result<caf::actor> {
            if (not viewport_layouts_.contains(layout_name)) {
                return make_error(
                    xstudio_error::error,
                    fmt::format(
                        "Requested viewport layout named \"{}\" which is not registered.",
                        layout_name));
            }
            return viewport_layouts_[layout_name].first;
        },
        [=](playhead::compare_mode_atom,
            const std::string &layout_name) -> result<std::pair<xstudio::playhead::AutoAlignMode, xstudio::playhead::AssemblyMode>> {
            if (not viewport_layouts_.contains(layout_name)) {
                return make_error(
                    xstudio_error::error,
                    fmt::format(
                        "Requested viewport layout named \"{}\" which is not registered.",
                        layout_name));
            }
            return viewport_layouts_[layout_name].second;
        }};

    spawn_plugins();

}

ViewportLayoutManager::~ViewportLayoutManager() {
}

void ViewportLayoutManager::on_exit() {
    viewport_layouts_.clear();
    caf::event_based_actor::on_exit();
}

void ViewportLayoutManager::spawn_plugins() {

    // get the plugin manager
    auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
    
    const auto ptype = plugin_manager::PluginType(
        plugin_manager::PluginFlags::PF_VIEWPORT_RENDERER
        );

    // SPAWN C++ PLUGINS (Python plugins are loaded in python startup script)

    // get details of viewport layout plugins
    request(
        pm,
        infinite, utility::detail_atom_v,
        ptype).then(

        [=](const std::vector<plugin_manager::PluginDetail> &renderer_plugin_details) {

            // loop over plugin details
            for (const auto &pd : renderer_plugin_details) {

                // instance the plugin. Each plugin automatically registeres 
                // itself with this class on construction (see above)
                utility::JsonStore j;
                j["name"] = pd.name_;
                j["is_python_plugin"] = false;
                request(
                    pm,
                    infinite,
                    plugin_manager::spawn_plugin_atom_v,
                    pd.uuid_,
                    j
                    ).then(
                        [=](caf::actor) {
                        },
                        [=](caf::error &err) {
                             spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });

            }
        },
        [=](caf::error &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
        });
                

}