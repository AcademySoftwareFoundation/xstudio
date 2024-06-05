// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/contact_sheet_ui.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/timeline_ui.hpp"
#include "xstudio/ui/qml/playlist_selection_ui.hpp"
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/ui/qml/session_ui.hpp"
#include "xstudio/ui/qml/subset_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/global_store/global_store.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::global_store;

PlaylistUI::PlaylistUI(const utility::Uuid cuuid, const utility::Uuid uuid, QObject *parent)
    : QMLActor(parent),
      cuuid_(std::move(cuuid)),
      uuid_(std::move(uuid)),
      backend_(),
      backend_events_(),
      name_("unknown"),
      flag_("#00000000") {
    emit mediaModelChanged();
}

void PlaylistUI::setName(const QString &name) {
    std::string _name = StdFromQString(name);
    if (_name != name_) {
        if (backend_) {
            scoped_actor sys{system()};
            sys->anon_send(backend_, utility::name_atom_v, _name);
        }
    }
}

void PlaylistUI::setSelected(const bool value, const bool recurse) {

    if (selected_ != value) {
        selected_ = value;
        emit selectedChanged();
    }
    if (recurse) {
        for (auto i : items_) {
            setSelection(i, false);
        }
    }
}


void PlaylistUI::set_backend(caf::actor backend, const bool partial) {

    backend_          = caf::actor();
    selected_subitem_ = nullptr;
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

    if (partial) {
        backend_ = backend;
    } else {

        try {
            auto detail     = request_receive<ContainerDetail>(*sys, backend, detail_atom_v);
            name_           = detail.name_;
            uuid_           = detail.uuid_;
            backend_events_ = detail.group_;
            backend_        = backend;

            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());

            emit backendChanged();
            emit nameChanged();
            emit uuidChanged();
            emit selectedSubitemChanged();
            update_on_change(false);

            auto selection_actor =
                request_receive<caf::actor>(*sys, backend_, playlist::selection_actor_atom_v);
            playlist_selection_ = new PlaylistSelectionUI(this);
            playlist_selection_->initSystem(this);
            playlist_selection_->set_backend(selection_actor);
            emit playlistSelectionThingChanged();

            // this will force the backend playlist to re-broadcast it's list of media
            // back to us (via the event_group)
            anon_send(backend_, playlist::media_content_changed_atom_v);

        } catch (const std::exception &e) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void PlaylistUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}


QFuture<bool> PlaylistUI::setJSONFuture(const QString &json, const QString &path) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto result = request_receive<bool>(
                    *sys,
                    backend_,
                    json_store::set_json_atom_v,
                    JsonStore(nlohmann::json::parse(StdFromQString(json))),
                    StdFromQString(path));

                return result;
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return false;
            }
        }
        return false;
    });
}

QFuture<QString> PlaylistUI::getJSONFuture(const QString &path) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto result = request_receive<JsonStore>(
                    *sys, backend_, json_store::get_json_atom_v, StdFromQString(path));

                return QStringFromStd(result.dump());
            } catch (const std::exception &) {
                // spdlog::warn("{} {}", __PRETTY_FUNCTION__,  err.what());
                return QString(); // QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


bool PlaylistUI::setJSON(const QString &json, const QString &path) {
    return setJSONFuture(json, path).result();
}

QString PlaylistUI::getJSON(const QString &path) { return getJSONFuture(path).result(); }


QObject *PlaylistUI::selectionFilter() { return static_cast<QObject *>(playlist_selection_); }

// QUrl to Uri ?
// we need trun a URL into media objects.
// url maybe a dir..
// Turn URL into list..
// do we handle http:// ? Or just file://
QFuture<QList<QUuid>> PlaylistUI::loadMediaFuture(const QUrl &path) {
    return QtConcurrent::run([=]() {
        // Code in this block will run in another thread
        QList<QUuid> result;
        try {
            scoped_actor sys{system()};
            auto new_media = request_receive<UuidActorVector>(
                *sys, backend_, playlist::add_media_atom_v, UriFromQUrl(path), true);
            for (const auto &i : new_media)
                result.push_back(QUuidFromUuid(i.uuid()));
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        return result;
    });
}

QFuture<QList<QUuid>> PlaylistUI::handleDropFuture(const QVariantMap &drop) {
    // handle drag drop..
    return QtConcurrent::run([=]() {
        scoped_actor sys{system()};
        QList<QUuid> results;

        auto jsn = dropToJsonStore(drop);

        // conver to json..
        if (jsn.count("text/uri-list")) {
            for (const auto &path : jsn["text/uri-list"]) {
                auto uri = caf::make_uri(path);
                if (uri) {
                    auto new_media = request_receive<UuidActorVector>(
                        *sys, backend_, playlist::add_media_atom_v, *uri, true);
                    for (const auto &i : new_media)
                        results.push_back(QUuidFromUuid(i.uuid()));
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
                        anon_send(backend_, playlist::add_media_atom_v, i, utility::Uuid());
                        results.push_back(QUuidFromUuid(i.uuid()));
                    }
                } else {
                    // try file load..
                    for (const auto &i : jsn["text/plain"]) {
                        auto uri = caf::make_uri("file:" + i.get<std::string>());
                        if (uri) {
                            auto new_media = request_receive<UuidActorVector>(
                                *sys, backend_, playlist::add_media_atom_v, *uri, true);
                            for (const auto &j : new_media)
                                results.push_back(QUuidFromUuid(j.uuid()));
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


// QFuture<QUuid> PlaylistUI::addMediaFuture(const QUrl &path, const QString &name, const QUuid
// &) {
//     return QtConcurrent::run([=]() {
//         // Code in this block will run in another thread
//         QUuid result;
//         try {
//             scoped_actor sys{system()};
//             auto new_media = request_receive<UuidActor>(
//                 *sys,
//                 backend_,
//                 playlist::add_media_atom_v,
//                 StdFromQString(name),
//                 UriFromQUrl(path)
//             );
//             result = QUuidFromUuid(new_media.first);
//         } catch (const std::exception &e) {
//             spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
//         }
//         return result;
//     });
// }

void PlaylistUI::setSelection(const QVariantList &cuuids, const bool clear) {
    emit newSelection(cuuids, clear);

    std::set<QUuid> uuids;
    for (auto &i : cuuids) {
        uuids.insert(i.toUuid());
    }

    bool selected_subitem_changed = false;

    // clear unselected
    for (QMap<QUuid, QObject *>::iterator i = items_.begin(); i != items_.end(); i++) {
        if (not uuids.count(i.key()) and clear) {
            setSelection(i.value(), false);
            if (i.value() == selected_subitem_) {
                selected_subitem_        = nullptr;
                selected_subitem_changed = true;
            }
        }
    }

    // selected
    for (QMap<QUuid, QObject *>::iterator i = items_.begin(); i != items_.end(); i++) {
        if (uuids.count(i.key())) {
            setSelection(i.value(), true);
        }
    }

    if (selected_subitem_changed)
        emit selectedSubitemChanged();
}

void PlaylistUI::setSelection(QObject *obj, const bool selected) {
    if (auto item = dynamic_cast<PlaylistUI *>(obj)) {
        item->setSelected(selected);
        item->setSelection(QVariantList());
    } else if (auto item = dynamic_cast<TimelineUI *>(obj)) {
        item->setSelected(selected);
        if (!selected_subitem_ and selected) {
            selected_subitem_ = obj;
            emit selectedSubitemChanged();
        }
    } else if (auto item = dynamic_cast<SubsetUI *>(obj)) {
        item->setSelected(selected);
        if (not selected_subitem_ and selected) {
            selected_subitem_ = obj;
            emit selectedSubitemChanged();
        }
    } else if (auto item = dynamic_cast<ContactSheetUI *>(obj)) {
        item->setSelected(selected);
        if (!selected_subitem_ and selected) {
            selected_subitem_ = obj;
            emit selectedSubitemChanged();
        }
    } else if (auto item = dynamic_cast<ContainerDividerUI *>(obj)) {
        item->setSelected(selected);
    } else if (auto item = dynamic_cast<ContainerGroupUI *>(obj)) {
        item->setSelected(selected);
    }
}

void PlaylistUI::dragDropReorder(const QVariantList dropped_uuids, const QString before_uuid) {
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

void PlaylistUI::sortAlphabetically() {
    anon_send(backend_, playlist::sort_alphabetically_atom_v);
}


void PlaylistUI::update_on_change(const bool select_new_items) {

    updateItemModel(select_new_items);
    emit itemModelChanged();
}


void PlaylistUI::init(actor_system &system_) {
    QMLActor::init(system_);
    emit systemInit(this);

    spdlog::debug("PlaylistUI init");

    // self()->set_down_handler([=](down_msg& msg) {
    // 	if(msg.source == store)
    // 		unsubscribe();
    // });

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](backend_atom, caf::actor actor) {
                set_backend(actor);

                // If this PlaylistUI is the one showing in the viewer, tell the backend
                // so it can open readers on the media in the playlist ready for quick
                // viewing
                if (auto session = dynamic_cast<SessionUI *>(parent())) {
                    if (session->playlist() == static_cast<QObject *>(this)) {
                        anon_send(actor, playlist::set_playlist_in_viewer_atom_v, true);
                    }
                }
            },

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {
                // if (msg.source == backend_events_)
                //     self()->leave(backend_events_);
            },

            [=](utility::event_atom, playlist::add_media_atom, const UuidActor &ua) {
                spdlog::warn("media added, emit signal");
                emit mediaAdded(QUuidFromUuid(ua.uuid()));
            },

            [=](utility::event_atom, playlist::create_contact_sheet_atom, const UuidActor &) {
                setExpanded(true);
                updateItemModel();
                emit itemModelChanged();
                // emit newItem(QVariant::fromValue(dynamic_cast<const ContactSheetUI
                // *>(findItem(ua.first))->qcuuid()));
            },

            [=](utility::event_atom, playlist::create_divider_atom, const Uuid &uuid) {
                setExpanded(true);
                updateItemModel();
                emit itemModelChanged();
                emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
            },

            [=](utility::event_atom, playlist::create_group_atom, const Uuid &uuid) {
                setExpanded(true);
                updateItemModel();
                emit itemModelChanged();
                emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
            },

            [=](utility::event_atom, playlist::create_timeline_atom, const UuidActor &) {
                setExpanded(true);
                updateItemModel();
                emit itemModelChanged();
                // emit newItem(QVariant::fromValue(dynamic_cast<const TimelineUI
                // *>(findItem(ua.first))->qcuuid()));
            },

            [=](utility::event_atom, playlist::create_subset_atom, const UuidActor &) {
                setExpanded(true);
                updateItemModel();
                emit itemModelChanged();
                // emit newItem(QVariant::fromValue(dynamic_cast<const SubsetUI
                // *>(findItem(ua.first))->qcuuid()));
            },

            [=](utility::event_atom, playlist::loading_media_atom, const bool is_loading) {
                if (loadingMedia_ != is_loading) {
                    loadingMedia_ = is_loading;
                    emit loadingMediaChanged();
                }
            },

            [=](utility::event_atom,
                playlist::media_content_changed_atom,
                const std::vector<UuidActor> &actors) {
                const auto tt = utility::clock::now();
                for (auto i : actors) {
                    if (not uuid_media_.count(i.uuid())) {
                        // subscribe to status event..
                        uuid_media_[i.uuid()] = new MediaUI(this);
                        QQmlEngine::setObjectOwnership(
                            uuid_media_[i.uuid()], QQmlEngine::CppOwnership);
                        QObject::connect(
                            uuid_media_[i.uuid()],
                            &MediaUI::mediaStatusChanged,
                            this,
                            &PlaylistUI::offlineMediaCountChanged);

                        uuid_media_[i.uuid()]->initSystem(this);
                        anon_send(uuid_media_[i.uuid()]->as_actor(), backend_atom_v, i.actor());
                    }
                }

                // remove old media..
                for (auto it = uuid_media_.begin(); it != uuid_media_.end(); ++it) {
                    bool found = false;
                    for (auto i : actors) {
                        if (i.uuid() == it->first) {
                            found = true;
                            break;
                        }
                    }
                    if (not found) {
                        delete (it->second);
                        it = uuid_media_.erase(it);

                        // hitting a weird bug in this for loop
                        // if  uuid_media_ gets emptied it hangs
                        // unless we break here
                        if (!uuid_media_.size())
                            break;
                    }
                }

                // rebuild list.
                media_.clear();
                media_order_.clear();
                auto ind = 0;
                for (auto i : actors) {
                    media_.append(static_cast<QObject *>(uuid_media_[i.uuid()]));
                    media_order_[QUuidFromUuid(i.uuid()).toString()] = QVariant::fromValue(ind);
                    ind++;
                }


                media_model_.populate(media_);

                emit offlineMediaCountChanged();
                emit mediaOrderChanged();
                emit mediaListChanged();

                spdlog::debug(
                    "PlaylistUI event_atom, media_content_changed_atom message handler took {} "
                    "milliseconds to complete",
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        utility::clock::now() - tt)
                        .count());
            },

            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid &,
                const Uuid &,
                const bool) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid &,
                const std::string &) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::remove_container_atom,
                const std::vector<Uuid> &) {},

            [=](utility::event_atom, playlist::remove_container_atom, const Uuid &) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::rename_container_atom,
                const Uuid &,
                const std::string &) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom, utility::change_atom) { update_on_change(); },

            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {
                update_on_change(false);
            },

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                if (name != name_) {
                    name_ = name;
                    emit nameChanged();
                }
            },

            [=](utility::last_changed_atom, const time_point &) {}};
    });
}

QUuid PlaylistUI::findCUuidFromUuid(const utility::Uuid &uuuid) const {
    QMap<QUuid, QObject *>::const_iterator i = items_.constBegin();
    QUuid cuuid;

    while (i != items_.constEnd()) {
        if (uuid(i.value()) == uuuid)
            return i.key();
        // else if(auto item = dynamic_cast<ContainerGroupUI *>(i)){
        //     cuuid = item->findCUuidFromUuid(uuid);
        //     if(cuuid != QUuid())
        //         return cuuid;
        // }

        ++i;
    }

    return cuuid;
}

QObject *PlaylistUI::findItem(const utility::Uuid &suuid) {
    QUuid qu = QUuidFromUuid(suuid);

    auto f = items_.find(qu);
    if (f != items_.end())
        return *f;

    for (auto &i : items_) {
        if (uuid(i) == suuid)
            return i;
        if (auto item = dynamic_cast<ContainerGroupUI *>(i)) {
            QObject *found = item->findItem(suuid);
            if (found)
                return found;
        }
    }
    return nullptr;
}

utility::Uuid PlaylistUI::uuid(const QObject *obj) const {
    if (auto item = dynamic_cast<const MediaUI *>(obj)) {
        return UuidFromQUuid(item->uuid());
    } else if (auto item = dynamic_cast<const TimelineUI *>(obj)) {
        return item->uuid();
    } else if (auto item = dynamic_cast<const SubsetUI *>(obj)) {
        return item->uuid();
    } else if (auto item = dynamic_cast<const ContactSheetUI *>(obj)) {
        return item->uuid();
    } else if (auto item = dynamic_cast<const ContainerDividerUI *>(obj)) {
        return item->uuid();
    } else if (auto item = dynamic_cast<const ContainerGroupUI *>(obj)) {
        return item->uuid();
    }

    return utility::Uuid();
}

utility::Uuid PlaylistUI::cuuid(const QObject *obj) const {
    if (auto item = dynamic_cast<const PlaylistUI *>(obj)) {
        return item->cuuid();
    } else if (auto item = dynamic_cast<const TimelineUI *>(obj)) {
        return item->cuuid();
    } else if (auto item = dynamic_cast<const SubsetUI *>(obj)) {
        return item->cuuid();
    } else if (auto item = dynamic_cast<const ContactSheetUI *>(obj)) {
        return item->cuuid();
    } else if (auto item = dynamic_cast<const ContainerDividerUI *>(obj)) {
        return item->cuuid();
    } else if (auto item = dynamic_cast<const ContainerGroupUI *>(obj)) {
        return item->cuuid();
    }

    return utility::Uuid();
}

QUuid PlaylistUI::createTimeline(const QString &name, const QUuid &before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<UuidUuidActor>(
            *sys,
            backend_,
            playlist::create_timeline_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_item.second.uuid());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid PlaylistUI::createSubset(const QString &name, const QUuid &before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<UuidUuidActor>(
            *sys,
            backend_,
            playlist::create_subset_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_item.second.uuid());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid PlaylistUI::createContactSheet(
    const QString &name, const QUuid &before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<UuidUuidActor>(
            *sys,
            backend_,
            playlist::create_contact_sheet_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_item.second.uuid());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid PlaylistUI::createDivider(const QString &name, const QUuid &before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<Uuid>(
            *sys,
            backend_,
            playlist::create_divider_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_item);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid PlaylistUI::createGroup(const QString &name, const QUuid &before) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<Uuid>(
            *sys,
            backend_,
            playlist::create_group_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before));
        result = QUuidFromUuid(new_item);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

bool PlaylistUI::reflagContainer(const QString &flag, const QUuid &uuid) {
    bool result = false;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<bool>(
            *sys,
            backend_,
            playlist::reflag_container_atom_v,
            StdFromQString(flag),
            UuidFromQUuid(uuid));
        result = new_item;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


bool PlaylistUI::renameContainer(const QString &name, const QUuid &uuid) {
    bool result = false;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<bool>(
            *sys,
            backend_,
            playlist::rename_container_atom_v,
            StdFromQString(name),
            UuidFromQUuid(uuid));
        result = new_item;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

bool PlaylistUI::removeContainer(const QUuid &uuid, const bool remove_media) {
    bool result = false;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<bool>(
            *sys,
            backend_,
            playlist::remove_container_atom_v,
            UuidFromQUuid(uuid),
            remove_media);
        result = new_item;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

bool PlaylistUI::removeMedia(const QUuid &uuid) {
    bool result = false;
    try {
        scoped_actor sys{system()};
        result = request_receive<bool>(
            *sys, backend_, playlist::remove_media_atom_v, UuidFromQUuid(uuid));
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


bool PlaylistUI::moveContainer(const QUuid &uuid, const QUuid &before_uuid, const bool into) {
    bool result = false;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<bool>(
            *sys,
            backend_,
            playlist::move_container_atom_v,
            UuidFromQUuid(uuid),
            UuidFromQUuid(before_uuid),
            into);
        result = new_item;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


QUuid PlaylistUI::duplicateContainer(
    const QUuid &cuuid, const QUuid &before_cuuid, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_playlist = request_receive<UuidVector>(
            *sys,
            backend_,
            playlist::duplicate_container_atom_v,
            UuidFromQUuid(cuuid),
            UuidFromQUuid(before_cuuid),
            into);
        result = QUuidFromUuid(new_playlist[0]);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

void PlaylistUI::updateItemModel(const bool select_new_items) {

    auto tt = utility::clock::now();

    try {
        scoped_actor sys{system()};
        std::map<Uuid, caf::actor> uuid_actor;
        std::map<Uuid, QObject *> hold;
        QList<QObject *> items_list_;

        auto rec_items_container = request_receive<utility::PlaylistTree>(
            *sys, backend_, playlist::get_container_atom_v);

        if (last_changed_tree_ == rec_items_container)
            return;

        last_changed_tree_ = rec_items_container;

        // build look up for playlist actors
        for (const auto &i : request_receive<std::vector<UuidActor>>(
                 *sys, backend_, playlist::get_container_atom_v, true)) {
            uuid_actor[i.uuid()] = i.actor();
        }
        // decompose..
        // all objects now in hold.

        bool last_selected_still_valid = false;

        while (!items_.empty()) {
            if (auto item = dynamic_cast<TimelineUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<SubsetUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<ContactSheetUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<ContainerDividerUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<ContainerGroupUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
                while (not item->items_.empty()) {
                    if (auto iitem = dynamic_cast<ContainerDividerUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    } else if (auto iitem = dynamic_cast<TimelineUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    } else if (auto iitem = dynamic_cast<SubsetUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    } else if (auto iitem = dynamic_cast<ContactSheetUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    }
                    item->items_.removeAt(0);
                }
            }
            items_.erase(items_.begin());
        }

        QVariantList new_objects;
        // Our QList is a collection of (playlists and playlist groups)
        // we use uuids as the index, as names may change..
        // playlist groups are only at the top level.
        for (auto &i : rec_items_container.children_ref()) {
            // auto i = dynamic_cast<PlaylistTree &>(i_);

            // spdlog::info("{} {} {} {}", to_string(i.uuid()), i.value().name(),
            // i.value().type(), to_string(i.value().uuid())); is item in hold?
            auto qcu  = QUuidFromUuid(i.uuid());
            auto uuid = i.value().uuid();
            auto type = i.value().type();

            if (hold.count(uuid)) {
                if (type == "ContainerGroup") {
                    auto obj = dynamic_cast<ContainerGroupUI *>(hold[uuid]);
                    obj->setName(QStringFromStd(i.value().name()));
                } else if (type == "ContainerDivider") {
                    auto obj = dynamic_cast<ContainerDividerUI *>(hold[uuid]);
                    obj->setName(QStringFromStd(i.value().name()));
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "Timeline") {
                    auto obj = dynamic_cast<TimelineUI *>(hold[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "Subset") {
                    auto obj = dynamic_cast<SubsetUI *>(hold[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "ContactSheet") {
                    auto obj = dynamic_cast<ContactSheetUI *>(hold[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                }

                if (hold[uuid] == selected_subitem_) {
                    last_selected_still_valid = true;
                }
                items_list_.append(hold[uuid]);
                items_[qcu] = hold[uuid];
                hold.erase(uuid);
            } else {
                // new item
                if (type == "ContainerGroup") {
                    auto obj = new ContainerGroupUI(
                        i.uuid(),
                        i.value().name(),
                        i.value().type(),
                        i.value().flag(),
                        i.value().uuid(),
                        this);
                    new_objects.append(qcu);
                    items_[qcu] = obj;
                    items_list_.append(obj);
                } else if (type == "ContainerDivider") {
                    new_objects.append(qcu);
                    auto obj = new ContainerDividerUI(
                        i.uuid(),
                        i.value().name(),
                        i.value().type(),
                        i.value().flag(),
                        i.value().uuid(),
                        this);
                    new_objects.append(qcu);
                    items_[qcu] = obj;
                    items_list_.append(obj);
                } else if (type == "Timeline") {
                    new_objects.append(qcu);
                    auto obj    = new TimelineUI(i.uuid(), this);
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    obj->init(system());
                    obj->set_backend(uuid_actor[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "Subset") {
                    new_objects.append(qcu);
                    auto obj    = new SubsetUI(i.uuid(), this);
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    obj->init(system());
                    obj->set_backend(uuid_actor[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "ContactSheet") {
                    new_objects.append(qcu);
                    auto obj    = new ContactSheetUI(i.uuid(), this);
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    obj->init(system());
                    obj->set_backend(uuid_actor[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                }
            }

            if (type == "ContainerGroup") {
                auto new_group = dynamic_cast<ContainerGroupUI *>(items_[qcu]);
                // as it's a groups we need to scan it..
                for (const auto &ii : i.children_ref()) {
                    // auto ii = dynamic_cast<const PlaylistTree &>(ii_);
                    spdlog::info(
                        "    {} {} {}",
                        ii.value().name(),
                        ii.value().type(),
                        to_string(ii.value().uuid()));
                    auto chtype = ii.value().type();
                    auto chuuid = ii.value().uuid();

                    if (hold.count(chuuid)) {
                        new_group->items_.append(hold[chuuid]);
                        if (chtype == "ContainerGroup") {
                            dynamic_cast<ContainerGroupUI *>(hold[chuuid])
                                ->setName(QStringFromStd(ii.value().name()));
                        } else if (chtype == "ContainerDivider") {
                            dynamic_cast<ContainerDividerUI *>(hold[chuuid])
                                ->setName(QStringFromStd(ii.value().name()));
                            dynamic_cast<ContainerDividerUI *>(hold[chuuid])
                                ->setFlag(QStringFromStd(ii.value().flag()));
                        } else if (chtype == "Timeline") {
                            auto obj = dynamic_cast<TimelineUI *>(hold[chuuid]);
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        } else if (chtype == "Subset") {
                            auto obj = dynamic_cast<SubsetUI *>(hold[chuuid]);
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        } else if (chtype == "ContactSheet") {
                            auto obj = dynamic_cast<ContactSheetUI *>(hold[chuuid]);
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        }
                        hold.erase(chuuid);
                    } else if (chtype == "ContainerDivider") {
                        new_objects.append(QUuidFromUuid(chuuid));
                        auto obj = new ContainerDividerUI(
                            ii.uuid(),
                            ii.value().name(),
                            ii.value().type(),
                            ii.value().flag(),
                            ii.value().uuid(),
                            this);
                        new_group->items_.append(obj);
                    } else if (chtype == "Timeline") {
                        new_objects.append(QUuidFromUuid(chuuid));
                        auto obj = new TimelineUI(i.uuid(), this);
                        new_group->items_.append(obj);
                        obj->init(system());
                        obj->set_backend(uuid_actor[chuuid]);
                        obj->setFlag(QStringFromStd(ii.value().flag()));
                    } else if (chtype == "Subset") {
                        new_objects.append(QUuidFromUuid(chuuid));
                        auto obj = new SubsetUI(i.uuid(), this);
                        new_group->items_.append(obj);
                        obj->init(system());
                        obj->set_backend(uuid_actor[chuuid]);
                        obj->setFlag(QStringFromStd(ii.value().flag()));
                    } else if (chtype == "ContactSheet") {
                        new_objects.append(QUuidFromUuid(chuuid));
                        auto obj = new ContactSheetUI(i.uuid(), this);
                        new_group->items_.append(obj);
                        obj->init(system());
                        obj->set_backend(uuid_actor[chuuid]);
                        obj->setFlag(QStringFromStd(ii.value().flag()));
                    }
                }
                new_group->changedItems();
            }
        }

        // these are gone.
        for (auto &i : hold) {
            // we should notify the item that it's backend is gone..
            if (auto item = dynamic_cast<TimelineUI *>(i.second)) {
                item->set_backend(caf::actor());
            } else if (auto item = dynamic_cast<SubsetUI *>(i.second)) {
                item->set_backend(caf::actor());
            } else if (auto item = dynamic_cast<ContactSheetUI *>(i.second)) {
                item->set_backend(caf::actor());
            }
        }

        if (!last_selected_still_valid and selected_subitem_) {
            selected_subitem_ = nullptr;
            emit selectedSubitemChanged();
        }

        // select first new object.
        if (new_objects.count() and select_new_items) {
            auto i = new_objects[0];
            new_objects.clear();
            new_objects.append(i);
            setSelection(new_objects, true);
        }


        model_.populate(items_list_, this);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    spdlog::debug(
        "PlaylistUI::updateItemModeltook {} milliseconds to complete",
        std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now() - tt)
            .count());
}

// find item based off container uuid or actor uuid
QObject *PlaylistUI::getNextItem(const utility::Uuid &cuuid) {
    // might be media uuid ?
    if (uuid_media_.count(cuuid)) {
        // find nextmedia
        QObject *mojb = uuid_media_[cuuid];
        int ind       = 0;
        for (auto &i : media_) {
            if (i == mojb) {
                ind++;
                if (media_.size() > ind) {
                    return media_[ind];
                }
                break;
            }
            ind++;
        }
    }

    try {
        scoped_actor sys{system()};
        auto rec_items_container = request_receive<utility::PlaylistTree>(
            *sys, backend_, playlist::get_container_atom_v);
        auto next_item = rec_items_container.find_next_at_same_depth(cuuid);
        if (next_item)
            return findItem(*next_item);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return nullptr;
}

QObject *PlaylistUI::findMediaObject(const QUuid &quuid) {
    auto uuid = UuidFromQUuid(quuid);

    if (uuid_media_.count(uuid)) {
        return uuid_media_[uuid];
    }
    return nullptr;
}


QUuid PlaylistUI::getNextItemUuid(const QUuid &quuid) {
    QUuid result;

    auto obj = getNextItem(UuidFromQUuid(quuid));
    if (obj)
        return QUuidFromUuid(uuid(obj));

    try {
        scoped_actor sys{system()};
        auto rec_items_container = request_receive<utility::PlaylistTree>(
            *sys, backend_, playlist::get_container_atom_v);
        auto next_item =
            rec_items_container.find_next_value_at_same_depth(UuidFromQUuid(quuid));
        if (next_item)
            result = QUuidFromUuid(*next_item);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool PlaylistUI::contains(const QUuid &uuid_or_cuuid, QUuid &cuuid) const {
    QMap<QUuid, QObject *>::const_iterator i = items_.begin();

    while (i != items_.end()) {
        if (i.key() == uuid_or_cuuid) {
            cuuid = uuid_or_cuuid;
            return true;
        }
        QObject *oo = i.value();
        if (auto *ui = dynamic_cast<SubsetUI *>(oo)) {
            if (ui->quuid() == uuid_or_cuuid) {
                cuuid = ui->qcuuid();
                return true;
            }
        }
        if (auto *ui = dynamic_cast<TimelineUI *>(oo)) {
            if (ui->quuid() == uuid_or_cuuid) {
                cuuid = ui->qcuuid();
                return true;
            }
        }
        if (auto *ui = dynamic_cast<ContactSheetUI *>(oo)) {
            if (ui->quuid() == uuid_or_cuuid) {
                cuuid = ui->qcuuid();
                return true;
            }
        }
        ++i;
    }

    return false;
}

QUuid PlaylistUI::convertToContactSheet(
    const QUuid &uuid_or_cuuid, const QString &name, const QUuid &before, const bool) {
    QUuid result;

    scoped_actor sys{system()};
    try {
        auto uua = request_receive<std::pair<utility::Uuid, UuidActor>>(
            *sys,
            backend_,
            playlist::convert_to_contact_sheet_atom_v,
            UuidFromQUuid(uuid_or_cuuid),
            StdFromQString(name),
            UuidFromQUuid(before));
        result = QUuidFromUuid(uua.first);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

QUuid PlaylistUI::convertToSubset(
    const QUuid &uuid_or_cuuid, const QString &name, const QUuid &before, const bool) {
    QUuid result;
    scoped_actor sys{system()};
    try {
        auto uua = request_receive<std::pair<utility::Uuid, UuidActor>>(
            *sys,
            backend_,
            playlist::convert_to_subset_atom_v,
            UuidFromQUuid(uuid_or_cuuid),
            StdFromQString(name),
            UuidFromQUuid(before));
        result = QUuidFromUuid(uua.first);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool PlaylistUI::contains_media(const QUuid &key) const {
    return uuid_media_.count(UuidFromQUuid(key));
}

void PlaylistUI::build_media_order() {
    media_order_.clear();
    for (auto i = 0; i < media_.size(); i++) {
        media_order_[QUuidFromUuid(uuid(media_[i])).toString()] = QVariant::fromValue(i);
    }
}

[[nodiscard]] int PlaylistUI::offlineMediaCount() const {
    auto offline = 0;
    for (const auto &i : media_) {
        auto m = qobject_cast<MediaUI *>(i);
        if (m and not m->mediaOnline())
            offline++;
    }
    return offline;
}
