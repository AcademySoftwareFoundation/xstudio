// SPDX-License-Identifier: Apache-2.0

#include <filesystem>

#include <iostream>
#include <regex>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/ui/qml/contact_sheet_ui.hpp"
#include "xstudio/ui/qml/timeline_ui.hpp"
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/ui/qml/session_ui.hpp"
#include "xstudio/ui/qml/subset_ui.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;
namespace fs = std::filesystem;


GroupModel::GroupModel(QObject *parent) : QAbstractListModel(parent) {}

int GroupModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return data_.count();
}

QVariant GroupModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= data_.count())
        return QVariant();

    if (role == ObjectRole)
        return QVariant::fromValue(data_[index.row()]);

    if (auto item = dynamic_cast<const PlaylistUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == ExpandedRole)
            return item->expanded();
        else if (role == SelectedRole)
            return item->selected();
    } else if (auto item = dynamic_cast<ContainerGroupUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == ExpandedRole)
            return item->expanded();
        else if (role == SelectedRole)
            return item->selected();
    } else if (auto item = dynamic_cast<ContainerDividerUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == SelectedRole)
            return item->selected();
    } else if (auto item = dynamic_cast<SubsetUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == SelectedRole)
            return item->selected();
    } else if (auto item = dynamic_cast<ContactSheetUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == SelectedRole)
            return item->selected();
    } else if (auto item = dynamic_cast<TimelineUI *>(data_[index.row()])) {
        if (role == TypeRole)
            return item->type();
        else if (role == NameRole)
            return item->name();
        else if (role == SelectedRole)
            return item->selected();
    }

    return QVariant();
}

QVariant GroupModel::get_object(const int sourceRow) {
    if (sourceRow < 0 || sourceRow >= data_.count())
        return QVariant();

    return QVariant::fromValue(data_[sourceRow]);
}

QVariant GroupModel::next_object(const QVariant obj) {
    for (auto i = 0; i < size(); i++) {
        if (get_object(i).value<QObject *>() == obj.value<QObject *>())
            return get_object(i + 1);
    }
    return QVariant();
}

bool GroupModel::empty() const { return data_.count() == 0; }

int GroupModel::size() const { return data_.count(); }


QHash<int, QByteArray> GroupModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TypeRole]     = "type";
    roles[NameRole]     = "name";
    roles[ObjectRole]   = "object";
    roles[ExpandedRole] = "expanded";
    roles[SelectedRole] = "selected";
    return roles;
}

// bool GroupModel::moveRows(
//     const QModelIndex &,
//     int sourceRow,
//     int count,
//     const QModelIndex &,
//     int destinationChild) {
//     spdlog::warn("{} {} {}", sourceRow, count, destinationChild);

//     beginMoveRows(QModelIndex(), sourceRow, sourceRow+(count-1), QModelIndex(),
//     destinationChild);
//     // / wrong..
//     for(auto i=0; i <count; i++)
//         mDatas.move(sourceRow+i, destinationChild+i);
//     endMoveRows();

//     return true;
// }
// bool QAbstractItemModel::beginMoveRows(const QModelIndex &sourceParent, int sourceFirst, int
// sourceLast, const QModelIndex &destinationParent, int destinationChild)

// However, when moving rows down in the same parent (sourceParent and destinationParent are
// equal), the rows will be placed before the destinationChild index. That is, if you wish to
// move rows 0 and 1 so they will become rows 1 and 2, destinationChild should be 3. In this
// case, the new index for the source row i (which is between sourceFirst and sourceLast) is
// equal to (destinationChild-sourceLast-1+i).

// Note that if sourceParent and destinationParent are the same, you must ensure that the
// destinationChild is not within the range of sourceFirst and sourceLast + 1. You must also
// ensure that you do not attempt to move a row to one of its own children or ancestors. This
// method returns false if either condition is true, in which case you should abort your move
// operation.

bool GroupModel::move(int src, int /*count*/, int dst) {
    // spdlog::warn("src {} count {} dst {}", src, count, dst);
    if (not controller_ || src == dst || src >= data_.size() || dst > data_.size())
        return false;

    QUuid cdst = QUuid();

    if (auto session = dynamic_cast<SessionUI *>(controller_)) {
        if (dst < data_.size())
            cdst = QUuidFromUuid(session->cuuid(data_[dst]));

        return session->moveContainer(QUuidFromUuid(session->cuuid(data_[src])), cdst);
    } else if (auto playlist = dynamic_cast<PlaylistUI *>(controller_)) {
        if (dst < data_.size())
            cdst = QUuidFromUuid(playlist->cuuid(data_[dst]));

        return playlist->moveContainer(QUuidFromUuid(playlist->cuuid(data_[src])), cdst);
    }

    return false;
}

bool GroupModel::populate(QList<QObject *> &items, QObject *controller) {
    // build change list
    bool different = true;
    bool changed   = false;
    if (controller)
        controller_ = controller;

    // move/delete/insert..

    // delete
    // items only appear once in list..
    while (different) {
        different = false;
        for (auto i = 0; i < data_.size(); i++) {
            if (not items.contains(data_[i])) {
                beginRemoveRows(QModelIndex(), i, i);
                data_.removeAt(i);
                endRemoveRows();
                different = true;
                changed   = true;
                break;
            }
        }
    }

    // move // insert.
    different = true;
    while (different) {
        different = false;
        for (auto i = 0; i < items.size(); i++) {
            // must be new item..
            if (data_.size() <= i) {
                changed   = true;
                different = true;
                beginInsertRows(QModelIndex(), i, i);
                data_.push_back(items[i]);
                endInsertRows();
            } else {
                //  check for difference.
                if (items[i] != data_[i]) {
                    changed   = true;
                    different = true;
                    // could be move ?
                    if (data_.contains(items[i])) {
                        // it's a move..
                        auto src = data_.indexOf(items[i]);
                        beginMoveRows(QModelIndex(), src, src, QModelIndex(), i);
                        data_.move(src, i);
                        endMoveRows();
                    } else {
                        beginInsertRows(QModelIndex(), i, i);
                        data_.insert(i, items[i]);
                        endInsertRows();
                    }
                }
            }
        }
    }
    return changed;
}

ContainerGroupUI::ContainerGroupUI(QObject *parent) : QObject(parent) {}

ContainerGroupUI::ContainerGroupUI(
    const utility::Uuid cuuid,
    const std::string name,
    const std::string type,
    const std::string flag,
    const utility::Uuid uuid,
    QObject *parent)
    : QObject(parent),
      cuuid_(std::move(cuuid)),
      name_(std::move(name)),
      type_(std::move(type)),
      flag_(std::move(flag)),
      uuid_(std::move(uuid)) {}

ContainerDividerUI::ContainerDividerUI(QObject *parent) : QObject(parent) {}

ContainerDividerUI::ContainerDividerUI(
    const utility::Uuid cuuid,
    const std::string name,
    const std::string type,
    const std::string flag,
    const utility::Uuid uuid,
    QObject *parent)
    : QObject(parent),
      cuuid_(std::move(cuuid)),
      name_(std::move(name)),
      type_(std::move(type)),
      flag_(std::move(flag)),
      uuid_(std::move(uuid)) {}

QObject *ContainerGroupUI::findItem(const utility::Uuid &uuid) const {
    QObject *found = nullptr;

    for (auto i = 0; i < items_.count() && not found; i++) {
        if (auto obj = dynamic_cast<const PlaylistUI *>(items_[i])) {
            if (obj->uuid() == uuid or obj->cuuid() == uuid)
                found = items_[i];
        } else if (auto obj = dynamic_cast<const ContainerDividerUI *>(items_[i])) {
            if (obj->uuid() == uuid or obj->cuuid() == uuid)
                found = items_[i];
        }
    }
    return found;
}

SessionUI::SessionUI(QObject *parent)
    : QMLActor(parent),
      backend_(),
      name_("unknown")

{
    init(CafSystemObject::get_actor_system());
}

SessionUI::~SessionUI() {
    if (dummy_playlist_) {
        anon_send_exit(dummy_playlist_, caf::exit_reason::user_shutdown);
        dummy_playlist_ = caf::actor();
    }
}

// helper ?

void SessionUI::set_backend(caf::actor backend) {
    backend_         = backend;
    playlist_        = nullptr;
    selected_source_ = nullptr;
    emit playlistChanged();
    // get backend state..
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


    try {
        backend_events_ = request_receive<caf::actor>(*sys, backend_, get_event_group_atom_v);
        request_receive<bool>(
            *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    try {
        auto nm     = request_receive<std::string>(*sys, backend_, utility::name_atom_v);
        name_       = QStringFromStd(nm);
        media_rate_ = request_receive<FrameRate>(*sys, backend_, session::media_rate_atom_v);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    try {
        auto pt = request_receive<std::pair<caf::uri, fs::file_time_type>>(
            *sys, backend_, session::path_atom_v);
        path_               = pt.first;
        session_file_mtime_ = pt.second;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (backend_) {
        session_actor_addr_ = actorToQString(QMLActor::system(), backend_);
        emit sessionActorAddrChanged();
    }

    emit mediaRateChanged(media_rate_);
    emit nameChanged();
    emit pathChanged();
    updateItemModel(false, true);
    emit itemModelChanged();

    if (items_.empty() && !dummy_playlist_) {

        // Because the toolbar is built dynamically based on attributes belonging
        // to the playhead, viewport and colour pipeline, we need a playhead
        // to exist so that the toolbar fills out even when there is no playlist
        scoped_actor sys{system()};
        dummy_playlist_ = sys->spawn<playlist::PlaylistActor>(
            std::string("Dummy2"), utility::Uuid(), backend_);
        auto playhead = request_receive_wait<utility::UuidActor>(
            *sys, dummy_playlist_, infinite, playlist::create_playhead_atom_v);

        anon_send(playhead.actor(), module::connect_to_ui_atom_v);
    }
    emit backendChanged();
}

void SessionUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void SessionUI::init(actor_system &system_) {
    QMLActor::init(system_);

    // It's alive!

    spdlog::debug("SessionUI init");

    {
        scoped_actor sys{system()};
        try {
            auto grp = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(studio_registry),
                utility::get_event_group_atom_v);
            request_receive<bool>(*sys, grp, broadcast::join_broadcast_atom_v, as_actor());

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    // make sure the audio output is connected to the UI (i.e. it is broacasting and receiveing
    // events to do with its Module attributes)
    auto audio_output_actor =
        system().registry().template get<caf::actor>(audio_output_registry);
    if (audio_output_actor)
        anon_send(audio_output_actor, module::connect_to_ui_atom_v);


    {
        scoped_actor sys{system()};
        auto prefs = global_store::GlobalStoreHelper(system());
        utility::JsonStore js;
        request_receive<bool>(
            *sys, prefs.get_group(js), broadcast::join_broadcast_atom_v, as_actor());
    }


    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {

            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {},

            [=](json_store::update_atom, const utility::JsonStore &js) {},

            [=](utility::event_atom, utility::name_atom, const std::string &name) {
                if (name_ != QString::fromStdString(name)) {
                    name_ = QString::fromStdString(name);
                    emit nameChanged();
                }
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {},

            [=](utility::event_atom, playlist::add_media_atom, const UuidActor &ua) {
                // not sure we recieve these.. as we listen to session events not playlist
                // ones.
                emit mediaAdded(QUuidFromUuid(ua.uuid()));
            },

            [=](utility::event_atom, playlist::create_divider_atom, const Uuid & /*uuid*/) {
                updateItemModel();
                emit itemModelChanged();
                // emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
            },

            [=](utility::event_atom, playlist::create_group_atom, const Uuid & /*uuid*/) {
                updateItemModel();
                emit itemModelChanged();
                // emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
            },

            [=](utility::event_atom,
                session::path_atom,
                const std::pair<caf::uri, fs::file_time_type> &pt) {
                path_               = pt.first;
                session_file_mtime_ = pt.second;
                emit pathChanged();
                emit sessionFileMTimechanged();
            },

            [=](utility::event_atom, playlist::loading_media_atom, const bool) {},

            [=](utility::event_atom, playlist::loading_media_atom, const bool) {},

            [=](utility::event_atom, session::current_playlist_atom, caf::actor playlist) {
                if (!playlist_ || playlist_->backend() != playlist) {

                    if (playlist) {

                        scoped_actor sys{system()};
                        auto uuid = request_receive_wait<utility::Uuid>(
                            *sys, playlist, std::chrono::seconds(1), utility::uuid_atom_v);
                        switchOnScreenSource(QUuidFromUuid(uuid), false);

                    } else {
                        switchOnScreenSource();
                    }

                    updateItemModel(false, true);
                }
            },

            [=](utility::event_atom,
                playlist::move_container_atom,
                const Uuid & /*src*/,
                const Uuid & /*before*/) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const Uuid & /*uuid*/,
                const std::string &) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::remove_container_atom,
                const std::vector<Uuid> &) {},

            [=](utility::event_atom, playlist::remove_container_atom, const Uuid & /*uuid*/) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom, playlist::remove_container_atom, const Uuid &uuid) {
                emit playlistRemoved(QVariant::fromValue(QUuidFromUuid(uuid)));
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom,
                playlist::rename_container_atom,
                const Uuid & /*uuid*/,
                const std::string &) {
                updateItemModel();
                emit itemModelChanged();
            },

            [=](utility::event_atom, session::add_playlist_atom, const UuidActor & /*ua*/) {
                updateItemModel();
                emit itemModelChanged();
                // ua.first must exist in our tree..
                // or not
                // auto plu = dynamic_cast<const PlaylistUI *>(findItem(ua.first));
                // if (plu) {
                //     emit newItem(QVariant::fromValue(plu->qcuuid()));
                // }
            },

            [=](utility::event_atom, session::media_rate_atom, const FrameRate &rate) {
                if (rate != media_rate_) {
                    media_rate_ = rate;
                    emit mediaRateChanged(media_rate_.to_fps());
                }
            },

            [=](utility::event_atom, session::session_atom, caf::actor session) {
                if (backend_ != session)
                    set_backend(session);
            },

            [=](utility::event_atom,
                session::session_request_atom,
                const std::string path,
                const JsonStore &js) {
                emit sessionRequest(
                    QUrlFromUri(posix_path_to_uri(path)), QByteArray(js.dump().c_str()));

                // scoped_actor sys{system()};
                // auto session = sys->spawn<session::SessionActor>(js);
                // auto global = system().registry().template get<caf::actor>(global_registry);

                // sys->anon_send(
                //     global,
                //     session::session_atom_v,
                //     session
                // );

                // setModified(false, true);
                // setPath(QStringFromStd(path));
            },

            [=](utility::event_atom, utility::change_atom) {
                // something changed in the playhead...
                // use this for media changes, which impact timeline
                // update_on_change();
            },

            [=](utility::event_atom,
                utility::last_changed_atom,
                const time_point &last_changed) {
                last_changed_ = last_changed;
                emit modifiedChanged();
            }};
    });


    // is this ever reached ?
    try {
        scoped_actor sys{system()};
        // Get the session ...
        auto global = system().registry().template get<caf::actor>(global_registry);

        auto session = request_receive_wait<caf::actor>(
            *sys, global, std::chrono::seconds(1), session::session_atom_v);
        if (backend_ != session)
            set_backend(session);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

QList<QObject *> SessionUI::getPlaylists() {
    QList<QObject *> result;

    for (auto i : items_) {
        if (dynamic_cast<PlaylistUI *>(i))
            result.append(i);
    }

    return result;
}


void SessionUI::setSelection(const QVariantList &cuuids, const bool clear) {

    std::set<QUuid> uuids;

    for (auto &i : cuuids) {
        uuids.insert(i.toUuid());
    }
    QMap<QUuid, QObject *>::iterator i = items_.begin();

    auto last_selected = selected_source_;
    selected_source_   = nullptr;

    while (i != items_.end()) {
        if (uuids.count(i.key())) {
            setSelection(i.value(), true, clear);
        } else if (clear) {
            setSelection(i.value(), false, clear);
        }
        if (auto playlist = dynamic_cast<PlaylistUI *>(i.value())) {
            for (auto uuid : uuids) {
                QUuid unused;
                if (playlist->quuid() == uuid) {

                    setSelection(i.value(), true, clear);
                    bool oldState    = playlist->blockSignals(true);
                    selected_source_ = playlist;
                    playlist->setSelection(QVariantList());
                    playlist->blockSignals(oldState);

                } else if (playlist->contains(uuid, unused)) {
                    bool oldState = playlist->blockSignals(true);
                    playlist->setSelection(QVariantList({unused}));
                    playlist->blockSignals(oldState);
                    selected_source_ = playlist->selectedSubitem();
                }
            }
        }
        ++i;
    }

    if (last_selected != selected_source_) {
        emit selectedSourceChanged();
    }
}


bool SessionUI::isSelected(QObject *obj) const {

    if (auto item = dynamic_cast<PlaylistUI *>(obj)) {
        return item->selected();
    } else if (auto item = dynamic_cast<TimelineUI *>(obj)) {
        return item->selected();
    } else if (auto item = dynamic_cast<SubsetUI *>(obj)) {
        return item->selected();
    } else if (auto item = dynamic_cast<ContactSheetUI *>(obj)) {
        return item->selected();
    } else if (auto item = dynamic_cast<ContainerDividerUI *>(obj)) {
        return item->selected();
    } else if (auto item = dynamic_cast<ContainerGroupUI *>(obj)) {
        return item->selected();
    }

    return false;
}

void SessionUI::setSelection(QObject *obj, const bool selected, const bool clear) {

    if (auto item = dynamic_cast<PlaylistUI *>(obj)) {
        item->setSelected(selected);
        if (clear) {
            // stop signal loop
            bool oldState = item->blockSignals(true);
            item->setSelection(QVariantList());
            item->blockSignals(oldState);
        }
        if (selected) {
            selected_source_ = item;
        }
    } else if (auto item = dynamic_cast<TimelineUI *>(obj)) {
        item->setSelected(selected);
    } else if (auto item = dynamic_cast<SubsetUI *>(obj)) {
        item->setSelected(selected);
    } else if (auto item = dynamic_cast<ContactSheetUI *>(obj)) {
        item->setSelected(selected);
    } else if (auto item = dynamic_cast<ContainerDividerUI *>(obj)) {
        item->setSelected(selected);
    } else if (auto item = dynamic_cast<ContainerGroupUI *>(obj)) {
        item->setSelected(selected);
    }
}

QObject *SessionUI::getPlaylist(const QUuid &uuid) {
    QObject *rt = nullptr;
    for (auto i : items_) {
        if (auto item = dynamic_cast<PlaylistUI *>(i)) {

            if (item->quuid() == uuid || item->qcuuid() == uuid) {

                rt = static_cast<QObject *>(item);
                break;
            }
        }
    }
    return rt;
}

void SessionUI::setMediaRate(const double rate) {
    if ((1.0 / rate) != media_rate_.to_fps())
        anon_send(backend_, session::media_rate_atom_v, FrameRate(1.0 / rate));
}


// find item based off container uuid or actor uuid
QObject *SessionUI::findItem(const utility::Uuid &cuuid) const {
    QUuid qu = QUuidFromUuid(cuuid);

    auto f = items_.find(qu);
    if (f != items_.end())
        return *f;

    for (auto &i : items_) {
        if (uuid(i) == cuuid) {
            return i;
        } else if (auto item = dynamic_cast<ContainerGroupUI *>(i)) {
            QObject *found = item->findItem(cuuid);
            if (found)
                return found;
        }
    }
    return nullptr;
}

// find item based off container uuid or actor uuid
QObject *SessionUI::getNextItem(const utility::Uuid &cuuid) {
    scoped_actor sys{system()};
    auto rec_items_container =
        request_receive<utility::PlaylistTree>(*sys, backend_, playlist::get_container_atom_v);
    auto next_item = rec_items_container.find_next_at_same_depth(cuuid);
    if (next_item)
        return findItem(*next_item);

    return nullptr;
}

utility::Uuid SessionUI::uuid(const QObject *obj) const {
    if (auto item = dynamic_cast<const PlaylistUI *>(obj)) {
        return item->uuid();
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

utility::Uuid SessionUI::cuuid(const QObject *obj) const {
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

bool SessionUI::moveContainer(const QUuid &uuid, const QUuid &before_uuid, const bool into) {
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

bool SessionUI::renameContainer(const QString &name, const QUuid &uuid) {
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

bool SessionUI::reflagContainer(const QString &flag, const QUuid &uuid) {
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

QUuid SessionUI::createDivider(const QString &name, const QUuid before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_playlist = request_receive<Uuid>(
            *sys,
            backend_,
            playlist::create_divider_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_playlist);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


QUuid SessionUI::createPlaylist(const QString &name, const QUuid before, const bool into) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_playlist = request_receive<UuidUuidActor>(
            *sys,
            backend_,
            session::add_playlist_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before),
            into);
        result = QUuidFromUuid(new_playlist.second.uuid());
        updateItemModel();
        emit itemModelChanged();
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid SessionUI::createGroup(const QString &name, const QUuid before) {
    QUuid result;
    try {
        scoped_actor sys{system()};
        auto new_playlist = request_receive<Uuid>(
            *sys,
            backend_,
            playlist::create_group_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before));
        result = QUuidFromUuid(new_playlist);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid SessionUI::getNextItemUuid(const QUuid &quuid) {
    QUuid result;
    auto obj = getNextItem(UuidFromQUuid(quuid));
    if (obj)
        result = QUuidFromUuid(uuid(obj));

    return result;
}

bool SessionUI::removeContainer(const QUuid &uuid) {

    // are we trying to remove a playlist that is currently selected and (possibly)
    // the on screen source?
    if (selected_source_ && cuuid(selected_source_) == UuidFromQUuid(uuid)) {

        bool onscreen_is_selected = on_screen_source_ == selected_source_;
        // we are about to remove the selected item - therefore we need to try and select
        // the next object
        QObject *next = getNextItem(UuidFromQUuid(uuid));
        if (next) {
            setSelection(next);
            emit selectedSourceChanged();
            if (onscreen_is_selected) {
                switchOnScreenSource(QUuidFromUuid(cuuid(selected_source_)));
            }
        }
    }

    bool result = false;
    try {
        scoped_actor sys{system()};
        auto new_item = request_receive<bool>(
            *sys, backend_, playlist::remove_container_atom_v, UuidFromQUuid(uuid));
        result = new_item;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}


QUuid SessionUI::duplicateContainer(
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

void SessionUI::updateItemModel(const bool select_new_items, const bool reset) {
    // spdlog::stopwatch sw;
   try {
        scoped_actor sys{system()};
        std::map<Uuid, caf::actor> uuid_actor;
        std::map<Uuid, QObject *> hold;
        items_list_.clear();

        auto rec_items_container = request_receive<utility::PlaylistTree>(
            *sys, backend_, playlist::get_container_atom_v);

        if (not reset && last_changed_tree_ == rec_items_container)
            return;

        if (reset) {
            items_.clear();
            items_list_.clear();
            on_screen_source_ = nullptr;
        }

        last_changed_tree_ = rec_items_container;

        // build look up for actors
        // based on object UUID
        for (const auto &i : request_receive<std::vector<UuidActor>>(
                 *sys, backend_, session::get_playlists_atom_v)) {
            uuid_actor[i.uuid()] = i.actor();
        }

        // decompose..
        // all QT objects now in hold.
        // we store base on object uuid NOT container UUID
        // we also clear dependencies.

        while (!items_.empty()) {
            if (auto item = dynamic_cast<const PlaylistUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<ContainerDividerUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
            } else if (auto item = dynamic_cast<ContainerGroupUI *>(items_.begin().value())) {
                hold[item->uuid()] = items_.begin().value();
                while (not item->items_.empty()) {
                    if (auto iitem = dynamic_cast<ContainerDividerUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    } else if (auto iitem = dynamic_cast<PlaylistUI *>(item->items_[0])) {
                        hold[iitem->uuid()] = item->items_[0];
                    }
                    item->items_.removeAt(0);
                }
            }
            items_.erase(items_.begin());
        }

        bool new_objects = false;

        bool last_selected_still_valid = false;

        // Our QList is a collection of (playlists and playlist groups)
        // we use uuids as the index, as names may change..
        // playlist groups are only at the top level.
        for (auto &i : rec_items_container.children_ref()) {
            // is item in hold?
            auto qcu  = QUuidFromUuid(i.uuid());
            auto uuid = i.value().uuid();
            auto type = i.value().type();

            if (hold.count(uuid)) {
                if (type == "ContainerGroup") {
                    auto obj = dynamic_cast<ContainerGroupUI *>(hold[uuid]);
                    obj->setName(QStringFromStd(i.value().name()));
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "ContainerDivider") {
                    auto obj = dynamic_cast<ContainerDividerUI *>(hold[uuid]);
                    obj->setName(QStringFromStd(i.value().name()));
                    obj->setFlag(QStringFromStd(i.value().flag()));
                } else if (type == "Playlist") {
                    auto obj = dynamic_cast<PlaylistUI *>(hold[uuid]);
                    obj->setFlag(QStringFromStd(i.value().flag()));
                    if (obj == selected_source_) {
                        last_selected_still_valid = true;
                    }
                }
                items_list_.append(hold[uuid]);
                items_[qcu] = hold[uuid];

            } else {
                // new item
                if (type == "ContainerGroup") {
                    new_objects = true;
                    auto obj    = new ContainerGroupUI(
                        i.uuid(), i.value().name(), type, i.value().flag(), uuid, this);
                    if (select_new_items)
                        obj->setSelected();
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));

                } else if (type == "ContainerDivider") {
                    auto obj = new ContainerDividerUI(
                        i.uuid(), i.value().name(), type, i.value().flag(), uuid, this);
                    if (select_new_items and not new_objects)
                        obj->setSelected();
                    new_objects = true;
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
                } else if (type == "Playlist") {
                    auto obj = new PlaylistUI(i.uuid(), uuid, this);
                    QObject::connect(
                        obj,
                        SIGNAL(newSelection(const QVariantList, const bool)),
                        this,
                        SLOT(setSelection(const QVariantList &, const bool)));
                    QObject::connect(
                        obj, SIGNAL(nameChanged()), this, SLOT(rebuildPlaylistNamesList()));
                    if (select_new_items and not new_objects)
                        obj->setSelected();
                    new_objects = true;
                    items_[qcu] = obj;
                    items_list_.append(obj);
                    obj->init(system());
                    // quick set backend
                    obj->set_backend(uuid_actor[uuid], true);

                    // we need this to complete..
                    anon_send(obj->as_actor(), backend_atom_v, uuid_actor[uuid]);

                    obj->setFlag(QStringFromStd(i.value().flag()));
                    emit newItem(QVariant::fromValue(QUuidFromUuid(uuid)));
                }
            }

            if (type == "ContainerGroup") {
                auto new_group = dynamic_cast<ContainerGroupUI *>(items_[qcu]);
                // as it's a groups we need to scan it..
                for (const auto &ii : i.children_ref()) {
                    auto chtype = ii.value().type();
                    auto chuuid = ii.value().uuid();

                    if (hold.count(chuuid)) {
                        if (chtype == "ContainerGroup") {
                            auto obj = dynamic_cast<ContainerGroupUI *>(hold[chuuid]);
                            obj->setName(QStringFromStd(ii.value().name()));
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        } else if (chtype == "ContainerDivider") {
                            auto obj = dynamic_cast<ContainerDividerUI *>(hold[chuuid]);
                            obj->setName(QStringFromStd(ii.value().name()));
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        } else if (chtype == "Playlist") {
                            auto obj = dynamic_cast<PlaylistUI *>(hold[chuuid]);
                            obj->setFlag(QStringFromStd(ii.value().flag()));
                        }
                        new_group->items_.append(hold[chuuid]);
                    } else if (chtype == "ContainerDivider") {
                        auto obj = new ContainerDividerUI(
                            ii.uuid(),
                            ii.value().name(),
                            ii.value().type(),
                            ii.value().flag(),
                            ii.value().uuid(),
                            this);
                        if (select_new_items and not new_objects)
                            obj->setSelected();
                        new_objects = true;
                        new_group->items_.append(obj);
                        emit newItem(QVariant::fromValue(QUuidFromUuid(ii.value().uuid())));
                    } else if (chtype == "Playlist") {
                        // new playlist
                        auto obj = new PlaylistUI(ii.uuid(), chuuid, this);
                        QObject::connect(
                            obj, SIGNAL(nameChanged()), this, SLOT(rebuildPlaylistNamesList()));
                        if ((select_new_items or dummy_playlist_) and not new_objects)
                            obj->setSelected();
                        new_objects = true;
                        new_group->items_.append(obj);

                        obj->init(system());
                        anon_send(obj->as_actor(), backend_atom_v, uuid_actor[chuuid]);

                        obj->setFlag(QStringFromStd(ii.value().flag()));
                        emit newItem(QVariant::fromValue(QUuidFromUuid(ii.value().uuid())));
                    }
                }
                new_group->changedItems();
            }
        }

        if (!last_selected_still_valid) {
            selected_source_ = nullptr;
            emit selectedSourceChanged();
        }

        // deselect old objects.
        if (new_objects and select_new_items) {
            for (auto i : hold) {
                if (auto item = dynamic_cast<PlaylistUI *>(i.second)) {
                    item->setSelected(false);
                } else if (auto item = dynamic_cast<TimelineUI *>(i.second)) {
                    item->setSelected(false);
                } else if (auto item = dynamic_cast<SubsetUI *>(i.second)) {
                    item->setSelected(false);
                } else if (auto item = dynamic_cast<ContactSheetUI *>(i.second)) {
                    item->setSelected(false);
                } else if (auto item = dynamic_cast<ContainerDividerUI *>(i.second)) {
                    item->setSelected(false);
                } else if (auto item = dynamic_cast<ContainerGroupUI *>(i.second)) {
                    item->setSelected(false);
                }
            }
        }

        if (model_.populate(items_list_, this))
            emit itemModelChanged();

        if ((on_screen_source_ && !items_list_.contains(on_screen_source_)) ||
            !on_screen_source_) {
            // the current selected source is not in our list (it may have been removed) or
            // there is no on screen source, so try and select
            switchOnScreenSource();
        }

        rebuildPlaylistNamesList();

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    // spdlog::error("session update {:.3}", sw);
}

bool SessionUI::loadUris(const std::vector<caf::uri> &uris, const bool single_playlist) {

    scoped_actor sys{system()};
    // check for session files ?
    // scan for single session file.. as we use difference behaviour.
    if (uris.size() == 1 and ends_with(to_lower(uri_to_posix_path(uris[0])), ".xst")) {
        try {
            auto path   = uri_to_posix_path(uris[0]);
            auto global = system().registry().template get<caf::actor>(global_registry);
            JsonStore js;
            std::ifstream i(path);
            i >> js;

            if (not model_.empty()) {
                return request_receive<bool>(
                    *sys, global, session::session_request_atom_v, path, js);
            } else {
                auto session = sys->spawn<session::SessionActor>(js, uris[0]);
                sys->anon_send(global, session::session_atom_v, session);
                set_backend(session);
                setModified(false, true);
                setPath(QUrlFromUri(uris[0]));
                return true;
            }
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            return false;
        }
    } else
        sys->anon_send(backend_, session::load_uris_atom_v, uris, single_playlist);

    return true;
}

bool SessionUI::loadUrl(const QUrl &url, const bool single_playlist) {
    // bool result=true;
    std::vector<caf::uri> paths;
    paths.emplace_back(UriFromQUrl(url));
    return loadUris(paths, single_playlist);
}

bool SessionUI::loadUrls(const QList<QUrl> &urls, const bool single_playlist) {
    // bool result=true;

    std::vector<caf::uri> paths;

    for (const auto &i : urls) {
        paths.emplace_back(UriFromQUrl(i));
    }

    return loadUris(paths, single_playlist);
}

void SessionUI::rebuildPlaylistNamesList() {

    scoped_actor sys{system()};

    auto rec_items_container =
        request_receive<utility::PlaylistTree>(*sys, backend_, playlist::get_container_atom_v);

    QVariantList rt;

    for (auto &i : rec_items_container.children_ref()) {

        auto qcu  = QUuidFromUuid(i.uuid());
        auto type = i.value().type();

        if (items_.count(qcu) && type == "Playlist") {
            auto item = dynamic_cast<PlaylistUI *>(items_[qcu]);
            if (item) {
                QMap<QString, QVariant> m;
                m["text"] = QVariant(item->name());
                m["uuid"] = QVariant(item->quuid());
                rt.append(m);
            }
        }
    }

    playlistNames_ = QVariant(rt);
    emit(playlistNamesChanged());
}

void SessionUI::setName(const QString &name) {
    if (name != name_) {
        if (backend_) {
            scoped_actor sys{system()};
            sys->anon_send(backend_, utility::name_atom_v, name.toStdString());
        }
    }
}

QString SessionUI::save() {
    if (not path().path().isEmpty()) {
        return save(path());
    }
    return QString("Session has not been saved yet, use SaveAs instead.");
}

void SessionUI::setSessionActorAddr(const QString &addr) {
    auto actor = actorFromQString(system(), addr);
    if (actor) {
        set_backend(actor);
        setModified(false);
        setPath(QUrl());
        switchOnScreenSource(QUuid());
        setViewerPlayhead();
    }
}


void SessionUI::newSession(const QString &name) {

    scoped_actor sys{system()};

    auto session = sys->spawn<session::SessionActor>(StdFromQString(name));
    set_backend(session);

    auto global = system().registry().template get<caf::actor>(global_registry);
    sys->anon_send(global, session::session_atom_v, session);

    setModified(false);
    setPath(QUrl());
    switchOnScreenSource(QUuid());
    setViewerPlayhead();
}

QString SessionUI::save(const QUrl &path) {
    QString result;
    // maybe done by backend?
    try {
        scoped_actor sys{system()};
        // Get the session ...
        if (request_receive_wait<size_t>(
                *sys,
                backend_,
                std::chrono::seconds(60),
                global_store::save_atom_v,
                UriFromQUrl(path))) {
            setModified(false);
            setPath(path);
        }
    } catch (std::exception &err) {
        std::string error = "Session save to " + StdFromQString(path.path()) + " failed!\n";

        if (std::string(err.what()) == "caf::sec::request_timeout") {
            error += "Backend did not return session data within timeout.";
        } else {
            auto errstr = std::string(err.what());
            errstr      = std::regex_replace(
                errstr, std::regex(R"foo(.*caf::sec::runtime_error\("([^"]+)"\).*)foo"), "$1");
            error += errstr;
        }

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, error);
        result = QStringFromStd(error);
    }

    return result;
}

QString SessionUI::save_selected(const QUrl &path) {
    QString result;

    try {
        scoped_actor sys{system()};
        // Get the session ...
        // build selected item list.
        std::vector<Uuid> selected;
        for (const auto &i : items_) {
            if (isSelected(i))
                selected.push_back(cuuid(i));
        }

        request_receive_wait<size_t>(
            *sys,
            backend_,
            std::chrono::seconds(60),
            global_store::save_atom_v,
            UriFromQUrl(path),
            selected);

    } catch (std::exception &err) {
        std::string error =
            "Selected session save to " + StdFromQString(path.path()) + " failed!\n";

        if (std::string(err.what()) == "caf::sec::request_timeout") {
            error += "Backend did not return session data within timeout.";
        } else {
            auto errstr = std::string(err.what());
            errstr      = std::regex_replace(
                errstr, std::regex(R"foo(.*caf::sec::runtime_error\("([^"]+)"\).*)foo"), "$1");
            error += errstr;
        }

        spdlog::warn("{} {}", __PRETTY_FUNCTION__, error);
        result = QStringFromStd(error);
    }

    return result;
}

void SessionUI::setPath(const QUrl &path, const bool inform_backend) {
    auto newpath = UriFromQUrl(path);
    if (newpath != path_) {
        path_ = newpath;
        if (backend_ and inform_backend) {
            scoped_actor sys{system()};
            sys->anon_send(backend_, session::path_atom_v, path_);
        }

        emit pathChanged();
    }
}

bool SessionUI::load(const QUrl &path, const QVariant &json) {
    bool result = false;
    try {
        scoped_actor sys{system()};

        JsonStore js(qvariant_to_json(json));
        auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));

        auto global = system().registry().template get<caf::actor>(global_registry);
        sys->anon_send(global, session::session_atom_v, session);

        set_backend(session);


        setModified(false, true);
        setPath(path, false);
        result = true;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

bool SessionUI::import(const QUrl &path, const QVariant &json) {

    bool result = false;
    JsonStore js;
    scoped_actor sys{system()};
    if (json.isNull()) {
        try {
            std::ifstream i(StdFromQString(path.path()));
            i >> js;
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            return false;
        }
    } else {
        try {
            js = JsonStore(qvariant_to_json(json));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            return false;
        }
    }

    try {
        auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));
        sys->anon_send(backend_, session::merge_session_atom_v, session);

        result = true;
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool SessionUI::load(const QUrl &path) {

    scoped_actor sys{system()};

    bool result = false;

    try {
        JsonStore js;
        std::ifstream i(StdFromQString(path.path()));
        i >> js;

        auto session = sys->spawn<session::SessionActor>(js, UriFromQUrl(path));

        // destroy old session and point to new session.
        auto global = system().registry().template get<caf::actor>(global_registry);
        sys->anon_send(global, session::session_atom_v, session);

        set_backend(session);

        setModified(false, true);
        setPath(path);
        result = true;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QObject *SessionUI::playlist() { return static_cast<QObject *>(playlist_); }

QObject *SessionUI::selectedSource() { return selected_source_; }


void SessionUI::switchOnScreenSource(const QUuid &uuid, const bool broadcast) {

    // This can be called at start-up before a playlist uuid is set-up
    spdlog::debug("switchOnScreenSource {}", to_string(UuidFromQUuid(uuid)));
    // to_string(UuidFromQUuid(suuid)));
    QObject *prev_source      = on_screen_source_;
    on_screen_source_         = nullptr;
    PlaylistUI *prev_playlist = playlist_;
    playlist_                 = nullptr;
    // null and no current playlist - pick the first playlist that we have
    if (uuid.isNull()) {
        if (not playlist_ and not items_.empty()) {
            for (auto i : items_) {
                if (auto item = dynamic_cast<PlaylistUI *>(i)) {
                    playlist_ = item;
                    anon_send(
                        backend_,
                        session::current_playlist_atom_v,
                        playlist_->backend(),
                        broadcast);
                    on_screen_source_ = playlist_;
                    emit playlistChanged();
                    setSelection(QVariantList({QVariant(item->qcuuid())}), true);
                    break;
                }
            }
        } else {
            emit playlistChanged();
            emit onScreenSourceChanged();
        }
    } else {

        QUuid cuuid;
        for (auto i : items_) {
            if (auto item = dynamic_cast<PlaylistUI *>(i)) {
                if (item->quuid() == uuid || item->qcuuid() == uuid) {
                    playlist_ = item;
                    anon_send(
                        backend_,
                        session::current_playlist_atom_v,
                        playlist_->backend(),
                        broadcast);
                    on_screen_source_ = playlist_;
                    emit playlistChanged();
                    break;
                } else if (item->contains(uuid, cuuid)) {
                    playlist_ = item;
                    emit playlistChanged();
                    playlist_->setSelection(QVariantList({QVariant(cuuid)}), true);
                    on_screen_source_ = playlist_->selectedSubitem();
                }
            }
        }
    }

    if (on_screen_source_ != prev_source) {
        emit onScreenSourceChanged();
        setViewerPlayhead();
    }

    if (playlist_ && playlist_ != prev_playlist) {

        // when user has changed playlist, we want to open readers for the media
        // items ready for the user to jump around the playlist
        if (playlist_->backend())
            anon_send(playlist_->backend(), playlist::set_playlist_in_viewer_atom_v, true);
        if (prev_playlist && prev_playlist->backend())
            anon_send(prev_playlist->backend(), playlist::set_playlist_in_viewer_atom_v, false);
    } else if (playlist_ != prev_playlist) {
        emit playlistChanged();
    }
}

void SessionUI::setViewerPlayhead() {


    caf::actor on_screen_item_backend;
    if (auto subset = dynamic_cast<SubsetUI *>(on_screen_source_)) {
        on_screen_item_backend = subset->backend();
    } else if (auto contact_sheet = dynamic_cast<ContactSheetUI *>(on_screen_source_)) {
        on_screen_item_backend = contact_sheet->backend();
    } else if (auto timeline = dynamic_cast<TimelineUI *>(on_screen_source_)) {
        on_screen_item_backend = timeline->backend();
    } else if (auto playlist = dynamic_cast<PlaylistUI *>(on_screen_source_)) {
        on_screen_item_backend = playlist->backend();
    }
    auto ph_events = system().registry().template get<caf::actor>(global_playhead_events_actor);
    if (!on_screen_item_backend) {
        on_screen_item_backend = dummy_playlist_;
    }
    scoped_actor sys{system()};
    try {
        auto playhead = request_receive<UuidActor>(
                            *sys, on_screen_item_backend, playlist::create_playhead_atom_v)
                            .actor();
        anon_send(ph_events, viewport::viewport_playhead_atom_v, playhead);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}


QUuid SessionUI::mergePlaylists(
    const QVariantList &cuuids, const QString &name, const QUuid &before_cuuid, const bool) {
    // convert cuuids into playlist uuids.
    QUuid new_playlist_cuuid;
    UuidVector playlists;

    for (auto &i : cuuids) {
        auto it = items_.find(i.toUuid());
        if (it != items_.end()) {
            PlaylistUI *pl = nullptr;
            if ((pl = dynamic_cast<PlaylistUI *>(*it))) {
                playlists.push_back(pl->uuid());
            }
        }
    }

    scoped_actor sys{system()};
    try {
        auto result = request_receive<utility::UuidUuidActor>(
            *sys,
            backend_,
            session::merge_playlist_atom_v,
            StdFromQString(name),
            UuidFromQUuid(before_cuuid),
            playlists);
        new_playlist_cuuid = QUuidFromUuid(result.first);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return new_playlist_cuuid;
}

QVariantList SessionUI::copyContainers(
    const QVariantList &cuuids, const QUuid &playlist_cuuid, const QUuid &before_cuuid) {
    QVariantList containers;

    UuidVector copy_containers;

    for (auto &i : cuuids)
        copy_containers.push_back(UuidFromQUuid(i.toUuid()));

    scoped_actor sys{system()};
    try {
        auto result = request_receive<UuidVector>(
            *sys,
            backend_,
            playlist::copy_container_to_atom_v,
            UuidFromQUuid(playlist_cuuid),
            copy_containers,
            UuidFromQUuid(before_cuuid),
            false);

        for (auto const &i : result)
            containers.push_back(QVariant(QUuidFromUuid(i)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return containers;
}

QVariantList SessionUI::moveContainers(
    const QVariantList &cuuids,
    const QUuid &playlist_cuuid,
    const bool with_media,
    const QUuid &before_cuuid) {
    QVariantList containers;

    UuidVector copy_containers;

    for (auto &i : cuuids)
        copy_containers.push_back(UuidFromQUuid(i.toUuid()));

    scoped_actor sys{system()};
    try {
        auto result = request_receive<UuidVector>(
            *sys,
            backend_,
            playlist::move_container_to_atom_v,
            UuidFromQUuid(playlist_cuuid),
            copy_containers,
            with_media,
            UuidFromQUuid(before_cuuid),
            false);

        for (auto const &i : result)
            containers.push_back(QVariant(QUuidFromUuid(i)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return containers;
}

QVariantList SessionUI::copyMedia(
    const QUuid &target_cuuid,
    const QVariantList &media_quuids,
    const bool force_duplicate,
    const QUuid &before_cuuid) {

    QVariantList qresult;
    UuidVector media_uuids;

    for (auto &i : media_quuids)
        media_uuids.push_back(UuidFromQUuid(i.toUuid()));

    scoped_actor sys{system()};
    try {
        auto result = request_receive<UuidVector>(
            *sys,
            backend_,
            playlist::copy_media_atom_v,
            UuidFromQUuid(target_cuuid),
            media_uuids,
            force_duplicate,
            UuidFromQUuid(before_cuuid),
            false);

        for (auto const &i : result)
            qresult.push_back(QVariant(QUuidFromUuid(i)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return qresult;
}

QVariantList SessionUI::moveMedia(
    const QUuid &target_cuuid,
    const QUuid &src_cuuid,
    const QVariantList &media_quuids,
    const QUuid &before_cuuid) {

    QVariantList qresult;
    UuidVector media_uuids;

    for (auto &i : media_quuids)
        media_uuids.push_back(UuidFromQUuid(i.toUuid()));

    scoped_actor sys{system()};
    try {
        auto result = request_receive<UuidVector>(
            *sys,
            backend_,
            playlist::move_media_atom_v,
            UuidFromQUuid(target_cuuid),
            UuidFromQUuid(src_cuuid),
            media_uuids,
            UuidFromQUuid(before_cuuid),
            false);

        for (auto const &i : result)
            qresult.push_back(QVariant(QUuidFromUuid(i)));
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return qresult;
}

QFuture<QList<QUuid>> SessionUI::handleDropFuture(const QVariantMap &drop) {
    return QtConcurrent::run([=]() {
        QList<QUuid> results;
        try {
            auto jsn = dropToJsonStore(drop);

            // spdlog::warn("{}", jsn.dump(2));

            if (jsn.count("text/uri-list")) {
                QList<QUrl> urls;
                for (const auto &i : jsn["text/uri-list"])
                    urls.append(QUrl(QStringFromStd(i)));
                loadUrls(urls);
            } else {
                // something else...
                // dispatch to datasource, if that doesn't work then assume file paths..
                scoped_actor sys{system()};
                auto pm = system().registry().template get<caf::actor>(plugin_manager_registry);
                try {
                    auto result = request_receive<UuidActorVector>(
                        *sys, pm, data_source::use_data_atom_v, JsonStore(jsn), true);
                    if (result.empty()) {
                        for (const auto &i : jsn["text/plain"])
                            loadUrl(QUrl(QStringFromStd("file:" + i.get<std::string>())));
                    } else {
                        // we've got a collection of actors..
                        // lets assume they are media... (WARNING this may not be the case...)
                        // create new playlist and add them...

                        auto new_playlist = request_receive<UuidUuidActor>(
                            *sys,
                            backend_,
                            session::add_playlist_atom_v,
                            "DragDrop Playlist",
                            utility::Uuid(),
                            false);
                        for (const auto &i : result) {
                            anon_send(
                                new_playlist.second.actor(),
                                playlist::add_media_atom_v,
                                i,
                                utility::Uuid());
                            results.push_back(QUuidFromUuid(i.uuid()));
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return results;
    });
}
