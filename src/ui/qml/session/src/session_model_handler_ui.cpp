// SPDX-License-Identifier: Apache-2.0


#include "xstudio/media/media.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/ui/qml/caf_response_ui.hpp"
#include "xstudio/ui/qml/job_control_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"

CAF_PUSH_WARNINGS
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QSignalSpy>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;


void SessionModel::updateMedia() {
    mediaStatusChangePending_ = false;
    emit mediaStatusChanged(mediaStatusIndex_);
    mediaStatusIndex_ = QModelIndex();
}

void SessionModel::triggerMediaStatusChange(const QModelIndex &index) {
    if (mediaStatusChangePending_ and mediaStatusIndex_ == index) {
        // no op
    } else if (mediaStatusChangePending_) {
        emit mediaStatusChanged(index);
    } else {
        mediaStatusChangePending_ = true;
        mediaStatusIndex_         = index;
        QTimer::singleShot(100, this, SLOT(updateMedia()));
    }
}

void SessionModel::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(
        [this](caf::scheduled_actor *, caf::message &msg) -> caf::skippable_result {
            //  UNCOMMENT TO DEBUG UNEXPECT MESSAGES

            spdlog::warn(
                "Got adfd from {} {}", to_string(self()->current_sender()), to_string(msg));

            return message{};
        });
    setModified(false, true);

    try {
        utility::print_on_create(as_actor(), "SessionModel");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {

            [=](utility::event_atom, timeline::item_atom, const timeline::Item &) {
                // spdlog::info("utility::event_atom, timeline::item_atom, const timeline::Item
                // &");
            },
            [=](utility::event_atom,
                timeline::item_atom,
                const JsonStore &event,
                const bool silent) {
                try {
                    // spdlog::info("utility::event_atom, timeline::item_atom, {}, {}",
                    // event.dump(2), silent);
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    if (timeline_lookup_.count(src)) {
                        // spdlog::warn("update timeline");
                        if (not timeline_lookup_[src].update(event).empty()) {
                            // refresh ?
                            // timeline_lookup_[src].refresh(-1);
                            // spdlog::warn("state changed");
                            // spdlog::warn("{}", timeline_lookup_[src].serialise(-1).dump(2));
                        }
                    } else {
                        // spdlog::warn("failed update timeline");
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](json_store::update_atom, const utility::JsonStore &) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    auto indexes = searchRecursiveList(
                        QVariant::fromValue(QStringFromStd(src_str)),
                        actorRole,
                        QModelIndex(),
                        0,
                        -1);

                    for (const auto index : indexes) {
                        if (index.isValid())
                            emit dataChanged(index, index, QVector<int>({metadataChangedRole}));
                        // setData(index, QVariant(), metadataChangedRole);
                    }
                } catch (...) {
                }
            },

            [=](json_store::update_atom,
                const utility::JsonStore &,
                const std::string &path,
                const utility::JsonStore &) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    auto indexes = searchRecursiveList(
                        QVariant::fromValue(QStringFromStd(src_str)),
                        actorRole,
                        QModelIndex(),
                        0,
                        -1);


                    for (const auto index : indexes) {
                        if (index.isValid())
                            emit dataChanged(index, index, QVector<int>({metadataChangedRole}));
                        //         setData(index, QVariant(), metadataChangedRole);
                    }

                } catch (...) {
                }
            },

            [=](utility::event_atom,
                media_metadata::get_metadata_atom,
                const utility::JsonStore &jsn) {},

            [=](utility::event_atom,
                media::media_status_atom,
                const media::MediaStatus status) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    // src_str); search from index..
                    receivedData(
                        json(src_str), actorRole, QModelIndex(), mediaStatusRole, json(status));
                } catch (...) {
                }
            },

            [=](utility::event_atom, utility::notification_atom, const JsonStore &digest) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    // src_str); search from index..
                    receivedData(
                        json(src_str), actorRole, QModelIndex(), notificationRole, digest);
                } catch (...) {
                }
            },

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                // find this actor... in our data..
                // we need to map this to a string..
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // spdlog::info("event_atom name_atom {} {} {}", name, to_string(src), src_str);
                // search from index..
                receivedData(json(src_str), actorRole, QModelIndex(), nameRole, json(name));
            },

            [=](utility::event_atom,
                media::media_display_info_atom,
                const utility::JsonStore &info) {
                // this comes direct from media actor, we don't use as we
                // prefer to get it via the playlist to reduce the searching
                // that we have to do
            },

            [=](utility::event_atom,
                media::media_display_info_atom,
                const utility::JsonStore &info,
                caf::actor_addr media) {
                // re-broadcast of media_display_info_atom event that came from
                // a MediaActor. The playlist has done the re-braodcast

                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // request update of children..
                // find container owner..
                auto playlist_index =
                    searchRecursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                if (playlist_index.isValid()) {

                    auto m         = caf::actor_cast<caf::actor>(media);
                    auto media_str = actorToString(system(), m);

                    auto indeces = searchRecursiveList(
                        QVariant::fromValue(QStringFromStd(media_str)),
                        actorRole,
                        playlist_index,
                        0,
                        -1);

                    for (const auto index : indeces) {
                        if (index.isValid()) {
                            // request update of containers.
                            try {
                                nlohmann::json &j = indexToData(index);
                                if (j.at("type") == "Media") {
                                    if (j.count("media_display_info") and
                                        j.at("media_display_info") != info) {
                                        j["media_display_info"] = info;
                                        emit dataChanged(
                                            index, index, QVector<int>({mediaDisplayInfoRole}));
                                    }
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }
                    }
                }
            },

            [=](utility::event_atom,
                media::add_media_source_atom,
                const utility::UuidActorVector &uav) {
                try {
                    // spdlog::info(
                    //     "event_atom add_media_source_atom {} {} {}",
                    //     to_string(uav[0].uuid()),
                    //     to_string(uav[0].actor()), uav.size());
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    auto indexes = searchRecursiveList(
                        QVariant::fromValue(QStringFromStd(src_str)),
                        actorRole,
                        QModelIndex(),
                        0,
                        1);

                    if (not indexes.empty() and indexes[0].isValid()) {
                        const nlohmann::json &j = indexToData(indexes[0]);
                        requestData(
                            QVariant::fromValue(QStringFromStd(src_str)),
                            actorRole,
                            getPlaylistIndex(indexes[0]),
                            j,
                            JSONTreeModel::Roles::childrenRole);

                    } else {
                        spdlog::warn("FAIELD");
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom, media_metadata::get_metadata_atom, const JsonStore &jsn) {
            },


            [=](utility::event_atom, playlist::remove_media_atom, const UuidVector &removed) {
                // spdlog::info("event_atom playlist::remove_media_atom {}", removed.size());
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    auto index   = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    if (index.isValid()) {
                        const nlohmann::json &j = indexToData(index);
                        if (j.at("type") == "ContactSheet" or j.at("type") == "Subset" or j.at("type") == "Timeline" or
                            j.at("type") == "Playlist") {
                            // get media container index
                            index                    = index.model()->index(0, 0, index);
                            const nlohmann::json &jj = indexToData(index);
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                idRole,
                                index,
                                index,
                                JSONTreeModel::Roles::childrenRole);
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom, playlist::add_media_atom, const UuidActorVector &) {
                auto src = caf::actor_cast<caf::actor>(self()->current_sender());

                if (src != session_actor_) {

                    auto src_str = actorToString(system(), src);

                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    // trigger update of model..
                    if (index.isValid()) {
                        try {
                            // request update of containers.
                            const nlohmann::json &j = indexToData(index);
                            emit mediaAdded(index);

                            index = SessionModel::index(0, 0, index);
                            if (index.isValid()) {
                                const nlohmann::json &jj = indexToData(index);

                                // spdlog::info(
                                //     "utility::event_atom, playlist::add_media_atom {} {} {}",
                                //     to_string(jj.at("id").get<Uuid>()),
                                //     src_str,
                                //     jj.dump(2));

                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    jj,
                                    JSONTreeModel::Roles::childrenRole);
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }

                } else {
                    // ignore this message from the session actor..
                    // not sure why it want's this..
                }
            },

            [=](utility::event_atom, playlist::create_divider_atom, const Uuid &uuid) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // spdlog::info(
                //     "create_divider_atom {} {} {}", to_string(src), src_str,
                //     to_string(uuid));
                // request update of children..
                // find container owner..
                auto index =
                    searchRecursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                if (index.isValid()) {
                    // request update of containers.
                    try {
                        const nlohmann::json &j = indexToData(index);

                        if (j.at("type") == "Playlist") {
                            // spdlog::warn("create_divider_atom Playlist {}", j.dump(2));

                            index = SessionModel::index(2, 0, index);
                            if (index.isValid()) {
                                const nlohmann::json &jj = indexToData(index);
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                    idRole,
                                    index,
                                    index,
                                    JSONTreeModel::Roles::childrenRole);
                            }
                        } else {
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                index,
                                index,
                                JSONTreeModel::Roles::childrenRole);
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom, playlist::create_group_atom, const Uuid & /*uuid*/) {},

            [=](utility::event_atom,
                session::path_atom,
                const std::pair<caf::uri, fs::file_time_type> &pt) {
                // spdlog::warn("path_event {}", to_string(pt.first));

                // update session path/mtime
                try {
                    auto index        = SessionModel::index(0, 0);
                    nlohmann::json &j = indexToData(index);

                    requestData(
                        QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                        idRole,
                        index,
                        index,
                        pathRole);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom, playlist::loading_media_atom, const bool value) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // request update of children..
                // find container owner..
                auto index =
                    searchRecursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                if (index.isValid()) {
                    // request update of containers.
                    try {
                        nlohmann::json &j = indexToData(index);
                        if (j.at("type") == "Playlist") {
                            if (j.count("busy") and j.at("busy") != value) {
                                j["busy"] = value;
                                emit dataChanged(index, index, QVector<int>({busyRole}));
                            }
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },

            [=](utility::event_atom,
                session::active_media_container_atom,
                utility::UuidActor &a) {
                // this comes from the backend SessionActor
                updateCurrentMediaContainerIndexFromBackend();
            },

            [=](utility::event_atom,
                session::viewport_active_media_container_atom,
                utility::UuidActor &a) {
                // this comes from the backend SessionActor
                updateViewportCurrentMediaContainerIndexFromBackend();
            },

            [=](utility::event_atom,
                ui::viewport::viewport_playhead_atom,
                utility::Uuid playhead_uuid) {
                // this comes from the backend SessionActor
                if (QUuidFromUuid(playhead_uuid) != on_screen_playhead_uuid_) {
                    on_screen_playhead_uuid_ = QUuidFromUuid(playhead_uuid);
                    emit onScreenPlayheadUuidChanged();
                }
            },

            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid &src,
                const Uuid &before,
                const bool) {
                // spdlog::info("utility::event_atom, playlist::move_container_atom");
                auto src_index =
                    searchRecursive(QVariant::fromValue(QUuidFromUuid(src)), containerUuidRole);

                if (src_index.isValid()) {
                    if (before.is_null()) {
                        JSONTreeModel::moveRows(
                            src_index.parent(),
                            src_index.row(),
                            1,
                            src_index.parent(),
                            rowCount(src_index.parent()));
                    } else {
                        auto before_index = searchRecursive(
                            QVariant::fromValue(QUuidFromUuid(before)), containerUuidRole);
                        // spdlog::warn("move before {} {}", src_index.row(),
                        // before_index.row());
                        if (before_index.isValid()) {
                            JSONTreeModel::moveRows(
                                src_index.parent(),
                                src_index.row(),
                                1,
                                before_index.parent(),
                                before_index.row());
                        } else {
                            // spdlog::warn("unfound before");
                        }
                    }
                } else {
                    // spdlog::warn("unfound to end");
                }
                // spdlog::warn("{}", data_.dump(2));
            },


            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid &src,
                const Uuid &before) {
                // spdlog::info("utility::event_atom, playlist::move_container_atom");
                auto src_index =
                    searchRecursive(QVariant::fromValue(QUuidFromUuid(src)), containerUuidRole);

                if (src_index.isValid()) {
                    if (before.is_null()) {
                        JSONTreeModel::moveRows(
                            src_index.parent(),
                            src_index.row(),
                            1,
                            src_index.parent(),
                            rowCount(src_index.parent()));
                    } else {
                        auto before_index = searchRecursive(
                            QVariant::fromValue(QUuidFromUuid(before)), containerUuidRole);
                        // spdlog::warn("move before {} {}", src_index.row(),
                        // before_index.row());
                        if (before_index.isValid()) {
                            JSONTreeModel::moveRows(
                                src_index.parent(),
                                src_index.row(),
                                1,
                                before_index.parent(),
                                before_index.row());
                        } else {
                            // spdlog::warn("unfound before");
                        }
                    }
                } else {
                    // spdlog::warn("unfound to end");
                }
                // spdlog::warn("{}", data_.dump(2));
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid &uuid,
                const std::string &value) {
                // spdlog::info("reflag_container_atom {} {}", to_string(uuid), value);
                receivedData(
                    json(uuid), containerUuidRole, QModelIndex(), flagColourRole, json(value));
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid &uuid,
                const std::tuple<std::string, std::string> &value) {
                // spdlog::info("reflag_container_atom {}", to_string(uuid));

                const auto [flag, text] = value;
                receivedData(
                    json(uuid), actorUuidRole, QModelIndex(), flagColourRole, json(flag));
                receivedData(
                    json(uuid), actorUuidRole, QModelIndex(), flagTextRole, json(text));
            },

            [=](utility::event_atom,
                media::current_media_source_atom,
                const utility::UuidActor &ua) {
                // spdlog::warn(
                //     "utility::event_atom, media::current_media_source_atom {}",
                //     to_string(ua.uuid()));
            },

            [=](utility::event_atom,
                media_reader::get_thumbnail_atom,
                const thumbnail::ThumbnailBufferPtr &buf) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                media_thumbnails_[QStringFromStd(src_str)] = QImage(
                    (uchar *)&(buf->data()[0]),
                    buf->width(),
                    buf->height(),
                    3 * buf->width(),
                    QImage::Format_RGB888);
                auto media_indexes = searchRecursiveList(
                    QVariant::fromValue(QStringFromStd(src_str)),
                    actorRole,
                    QModelIndex(),
                    0,
                    -1);
                for (const auto &idx : media_indexes) {
                    emit dataChanged(idx, idx, QVector<int>({thumbnailImageRole}));
                }
            },

            [=](utility::event_atom,
                media::current_media_source_atom,
                const utility::UuidActor &ua,
                const media::MediaType mt) {
                START_SLOW_WATCHER()

                // comes from media actor..
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);

                // spdlog::warn(
                //     "utility::event_atom, media::current_media_source_atom {} {} {}",
                //     to_string(ua.uuid()),
                //     mt,
                //     src_str);

                // find first instance of media
                auto media_source_variant =
                    QVariant::fromValue(QStringFromStd(actorToString(system(), ua.actor())));
                auto media_actor_variant = QVariant::fromValue(QStringFromStd(src_str));

                auto media_indexes = searchRecursiveList(
                    QVariant::fromValue(QStringFromStd(src_str)),
                    actorRole,
                    QModelIndex(),
                    0,
                    -1);

                auto media_index = QModelIndex();
                if (not media_indexes.empty())
                    media_index = media_indexes[0];

                if (media_index.isValid()) {
                    auto plindex = getPlaylistIndex(media_index);

                    // trigger model update.
                    if (mt == media::MediaType::MT_IMAGE) {
                        receivedData(
                            json(src_str),
                            actorRole,
                            plindex,
                            imageActorUuidRole,
                            json(ua.uuid()));
                    } else {
                        receivedData(
                            json(src_str),
                            actorRole,
                            plindex,
                            audioActorUuidRole,
                            json(ua.uuid()));
                    }

                    // trigger media status check..
                    if (mt == media::MediaType::MT_IMAGE)
                        requestData(
                            media_actor_variant,
                            actorRole,
                            plindex,
                            media_index,
                            mediaStatusRole);

                    // get playlist..
                    if (plindex.isValid()) {
                        triggerMediaStatusChange(plindex);

                        // for each instance of this media emit a source change event.
                        for (const auto &i : media_indexes) {
                            if (i.isValid()) {
                                auto media_source_index =
                                    search(media_source_variant, actorRole, i);

                                if (media_source_index.isValid()) {
                                    emit mediaSourceChanged(
                                        i, media_source_index, static_cast<int>(mt));
                                }
                            }
                        }
                    }
                }
                CHECK_SLOW_WATCHER()
            },

            [=](utility::event_atom,
                bookmark::bookmark_change_atom,
                const utility::Uuid &uuid) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);

                auto qactor = QVariant::fromValue(QStringFromStd(src_str));

                auto mindex = searchRecursive(qactor, actorRole);
                if (mindex.isValid()) {
                    auto pindex = getPlaylistIndex(mindex);
                    if (pindex.isValid()) {
                        auto media_indexes =
                            searchRecursiveList(qactor, actorRole, pindex, 0, -1);

                        for (auto &index : media_indexes) {
                            if (index.isValid() and
                                StdFromQString(index.data(typeRole).toString()) == "Media") {
                                const auto &j = indexToData(index);
                                requestData(
                                    QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                    idRole,
                                    index,
                                    index,
                                    bookmarkUuidsRole);
                            }
                        }
                    }
                }
            },

            [=](utility::event_atom,
                bookmark::bookmark_change_atom,
                const utility::Uuid &uuid,
                const utility::UuidList &uuid_list) {},

            [=](utility::event_atom,
                playlist::remove_container_atom,
                const std::vector<Uuid> &uuids) {
                for (const auto &uuid : uuids) {
                    // spdlog::info("remove_containers_atom {} {}", to_string(uuid));

                    auto index = searchRecursive(
                        QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
                    if (index.isValid()) {
                        // use base class so we don't reissue the removal.
                        JSONTreeModel::removeRows(index.row(), 1, index.parent());

                        // if(ij.at("type") == "Media List" and ij.at("children").is_array()) {
                        //     spdlog::warn("mediaCountRole {}", ij.at("children").size());
                        //     setData(index.parent(),
                        //     QVariant::fromValue(ij.at("children").size()), mediaCountRole);
                        // }
                    }
                }
            },

            [=](utility::event_atom, playlist::remove_container_atom, const Uuid &uuid) {
                // find container entry and remove it..
                // spdlog::info("remove_container_atom {} {}", to_string(uuid));
                auto index = searchRecursive(
                    QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
                if (index.isValid()) {
                    // use base class so we don't reissue the removal.
                    JSONTreeModel::removeRows(index.row(), 1, index.parent());
                }
            },

            // [=](utility::event_atom, playlist::remove_container_atom, const Uuid &uuid, const
            // bool) {
            //     // find container entry and remove it..
            //     spdlog::info("remove_container_atom {} {}", to_string(uuid));
            //     auto index = searchRecursive(
            //         QVariant::fromValue(QUuidFromUuid(uuid)), containerUuidRole);
            //     if (index.isValid()) {
            //         // use base class so we don't reissue the removal.
            //         JSONTreeModel::removeRows(index.row(), 1, index.parent());
            //     }
            // },

            [=](utility::event_atom,
                playlist::rename_container_atom,
                const Uuid &uuid,
                const std::string &name) {
                // update model..
                // spdlog::info("rename_container_atom {} {}", to_string(uuid), name);
                receivedData(
                    json(uuid), containerUuidRole, QModelIndex(), nameRole, json(name));
            },

            [=](utility::event_atom, session::add_playlist_atom, const UuidActor &ua) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // spdlog::info(
                //     "add_playlist_atom {} {} {}",
                //     to_string(src),
                //     src_str,
                //     to_string(ua.uuid()));
                // request update of children..
                // find container owner..

                auto index =
                    searchRecursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                if (index.isValid()) {
                    try {
                        const nlohmann::json &j = indexToData(index);
                        // request update of containers.
                        requestData(
                            QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                            idRole,
                            index,
                            index,
                            JSONTreeModel::Roles::childrenRole);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
                // spdlog::info(
                //     "add_playlist_atom {} {} {}",
                //     to_string(src),
                //     src_str,
                //     to_string(ua.uuid()));
            },

            [=](utility::event_atom, session::media_rate_atom, const FrameRate &rate) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);

                if (src == session_actor_) {
                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);
                    if (index.isValid()) {
                        try {
                            const nlohmann::json &j = indexToData(index);
                            receivedData(j.at("id"), idRole, index, rateFPSRole, json(rate));
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                }
            },

            [=](utility::event_atom,
                playlist::media_content_changed_atom,
                const utility::UuidActorVector &) {
                auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                auto src_str = actorToString(system(), src);
                // spdlog::info(
                //     "utility::event_atom, playlist::media_content_changed_atom {}", src_str);

                auto index =
                    searchRecursive(QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                // trigger update of model..
                if (index.isValid()) {

                    emit mediaAdded(index);

                    // request update of containers.
                    try {
                        const nlohmann::json &j = indexToData(index);

                        index = SessionModel::index(0, 0, index);


                        if (index.isValid()) {
                            const nlohmann::json &jj = indexToData(index);
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                idRole,
                                index,
                                index,
                                JSONTreeModel::Roles::childrenRole);
                        }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },


            [=](utility::event_atom,
                playhead::source_atom,
                const std::vector<caf::actor> &actors) {
                // update media selection model.
                // PlayheadSelectionActor

                try {
                    auto src          = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str      = actorToString(system(), src);
                    auto search_value = QVariant::fromValue(QStringFromStd(src_str));

                    auto playlists = searchList(
                        QVariant::fromValue(QString("Playlist")),
                        typeRole,
                        index(0, 0, QModelIndex()),
                        0,
                        -1);

                    auto psindex = QModelIndex();
                    for (const auto &i : playlists) {
                        // spdlog::warn("search playlist {} {}",
                        // StdFromQString(i.data(typeRole).toString()),
                        // StdFromQString(i.data(nameRole).toString()));
                        psindex = search(search_value, actorRole, i, 0);
                        if (psindex.isValid()) {
                            break;
                        }

                        // search directly under playlist containers.
                        psindex =
                            searchRecursive(search_value, actorRole, index(2, 0, i), 0, 1);
                        if (psindex.isValid()) {
                            // spdlog::warn("FOUND in container {} {} {}", psindex.row(),
                            // StdFromQString(psindex.data(typeRole).toString()),
                            // StdFromQString(psindex.parent().data(typeRole).toString()));
                            break;
                        }
                    }

                    if (not psindex.isValid()) {
                        psindex = searchRecursive(
                            QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                        if (psindex.isValid()) {
                            spdlog::warn(
                                "FOUND somewhere unexpected {} {} {}",
                                psindex.row(),
                                StdFromQString(psindex.data(typeRole).toString()),
                                StdFromQString(psindex.parent().data(typeRole).toString()));
                        }
                    }

                    // request update of children.
                    // trigger update of model..
                    if (psindex.isValid()) {
                        const nlohmann::json &j = indexToData(psindex);
                        if (j.at("type") == "PlayheadSelection") {
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(j.at("id"))),
                                idRole,
                                psindex,
                                psindex,
                                selectionRole);
                        }
                    }

                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },


            // NEVER TRIGGERS ? NOT TESTED !
            [=](utility::event_atom, media::add_media_stream_atom, const UuidActor &) {
                // spdlog::warn("media::add_media_stream_atom");
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    auto indexes = searchRecursiveList(
                        QVariant::fromValue(QStringFromStd(src_str)),
                        actorRole,
                        QModelIndex(),
                        0,
                        1);

                    if (not indexes.empty() and indexes[0].isValid()) {
                        const nlohmann::json &j = indexToData(indexes[0]);

                        // spdlog::warn("media::add_media_stream_atom REQUEST");
                        requestData(
                            QVariant::fromValue(QStringFromStd(src_str)),
                            actorRole,
                            getPlaylistIndex(indexes[0]),
                            j,
                            JSONTreeModel::Roles::childrenRole);
                    } else {
                        spdlog::warn("FAIELD");
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom,
                playlist::move_media_atom,
                const UuidVector &,
                const Uuid &) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);

                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    // spdlog::info("utility::event_atom, utility::move_media_atom {}",
                    // src_str);

                    // trigger update of model..
                    if (index.isValid()) {
                        const nlohmann::json &j = indexToData(index);
                        // spdlog::warn("{}", j.dump(2));
                        if (j.at("type") == "Subset" or j.at("type") == "Timeline" or j.at("type") == "ContactSheet") {
                            const auto tree = *(indexToTree(index)->child(0));
                            auto media_id   = tree.data().at("id");
                            requestData(
                                QVariant::fromValue(QUuidFromUuid(media_id)),
                                idRole,
                                index,
                                tree.data(),
                                JSONTreeModel::Roles::childrenRole);
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](utility::event_atom, utility::change_atom) {
                // arbitary change ..
                // try {
                //     auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                //     auto src_str = actorToString(system(), src);

                //     auto index =
                //         searchRecursive(QVariant::fromValue(QStringFromStd(src_str)),
                //         actorRole);

                //     spdlog::info("utility::event_atom, utility::change_atom {}", src_str);

                //     // trigger update of model..
                //     if (index.isValid()) {
                //         const nlohmann::json &j = indexToData(index);
                //         if (j.at("type") == "Subset" or j.at("type") == "Timeline") {
                //             auto media_id = j.at("children").at(0).at("id");
                //             requestData(
                //                 QVariant::fromValue(QUuidFromUuid(media_id)),
                //                 idRole,
                //                 j.at("children").at(0),
                //                 JSONTreeModel::Roles::childrenRole);
                //         }
                //     }
                // } catch (const std::exception &err) {
                //     spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                // }
            },

            [=](utility::event_atom,
                utility::last_changed_atom,
                const time_point &last_changed) {
                auto src = caf::actor_cast<caf::actor>(self()->current_sender());
                if (src == session_actor_) {
                    last_changed_ = last_changed;
                    emit modifiedChanged();
                }
            },

            [=](utility::event_atom,
                playlist::create_subset_atom,
                const utility::UuidActor &ua) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    // spdlog::info(
                    //     "create_subset_atom {} {} {}",
                    //     to_string(src),
                    //     src_str,
                    //     to_string(ua.uuid()));
                    // request update of children..
                    // find container owner..
                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    /*spdlog::info(
                        "create_subset_atom {} {} {}",
                        to_string(src),
                        src_str,
                        to_string(ua.uuid()));*/


                    if (index.isValid()) {
                        const nlohmann::json &j = indexToData(index);
                        // request update of containers.
                        try {
                            if (j.at("type") == "Playlist") {
                                // spdlog::warn("create_subset_atom Playlist {}", j.dump(2));

                                index = SessionModel::index(2, 0, index);
                                if (index.isValid()) {
                                    const nlohmann::json &jj = indexToData(index);
                                    requestData(
                                        QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                        idRole,
                                        index,
                                        index,
                                        JSONTreeModel::Roles::childrenRole);
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },
            [=](utility::event_atom,
                playlist::create_timeline_atom,
                const utility::UuidActor &ua) {
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    // spdlog::info(
                    //     "create_subset_atom {} {} {}",
                    //     to_string(src),
                    //     src_str,
                    //     to_string(ua.uuid()));
                    // request update of children..
                    // find container owner..
                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    if (index.isValid()) {
                        const nlohmann::json &j = indexToData(index);
                        // request update of containers.
                        try {
                            if (j.at("type") == "Playlist") {

                                index = SessionModel::index(2, 0, index);
                                if (index.isValid()) {
                                    const nlohmann::json &jj = indexToData(index);
                                    requestData(
                                        QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                        idRole,
                                        index,
                                        index,
                                        JSONTreeModel::Roles::childrenRole);
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },
            [=](utility::event_atom,
                playlist::create_contact_sheet_atom,
                const utility::UuidActor &ua) {
                
                try {
                    auto src     = caf::actor_cast<caf::actor>(self()->current_sender());
                    auto src_str = actorToString(system(), src);
                    auto index = searchRecursive(
                        QVariant::fromValue(QStringFromStd(src_str)), actorRole);

                    /*spdlog::info(
                        "create_contact_sheet_atom {} {} {}",
                        to_string(src),
                        src_str,
                        to_string(ua.uuid()));*/
                    // find container owner..

                    if (index.isValid()) {
                        const nlohmann::json &j = indexToData(index);
                        // request update of containers.
                        try {
                            if (j.at("type") == "Playlist") {
                                index = SessionModel::index(2, 0, index);
                                if (index.isValid()) {
                                    const nlohmann::json &jj = indexToData(index);
                                    requestData(
                                        QVariant::fromValue(QUuidFromUuid(jj.at("id"))),
                                        idRole,
                                        index,
                                        index,
                                        JSONTreeModel::Roles::childrenRole);
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](const group_down_msg &g) {
                caf::aout(self()) << "down: " << to_string(g.source) << std::endl;
            }

        };
    });
}
