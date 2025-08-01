// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/playlist_selection_ui.hpp"
#include "xstudio/ui/qml/timeline_ui.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

TimelineUI::TimelineUI(const utility::Uuid cuuid, QObject *parent)
    : QMLActor(parent),
      cuuid_(std::move(cuuid)),
      backend_(),
      backend_events_(),
      name_("unknown"),
      flag_("#00000000") {

    auto *p = dynamic_cast<PlaylistUI *>(parent);
    if (p) {
        QObject::connect(p, SIGNAL(nameChanged()), this, SIGNAL(nameChanged()));
    }
    emit parent_playlistChanged();
    emit mediaModelChanged();
}

void TimelineUI::set_backend(caf::actor backend) {

    scoped_actor sys{system()};
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

    backend_ = backend;

    if (backend_) {

        try {
            auto detail     = request_receive<ContainerDetail>(*sys, backend_, detail_atom_v);
            name_           = detail.name_;
            uuid_           = detail.uuid_;
            backend_events_ = detail.group_;
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }

        update_media();
        emit nameChanged();
        emit uuidChanged();
    }
    emit backendChanged();
    spdlog::debug("TimelineUI set_backend {}", to_string(uuid_));
}

void TimelineUI::setName(const QString &name) {
    std::string _name = StdFromQString(name);
    if (_name != name_) {
        if (backend_) {
            scoped_actor sys{system()};
            sys->anon_send(backend_, utility::name_atom_v, _name);
        }
    }
}

void TimelineUI::update_media() {
    scoped_actor sys{system()};

    sys->request(backend_, infinite, playlist::get_media_atom_v, true)
        .receive(
            [&](const std::vector<UuidActor> &actors) {
                QList<QObject *> old_media = media_;

                for (auto i : actors) {
                    if (not uuid_media_.count(i.uuid())) {
                        uuid_media_[i.uuid()] = new MediaUI(this);
                        uuid_media_[i.uuid()]->initSystem(this);
                        uuid_media_[i.uuid()]->set_backend(i.actor());
                    }
                }
                bool done = false;
                while (not done) {
                    done = true;
                    for (const auto &i : uuid_media_) {
                        bool found = false;
                        for (const auto &ii : actors) {
                            if (i.first == ii.uuid()) {
                                found = true;
                                break;
                            }
                        }
                        if (not found) {
                            delete i.second;
                            uuid_media_.erase(i.first);
                            done = false;
                            break;
                        }
                    }
                }

                // rebuild list.
                media_.clear();
                media_order_.clear();
                auto ind = 0;
                for (auto i : actors) {
                    media_.append(uuid_media_[i.uuid()]);
                    media_order_[QUuidFromUuid(i.uuid()).toString()] = QVariant::fromValue(ind);
                    ind++;
                }

                if (old_media != media_) {
                    // media_model_.populate(media_);
                    emit mediaOrderChanged();
                    emit mediaListChanged();
                }
            },
            [=](const caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}


void TimelineUI::init(actor_system &system_) {
    QMLActor::init(system_);
    emit systemInit(this);

    spdlog::debug("TimelineUI init");

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {},

            [=](utility::event_atom, timeline::item_atom, const timeline::Item &) {},
            [=](utility::event_atom, timeline::item_atom, const JsonStore &, const bool) {},

            [=](utility::event_atom, utility::change_atom) { update_media(); },

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                if (name_ != name) {
                    name_ = name;
                    emit nameChanged();
                }
            }};
    });
}

QObject *TimelineUI::selectionFilter() { return static_cast<QObject *>(playlist_selection_); }

QString TimelineUI::fullName() {
    if (auto *p = dynamic_cast<PlaylistUI *>(parent())) {
        return p->name() + QString(" - ") + name();
    } else {
        return name();
    }
}