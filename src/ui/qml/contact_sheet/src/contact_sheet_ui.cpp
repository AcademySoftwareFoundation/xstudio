// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/ui/qml/playlist_selection_ui.hpp"
#include "xstudio/ui/qml/contact_sheet_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

ContactSheetUI::ContactSheetUI(const utility::Uuid cuuid, QObject *parent)
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

void ContactSheetUI::set_backend(caf::actor backend) {
    scoped_actor sys{system()};

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

        auto selection_actor =
            request_receive<caf::actor>(*sys, backend_, playlist::selection_actor_atom_v);
        playlist_selection_ = new PlaylistSelectionUI(this);
        playlist_selection_->initSystem(this);
        playlist_selection_->set_backend(selection_actor);
        emit playlistSelectionThingChanged();

        update_media();
        emit nameChanged();
        emit uuidChanged();
        emit compareModeChanged();
    }
    emit backendChanged();
    spdlog::debug("ContactSheetUI set_backend {}", to_string(uuid_));
}

void ContactSheetUI::setName(const QString &name) {
    std::string _name = StdFromQString(name);
    if (_name != name_) {
        if (backend_) {
            scoped_actor sys{system()};
            sys->anon_send(backend_, utility::name_atom_v, _name);
        }
    }
}

QFuture<QList<QUuid>> ContactSheetUI::loadMediaFuture(const QUrl &path) {
    // expect URI's
    return QtConcurrent::run([=]() {
        QList<QUuid> result;
        scoped_actor sys{system()};
        try {
            auto actor = request_receive<caf::actor>(*sys, backend_, playhead::source_atom_v);

            auto items = utility::scan_posix_path(StdFromQString(path.path()));
            std::sort(
                std::begin(items),
                std::end(items),
                [](std::pair<caf::uri, FrameList> a, std::pair<caf::uri, FrameList> b) {
                    return a.first < b.first;
                });

            for (const auto &i : items) {
                try {
                    if (is_file_supported(i.first)) {
                        auto ua = request_receive<UuidActor>(
                            *sys,
                            actor,
                            playlist::add_media_atom_v,
                            "New media",
                            i.first,
                            i.second);
                        result.push_back(QUuidFromUuid(ua.uuid()));
                        anon_send(backend_, playlist::add_media_atom_v, ua.uuid(), Uuid());
                    } else {
                        spdlog::warn("Unsupported file type {}.", to_string(i.first));
                    }
                } catch (const std::exception &e) {
                    spdlog::error("Failed to create media {}", e.what());
                }
            }

        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        return result;
    });
}


// needs to add to playlist first..
QFuture<QList<QUuid>> ContactSheetUI::handleDropFuture(const QVariantMap &drop) {
    // handle drag drop..
    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;

        auto jsn = dropToJsonStore(drop);

        auto playlist = request_receive<caf::actor>(*sys, backend_, playhead::source_atom_v);


        // conver to json..
        if (jsn.count("text/uri-list")) {
            for (const auto &path : jsn["text/uri-list"]) {
                auto uri = caf::make_uri(path);
                if (uri) {
                    auto new_media = request_receive<UuidActorVector>(
                        *sys, playlist, playlist::add_media_atom_v, *uri, true);
                    for (const auto &i : new_media) {
                        anon_send(backend_, playlist::add_media_atom_v, i.uuid(), Uuid());
                        results.push_back(QUuidFromUuid(i.uuid()));
                    }
                }
            }
        } else {
            // forward to datasources for non file paths
            auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
            try {
                auto result = request_receive<UuidActorVector>(
                    *sys, pm, data_source::use_data_atom_v, JsonStore(jsn), true);
                if (not result.empty()) {
                    // we've got a collection of actors..
                    // lets assume they are media... (WARNING this may not be the case...)
                    // create new playlist and add them...
                    for (const auto &i : result) {
                        anon_send(playlist, playlist::add_media_atom_v, i, utility::Uuid());
                        anon_send(backend_, playlist::add_media_atom_v, i.uuid(), Uuid());
                        results.push_back(QUuidFromUuid(i.uuid()));
                    }
                } else {
                    // try file load..
                    for (const auto &i : jsn["text/plain"]) {
                        auto uri = caf::make_uri("file:" + i.get<std::string>());
                        if (uri) {
                            auto new_media = request_receive<UuidActorVector>(
                                *sys, playlist, playlist::add_media_atom_v, *uri, true);


                            for (const auto &j : new_media) {
                                anon_send(
                                    backend_, playlist::add_media_atom_v, j.uuid(), Uuid());
                                results.push_back(QUuidFromUuid(j.uuid()));
                            }
                        }
                    }
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        return results;
    });
}


// bool ContactSheetUI::addMedia(const QUuid &uuid, const QUuid &before_uuid) {
//     bool result = false;
//     scoped_actor sys{system()};

//     try {
//         result = request_receive<bool>(
//             *sys,
//             backend_,
//             playlist::add_media_atom_v,
//             UuidFromQUuid(uuid),
//             UuidFromQUuid(before_uuid));
//     } catch (const std::exception &e) {
//         spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
//     }

//     return result;
// }

void ContactSheetUI::update_media() {
    scoped_actor sys{system()};

    sys->request(backend_, infinite, playlist::get_media_atom_v)
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
                    media_model_.populate(media_);
                    emit mediaOrderChanged();
                    emit mediaListChanged();
                }
            },
            [=](const caf::error &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}


void ContactSheetUI::init(actor_system &system_) {
    QMLActor::init(system_);
    emit systemInit(this);

    spdlog::debug("ContactSheetUI init");

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {},

            [=](utility::event_atom, playlist::add_media_atom, const UuidActor &ua) {
                emit mediaAdded(QUuidFromUuid(ua.uuid()));
            },

            [=](utility::event_atom,
                playlist::media_content_changed_atom,
                const std::vector<UuidActor> &) {},

            [=](utility::event_atom, utility::change_atom) { update_media(); },

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                if (name_ != name) {
                    name_ = name;
                    emit nameChanged();
                }
            }};
    });
}

void ContactSheetUI::dragDropReorder(
    const QVariantList dropped_uuids, const QString before_uuid) {
    try {

        utility::UuidList drop_uuids;
        for (auto v : dropped_uuids) {
            drop_uuids.push_back(UuidFromQUuid(v.toUuid()));
        }
        utility::Uuid before = before_uuid == "" ? Uuid() : Uuid(StdFromQString(before_uuid));
        anon_send(backend_, playlist::move_media_atom_v, drop_uuids, before);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

void ContactSheetUI::sortAlphabetically() {
    anon_send(backend_, playlist::sort_alphabetically_atom_v);
}

QObject *ContactSheetUI::selectionFilter() {
    return static_cast<QObject *>(playlist_selection_);
}

QString ContactSheetUI::fullName() {
    if (auto *p = dynamic_cast<PlaylistUI *>(parent())) {
        return p->name() + QString(" - ") + name();
    } else {
        return name();
    }
}

// find item based off container uuid or actor uuid
MediaUI *ContactSheetUI::getNextItem(const utility::Uuid &uuid) {
    // might be media uuid ?
    if (uuid_media_.count(uuid)) {
        // find nextmedia
        MediaUI *mojb = uuid_media_[uuid];
        int ind       = 0;
        for (auto &i : media_) {
            if (i == mojb) {
                ind++;
                if (media_.size() > ind) {
                    return dynamic_cast<MediaUI *>(media_[ind]);
                }
                break;
            }
            ind++;
        }
    }

    return nullptr;
}

QUuid ContactSheetUI::getNextItemUuid(const QUuid &quuid) {
    QUuid result;

    auto obj = getNextItem(UuidFromQUuid(quuid));
    if (obj)
        result = obj->uuid();

    return result;
}

bool ContactSheetUI::contains_media(const QUuid &key) const {
    return uuid_media_.count(UuidFromQUuid(key));
}
