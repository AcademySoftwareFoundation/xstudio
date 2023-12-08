// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include "xstudio/atoms.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/module/global_module_events_actor.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"

#include <iostream>

using namespace xstudio::utility;
using namespace xstudio::module;
using namespace xstudio;

GlobalModuleAttrEventsActor::GlobalModuleAttrEventsActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    print_on_create(this, "GlobalModuleAttrEventsActor");
    print_on_exit(this, "GlobalModuleAttrEventsActor");

    system().registry().put(module_events_registry, this);
    module_backend_events_group_ = spawn<broadcast::BroadcastActor>(this);
    module_ui_events_group_      = spawn<broadcast::BroadcastActor>(this);
    playhead_group_              = spawn<broadcast::BroadcastActor>(this);

    link_to(module_backend_events_group_);
    link_to(module_ui_events_group_);
    link_to(playhead_group_);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
        [=](module_ui_events_group_atom) -> caf::actor { return module_ui_events_group_; },
        [=](utility::get_group_atom) -> caf::actor { return module_backend_events_group_; },
        [=](redraw_viewport_group_atom) -> caf::actor { return playhead_group_; },
        [=](full_attributes_description_atom,
            const AttributeSet &attrs,
            const utility::Uuid &requester_uuid) {
            // An attribute owner in the backend is sending a full description of it's module
            send(
                module_backend_events_group_,
                full_attributes_description_atom_v,
                attrs,
                requester_uuid);
        },

        [=](full_attributes_description_atom, const AttributeSet &attrs) {
            // An attribute owner in the backend is sending a full description of it's module
            send(module_backend_events_group_, full_attributes_description_atom_v, attrs);
        },

        [=](join_module_attr_events_atom, caf::actor events_group) {
            join_broadcast(this, events_group);
        },

        [=](leave_module_attr_events_atom, caf::actor events_group) {
            leave_broadcast(this, events_group);
        },

        [=](remove_attrs_from_ui_atom, const std::vector<utility::Uuid> &attr_uuids) {
            send(module_backend_events_group_, remove_attrs_from_ui_atom_v, attr_uuids);
        },

        [=](request_full_attributes_description_atom,
            const std::vector<std::string> &attrs_group_names,
            const utility::Uuid &requester_uuid) {
            // N.B. some actors that are also a Module are Qt mixin actors ... such actors
            // cannot have message handlers that return a response. So instead, to request the
            // description of the Attributes that actors own we do it passively like this,
            // sending a message asking for the attrs and they should send a message back which
            // is handled in the 'attributes_atom' msm handler here
            send(
                module_ui_events_group_,
                request_full_attributes_description_atom_v,
                attrs_group_names,
                requester_uuid);
        },

        [=](request_menu_attributes_description_atom,
            const std::string &root_menu_name,
            const utility::Uuid &requester_uuid) {
            // see comment immediately above
            send(
                module_ui_events_group_,
                request_menu_attributes_description_atom_v,
                root_menu_name,
                requester_uuid);
        },

        [=](change_attribute_request_atom,
            const utility::Uuid &attr_uuid,
            const int role,
            const utility::JsonStore &value) {
            send(
                module_ui_events_group_,
                change_attribute_request_atom_v,
                attr_uuid,
                role,
                value);
        },
        [=](attribute_deleted_event_atom, const utility::Uuid &attr_uuid) {
            // An attribute has been deleted at the backend, notify attribute watchers
            send(module_backend_events_group_, attribute_deleted_event_atom_v, attr_uuid);
        },

        [=](playhead::redraw_viewport_atom) {
            send(playhead_group_, playhead::redraw_viewport_atom_v);
        },
        [=](change_attribute_event_atom,
            const utility::Uuid &module_uuid,
            const utility::Uuid &attr_uuid,
            const int role,
            const utility::JsonStore &role_value,
            const bool redraw_viewport) {
            // This is notification that an attribute has actually changed - we pass it on to
            // attribute watchers (like UI componenets) so that they can update to reflect the
            // change
            send(
                module_backend_events_group_,
                change_attribute_event_atom_v,
                module_uuid,
                attr_uuid,
                role,
                role_value);

            // Also a special playheads group will be sent a redraw message if required.
            if (redraw_viewport) {
                send(playhead_group_, playhead::redraw_viewport_atom_v);
            }
        });
}

void GlobalModuleAttrEventsActor::on_exit() {
    system().registry().erase(module_events_registry);
}
