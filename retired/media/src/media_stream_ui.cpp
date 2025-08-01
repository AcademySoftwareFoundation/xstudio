// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

MediaStreamUI::MediaStreamUI(QObject *parent)
    : QMLActor(parent), backend_(), backend_events_() {}

void MediaStreamUI::set_backend(caf::actor backend) {
    spdlog::debug("MediaStreamUI set_backend");
    scoped_actor sys{system()};

    backend_ = backend;
    // get backend state..
    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        backend_events_ = caf::actor();
    }


    try {
        auto detail     = request_receive<ContainerDetail>(*sys, backend_, detail_atom_v);
        name_           = QStringFromStd(detail.name_);
        uuid_           = detail.uuid_;
        backend_events_ = detail.group_;
        request_receive<bool>(
            *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    update_on_change();

    emit backendChanged();
    emit nameChanged();
    emit uuidChanged();
    spdlog::debug("MediaStreamUI set_backend {}", to_string(uuid_));
}

void MediaStreamUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void MediaStreamUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("MediaStreamUI init");

    // self()->set_down_handler([=](down_msg& msg) {
    // 	if(msg.source == store)
    // 		unsubscribe();
    // });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](backend_atom, caf::actor actor) { set_backend(actor); },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                // 		if(msg.source == store_events)
                // unsubscribe();
            },

            [=](utility::event_atom, utility::change_atom) { update_on_change(); },

            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                QString tmp = QStringFromStd(name);
                if (tmp != name_) {
                    name_ = tmp;
                    emit nameChanged();
                }
            }};
    });
}

void MediaStreamUI::update_on_change() {}
