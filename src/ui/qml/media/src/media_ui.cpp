// SPDX-License-Identifier: Apache-2.0

#include <iostream>


#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/media_ui.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"


CAF_PUSH_WARNINGS
#include <QDebug>
#include <QPainter>
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

MediaModel::MediaModel(QObject *parent) : QAbstractListModel(parent) {}

int MediaModel::get_role(const QString &rolename) const {
    auto role      = -1;
    auto rolenames = roleNames();
    auto i         = rolenames.constBegin();
    while (i != rolenames.constEnd()) {
        if (QString(i.value()) == rolename) {
            role = i.key();
            break;
        }
        ++i;
    }
    return role;
}


QVariant MediaModel::get(const int row, const QString &rolename) const {
    return data(index(row, 0), get_role(rolename));
}

// QModelIndexList QAbstractItemModel::match(const QModelIndex &start, int role, const QVariant
// &value, int hits = 1, Qt::MatchFlags flags =
// Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap))

// Q_INVOKABLE int MediaModel::match(const QVariant &value, const QString &rolename,const int
// start_row) const {
//     auto row = -1;

//     auto indexs = QAbstractListModel::match(index(start_row, 0), get_role(rolename), value,
//     1, Qt::MatchExactly); if(not indexs.isEmpty())
//         row = indexs[0].row();

//     return row;
// }

int MediaModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return data_.count();
}

QVariant MediaModel::data(const QModelIndex &index, int role) const {
    auto row = index.row();
    if (row >= 0 and row < data_.count()) {
        switch (role) {
        case ObjectRole:
            return QVariant::fromValue(data_[row]);
            break;
        case UuidRole:
            if (auto i = dynamic_cast<MediaUI *>(data_[row]))
                return QVariant::fromValue(i->uuid());
            break;
        default:
            break;
        }
    }
    return QVariant();
}

QHash<int, QByteArray> MediaModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ObjectRole] = "media_ui_object";
    roles[UuidRole]   = "uuid";
    return roles;
}

bool MediaModel::populate(QList<QObject *> &items) {
    // build change list
    bool different         = true;
    bool changed           = false;
    const int previousSize = data_.size();
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

    if (data_.size() != previousSize)
        emit lengthChanged();

    return changed;
}


MediaUI::MediaUI(QObject *parent) : QMLActor(parent), backend_(), backend_events_() {}

bool MediaUI::mediaOnline() const {
    if (not media_source_)
        return false;

    return media_source_->mediaOnline();
}

QString MediaUI::mediaStatus() const {
    if (not media_source_)
        return QStringFromStd(to_string(media::MediaStatus::MS_MISSING));

    return media_source_->mediaStatus();
}


void MediaUI::set_backend(caf::actor backend) {
    spdlog::debug("MediaUI set_backend");

    const auto tt = utility::clock::now();
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

    if (!backend_) {
        name_         = "";
        uuid_         = Uuid();
        media_source_ = nullptr;
        emit mediaSourceChanged();
        emit backendChanged();
        emit uuidChanged();
        emit nameChanged();
        emit mediaStatusChanged();
        return;
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

    // find current..
    utility::Uuid current_ms;
    try {
        auto source =
            request_receive<UuidActor>(*sys, backend_, media::current_media_source_atom_v);
        media_source_ = new MediaSourceUI(this);
        media_source_->initSystem(this);
        media_source_->set_backend(source.actor());
        QObject::connect(
            media_source_,
            &MediaSourceUI::mediaStatusChanged,
            this,
            &MediaUI::mediaStatusChanged);
        emit mediaSourceChanged();
        emit mediaStatusChanged();
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    try {
        auto [flag, flag_text] = request_receive<std::tuple<std::string, std::string>>(
            *sys, backend_, playlist::reflag_container_atom_v);
        flag_      = flag;
        flag_text_ = flag_text;
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    update_on_change();
    emit backendChanged();
    emit mediaSourceChanged();
    emit uuidChanged();
    emit nameChanged();
    emit flagChanged();
    emit flagTextChanged();

    spdlog::debug(
        "MediaUI set_backend {} took {} milliseconds to complete",
        to_string(uuid_),
        std::chrono::duration_cast<std::chrono::milliseconds>(utility::clock::now() - tt)
            .count());
}
void MediaUI::setFlag(const QString &_flag) {
    if (_flag != flag()) {
        anon_send(
            backend_,
            playlist::reflag_container_atom_v,
            std::make_tuple(
                std::optional<std::string>(StdFromQString(_flag)),
                std::optional<std::string>{}));
    }
}

void MediaUI::setFlagText(const QString &_flag) {
    if (_flag != flagText())
        anon_send(
            backend_,
            playlist::reflag_container_atom_v,
            std::make_tuple(
                std::optional<std::string>{},
                std::optional<std::string>(StdFromQString(_flag))));
}

void MediaUI::setCurrentSource(const QUuid &quuid) {
    auto uuid = UuidFromQUuid(quuid);
    anon_send(backend_, media::current_media_source_atom_v, uuid);
}

void MediaUI::initSystem(QObject *system_qobject) {
    init(dynamic_cast<QMLActor *>(system_qobject)->system());
}

void MediaUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("MediaUI init");

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

            [=](utility::event_atom,
                media::current_media_source_atom,
                UuidActor &media_source,
                const media::MediaType media_type) {
                // for now, skipping audio media type
                if (media_type != media::MT_IMAGE)
                    return;

                // N.B. QML Garbage collection should delete the
                // old media source object, C++ side does not own it!!
                if (!media_source_) {
                    media_source_ = new MediaSourceUI(this);
                    media_source_->initSystem(this);
                    QObject::connect(
                        media_source_,
                        &MediaSourceUI::mediaStatusChanged,
                        this,
                        &MediaUI::mediaStatusChanged);
                }

                media_source_->set_backend(media_source.actor());

                emit mediaSourceChanged();
                emit mediaStatusChanged();
            },

            [=](utility::event_atom, utility::change_atom) { update_on_change(); },

            [=](utility::event_atom,
                playlist::reflag_container_atom,
                const utility::Uuid &,
                const std::tuple<std::string, std::string> &_flag) {
                if (std::get<0>(_flag) != flag_) {
                    flag_ = std::get<0>(_flag);
                    emit flagChanged();
                }
                if (std::get<1>(_flag) != flag_text_) {
                    flag_text_ = std::get<1>(_flag);
                    emit flagTextChanged();
                }
            },

            [=](utility::event_atom, bookmark::bookmark_change_atom, const utility::Uuid &) {},

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

void MediaUI::update_on_change() {}


QFuture<QString> MediaUI::getMetadataFuture() {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};
                return QStringFromStd(
                    request_receive<JsonStore>(
                        *sys, backend_, json_store::get_json_atom_v, utility::Uuid(), "")
                        .dump());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}