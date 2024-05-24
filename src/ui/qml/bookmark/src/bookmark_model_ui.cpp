// SPDX-License-Identifier: Apache-2.0
// CAF_PUSH_WARNINGS
// CAF_POP_WARNINGS

// #include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/bookmark_model_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/bookmark/bookmark.hpp"
// #include "xstudio/utility/helpers.hpp"
// #include "xstudio/utility/json_store.hpp"
// #include "xstudio/utility/logging.hpp"
// #include "xstudio/global_store/global_store.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
// using namespace xstudio::global_store;

BookmarkCategoryModel::BookmarkCategoryModel(QObject *parent) : JSONTreeModel(parent) {
    setRoleNames(std::vector<std::string>({"textRole", "valueRole", "colourRole"}));
}

void BookmarkCategoryModel::setValue(const QVariant &value) {
    if (value != value_) {
        value_ = value;
        emit valueChanged();

        setModelData(mapFromValue(value_));
    }
}

QVariant BookmarkCategoryModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case JSONTreeModel::Roles::JSONRole:
            result = QVariantMapFromJson((indexToFullData(index)));
            break;

        case Roles::valueRole:
            result = QString::fromStdString(j.at("value"));
            break;

        case Qt::DisplayRole:
        case Roles::textRole:
            if (j.count("text"))
                result = QString::fromStdString(j.at("text"));
            else
                result = QString::fromStdString(j.at("value"));
            break;

        case Roles::colourRole:
            if (j.count("colour"))
                result = QString::fromStdString(j.at("colour"));
            break;
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalId());
    }

    return result;
}

BookmarkFilterModel::BookmarkFilterModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    sort(0, Qt::DescendingOrder);
}

bool BookmarkFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {
    bool result = true;

    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    auto visible      = sourceModel()->data(index, BookmarkModel::Roles::visibleRole).toBool();

    if (not visible)
        return false;

    auto owner = sourceModel()->data(index, BookmarkModel::Roles::ownerRole).toString();

    if (StdFromQString(index.data(BookmarkModel::Roles::startTimecodeRole).toString()) ==
        "--:--:--:--")
        return false;

    switch (depth_) {
    case 3:
        break;
    case 2:
    case 1:
        result = media_order_.contains(owner);
        break;

    case 0:
    default:
        result = (current_media_.toString() == owner);
        break;
    }

    return result;
}

bool BookmarkFilterModel::lessThan(
    const QModelIndex &source_left, const QModelIndex &source_right) const {
    bool result = false;

    auto lor = source_left.data(BookmarkModel::Roles::ownerRole);
    auto ror = source_right.data(BookmarkModel::Roles::ownerRole);

    if (lor == ror) {
        result = source_left.data(BookmarkModel::Roles::startTimecodeRole).toString() >
                 source_right.data(BookmarkModel::Roles::startTimecodeRole).toString();
    } else {
        auto lors = lor.toString();
        auto rors = ror.toString();

        result =
            ((media_order_.contains(lors) ? media_order_[lors].toInt() : -1) >
             (media_order_.contains(rors) ? media_order_[rors].toInt() : -1));
    }

    return result;
}

void BookmarkFilterModel::setMediaOrder(const QVariantMap &mo) {
    if (mo != media_order_) {
        media_order_ = mo;
        emit mediaOrderChanged();
        invalidateFilter();
    }
}

void BookmarkFilterModel::setDepth(const int value) {
    if (value != depth_) {
        depth_ = value;
        emit depthChanged();
        invalidateFilter();
    }
}

void BookmarkFilterModel::setCurrentMedia(const QVariant &value) {
    auto uuid = value.toUuid();
    if (uuid != current_media_) {
        current_media_ = uuid;
        emit currentMediaChanged();
        invalidateFilter();
    }
}

BookmarkModel::BookmarkModel(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    setRoleNames(std::vector<std::string>(
        {"enabledRole",
         "focusRole",
         "frameRole",
         "frameFromTimecodeRole",
         "startTimecodeRole",
         "endTimecodeRole",
         "durationTimecodeRole",
         "startFrameRole",
         "endFrameRole",
         "hasNoteRole",
         "subjectRole",
         "noteRole",
         "authorRole",
         "categoryRole",
         "colourRole",
         "createdRole",
         "hasAnnotationRole",
         "thumbnailRole",
         "ownerRole",
         "uuidRole",
         "objectRole",
         "startRole",
         "durationRole",
         "durationFrameRole",
         "visibleRole"}));
}

// don't optimise yet.
QVector<int> BookmarkModel::getRoleChanges(
    const bookmark::BookmarkDetail &ood, const bookmark::BookmarkDetail &nbd) const {
    QVector<int> roles;
    return roles;
}


QFuture<QString>
BookmarkModel::getJSONFuture(const QModelIndex &index, const QString &path) const {
    return QtConcurrent::run([=]() {
        if (bookmark_actor_) {
            std::string path_string = StdFromQString(path);
            try {
                scoped_actor sys{system()};
                auto addr   = UuidFromQUuid(index.data(uuidRole).toUuid());
                auto result = request_receive<JsonStore>(
                    *sys,
                    bookmark_actor_,
                    json_store::get_json_atom_v,
                    addr,
                    path_string);

                return QStringFromStd(result.dump());

            } catch ([[maybe_unused]] const std::exception &err) {
                // spdlog::warn("{} {}", __PRETTY_FUNCTION__,  err.what());
                return QString(); // QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

void BookmarkModel::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(caf::drop);

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {},
            [=](utility::event_atom, utility::change_atom) {},

            [=](utility::event_atom,
                bookmark::bookmark_change_atom,
                const utility::UuidActor &ua) {
                // spdlog::warn("bookmark::bookmark_change_atom {}", to_string(ua.uuid()) );
                auto ind = search_recursive(QUuidFromUuid(ua.uuid()), "uuidRole");

                if (ind.isValid()) {
                    try {

                        auto detail = getDetail(ua.actor());
                        // compare detail, log changed roles..
                        auto jsn = createJsonFromDetail(detail);

                        auto node = indexToTree(ind);
                        node->data().update(jsn);

                        auto change = getRoleChanges(bookmarks_.at(ua.uuid()), detail);
                        // if(not change.isEmpty()) {
                        bookmarks_[ua.uuid()] = detail;
                        emit dataChanged(ind, ind, change);
                        // }
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
            },
            [=](utility::event_atom,
                bookmark::remove_bookmark_atom,
                const utility::Uuid &uuid) {
                // find in model, remove row and delete from helper map.
                auto ind = search_recursive(QUuidFromUuid(uuid), "uuidRole");
                if (ind.isValid()) {
                    removeRows(ind.row(), 1, ind.parent());
                    bookmarks_.erase(uuid);
                    emit lengthChanged();
                }
            },
            [=](utility::event_atom,
                bookmark::add_bookmark_atom,
                const utility::UuidActor &ua) {
                // spdlog::warn("bookmark::add_bookmark_atom {}", to_string(ua.uuid()) );
                // spdlog::warn("bookmark::add_bookmark_atom {}", data_.dump(2) );
                auto ind = search_recursive(QUuidFromUuid(ua.uuid()), "uuidRole");

                if (not ind.isValid()) {
                    // request detail.
                    try {
                        auto detail = getDetail(ua.actor());

                        if (JSONTreeModel::insertRows(rowCount(), 1)) {
                            bookmarks_[ua.uuid()] = detail;
                            auto ind              = index(rowCount() - 1, 0);
                            JSONTreeModel::setData(
                                ind,
                                mapFromValue(createJsonFromDetail(detail)),
                                JSONTreeModel::Roles::JSONRole);
                            emit lengthChanged();
                        }

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                }
                // populateBookmarks();
            },

            [=](utility::event_atom, utility::name_atom, const std::string &) {},
            [=](json_store::update_atom,
                const JsonStore & /*change*/,
                const std::string &,
                const JsonStore &) {},

            [=](json_store::update_atom, const JsonStore &) {},
        };
    });
}

bookmark::BookmarkDetail BookmarkModel::getDetail(const caf::actor &actor) const {
    scoped_actor sys{system()};
    return request_receive<bookmark::BookmarkDetail>(
        *sys, actor, bookmark::bookmark_detail_atom_v);
}


void BookmarkModel::setBookmarkActorAddr(const QString &addr) {
    if (addr != bookmark_actor_addr_) {
        bookmark_actor_addr_ = addr;
        emit bookmarkActorAddrChanged();

        bookmark_actor_ = actorFromQString(system(), addr);

        scoped_actor sys{system()};

        if (backend_events_) {
            try {
                request_receive<bool>(
                    *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
            } catch ([[maybe_unused]] const std::exception &e) {
            }
            backend_events_ = caf::actor();
        }

        // join bookmark events
        if (bookmark_actor_) {
            try {
                auto detail =
                    request_receive<ContainerDetail>(*sys, bookmark_actor_, detail_atom_v);
                // uuid_           = detail.uuid_;
                backend_events_ = detail.group_;
                request_receive<bool>(
                    *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }

        // we now need to pull all the bookmarks and reset the model.
        //  we might want to spin this off..
        setModelData(jsonFromBookmarks());
        emit lengthChanged();
    }
}

nlohmann::json
BookmarkModel::createJsonFromDetail(const bookmark::BookmarkDetail &detail) const {
    auto result = R"({"uuid": null, "thumbnailURL": "qrc:///feather_icons/film.svg"})"_json;

    result["uuid"] = detail.uuid_;
    // spdlog::warn("{} {} {}", bool(detail.owner_) , to_string((*(detail.owner_)).actor()),
    // bool(detail.logical_start_frame_) );

    if (detail.owner_ and (*(detail.owner_)).actor() and detail.logical_start_frame_) {
        result["thumbnailURL"] = StdFromQString(getThumbnailURL(
            system(), (*(detail.owner_)).actor(), *(detail.logical_start_frame_), true));
    }

    return result;
}

nlohmann::json BookmarkModel::jsonFromBookmarks() {
    nlohmann::json result = R"([])"_json;

    if (bookmark_actor_) {
        scoped_actor sys{system()};
        try {
            // get all bookmark detail..
            auto details = request_receive<std::vector<bookmark::BookmarkDetail>>(
                *sys, bookmark_actor_, bookmark::bookmark_detail_atom_v, UuidVector());
            bookmarks_.clear();

            // we now need to build json data from these...
            for (const auto &i : details) {
                bookmarks_[i.uuid_] = i;
                result.emplace_back(createJsonFromDetail(i));
            }

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    return result;
}

bool BookmarkModel::removeRows(int row, int count, const QModelIndex &parent) {
    bool result = false;

    // build uuid list..
    if (bookmark_actor_) {
        auto uuids = std::vector<utility::Uuid>();
        for (auto i = row; i < row + count; i++) {
            auto ind = index(i, 0, parent);
            if (not ind.isValid())
                break;
            uuids.emplace_back(UuidFromQUuid(ind.data(Roles::uuidRole).toUuid()));
        }

        for (const auto &i : uuids) {
            anon_send(bookmark_actor_, bookmark::remove_bookmark_atom_v, i);
            bookmarks_.erase(i);
        }
    }

    result = JSONTreeModel::removeRows(row, count, parent);

    if (result)
        emit lengthChanged();

    return result;
}


QVariant BookmarkModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);
        if (j.count("uuid")) {
            const auto &detail = bookmarks_.at(Uuid(j.at("uuid")));

            switch (role) {
            case objectRole:
                // result = QVariant::fromValue(dynamic_cast<QObject
                // *>(data_[index.row()]));
                break;

            case startRole:
                result = QVariant::fromValue(timebase::to_seconds(*(detail.start_)));
                break;

            case durationRole:
                result = QVariant::fromValue(timebase::to_seconds(*(detail.duration_)));
                break;

            case durationFrameRole:
                if (detail.media_reference_) {
                    result = QVariant::fromValue(
                        *(detail.duration_) / detail.media_reference_->rate());
                }
                break;

            case uuidRole:
                result = QVariant::fromValue(QUuidFromUuid(detail.uuid_));
                break;

            case visibleRole:
                result = QVariant::fromValue(*(detail.visible_));
                break;


            case enabledRole:
                result = QVariant::fromValue(*(detail.enabled_));
                break;
            case focusRole:
                result = QVariant::fromValue(*(detail.has_focus_));
                break;
            case frameRole:
                result = QVariant::fromValue(*(detail.logical_start_frame_) + 1);
                break;

            case startTimecodeRole:
                result = QVariant::fromValue(QStringFromStd(detail.start_timecode()));
                break;

            case frameFromTimecodeRole:
                result = QVariant::fromValue(detail.start_timecode_tc().total_frames());
                break;

            case startFrameRole:
                if (detail.media_reference_) {
                    auto fc =
                        (*(detail.media_reference_))
                            .duration()
                            .frame(max(*(detail.start_), timebase::k_flicks_zero_seconds));
                    // turn into timecode..
                    result = QVariant::fromValue(fc);
                } else
                    result = 0;
                break;

            case endTimecodeRole:
                result = QVariant::fromValue(QStringFromStd(detail.end_timecode()));
                break;

            case endFrameRole:

                if (detail.media_reference_) {
                    auto fs =
                        (*(detail.media_reference_))
                            .duration()
                            .frame(max(*(detail.start_), timebase::k_flicks_zero_seconds));
                    auto fd =
                        (*(detail.media_reference_)).duration().frame(*(detail.duration_));

                    if (*(detail.duration_) == timebase::k_flicks_max) {
                        fd = (*(detail.media_reference_))
                                 .duration()
                                 .frame(
                                     (*(detail.media_reference_)).duration().duration() -
                                     max(*(detail.start_), timebase::k_flicks_zero_seconds));
                    }

                    result = QVariant::fromValue(fs + fd);
                } else
                    result = QVariant::fromValue(0);
                break;

            case durationTimecodeRole:
                result = QVariant::fromValue(QStringFromStd(detail.duration_timecode()));
                break;

            case hasNoteRole:
                result = QVariant::fromValue(*(detail.has_note_));
                break;
            case noteRole:
                if (detail.note_)
                    result = QVariant::fromValue(QStringFromStd(*(detail.note_)));
                break;
            case authorRole:
                if (detail.author_)
                    result = QVariant::fromValue(QStringFromStd(*(detail.author_)));
                break;
            case subjectRole:
                if (detail.subject_)
                    result = QVariant::fromValue(QStringFromStd(*(detail.subject_)));
                break;
            case categoryRole:
                if (detail.category_)
                    result = QVariant::fromValue(QStringFromStd(*(detail.category_)));
                break;
            case colourRole:
                if (detail.colour_)
                    result = QVariant::fromValue(QStringFromStd(*(detail.colour_)));
                break;
            case createdRole:
                if (detail.created_) {
                    result = QVariant::fromValue(QLocale::system().toString(
                        QDateTime::fromSecsSinceEpoch(
                            std::chrono::duration_cast<std::chrono::seconds>(
                                (*(detail.created_)).time_since_epoch())
                                .count()),
                        QLocale::ShortFormat));
                } else {
                    result = QVariant::fromValue(QLocale::system().toString(
                        QDateTime::currentDateTime(), QLocale::ShortFormat));
                }
                break;
            case hasAnnotationRole:
                result = QVariant::fromValue(*(detail.has_annotation_));
                break;
            case thumbnailRole:
                result = QVariant::fromValue(QStringFromStd(j.at("thumbnailURL")));
                break;
            case ownerRole:
                result = QVariant::fromValue(QUuidFromUuid((*(detail.owner_)).uuid()));
                break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{}", rowCount(index.parent()));
        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalId());
    }

    return result;
}

bool BookmarkModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;
    QVector<int> roles({role});

    try {
        const nlohmann::json &j = indexToData(index);
        if (j.count("uuid")) {
            auto &detail = bookmarks_.at(Uuid(j.at("uuid")));
            bookmark::BookmarkDetail bm;
            bm.uuid_ = detail.uuid_;

            switch (role) {

            case startFrameRole:
                if (detail.media_reference_) {
                    bm.start_ = value.toInt() * (*(detail.media_reference_)).rate();
                    // adjust endFrame via duration
                    if (*(detail.duration_) != timebase::k_flicks_max) {
                        auto new_dur = *(detail.duration_) + (*(detail.start_) - *(bm.start_));
                        bm.duration_ = max(new_dur, timebase::k_flicks_zero_seconds);
                    }

                    if (bm.start_ != detail.start_) {
                        sendDetail(bm);
                        result = true;
                    }
                }
                break;

            case endFrameRole:
                if (detail.media_reference_) {
                    auto new_end = value.toInt() * (*(detail.media_reference_)).rate();

                    if (new_end < *(detail.start_)) {
                        bm.start_    = new_end;
                        bm.duration_ = timebase::k_flicks_zero_seconds;
                    } else if (*(detail.start_) == timebase::k_flicks_low) {
                        bm.duration_ = new_end;
                    } else {
                        bm.duration_ = (new_end - *(detail.start_));
                    }

                    if (bm.duration_ != detail.duration_) {
                        sendDetail(bm);
                        result = true;
                    }
                }
                break;

            case subjectRole: {
                auto str = StdFromQString(value.toString());
                if (not detail.subject_ or (*detail.subject_) != str) {
                    detail.subject_ = bm.subject_ = str;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case noteRole: {
                auto str = StdFromQString(value.toString());
                if (not detail.note_ or (*detail.note_) != str) {
                    detail.note_ = bm.note_ = str;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case authorRole: {
                auto str = StdFromQString(value.toString());
                if (not detail.author_ or (*detail.author_) != str) {
                    detail.author_ = bm.author_ = str;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case categoryRole: {
                auto str = StdFromQString(value.toString());
                if (not detail.category_ or (*detail.category_) != str) {
                    detail.category_ = bm.category_ = str;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case colourRole: {
                auto str = StdFromQString(value.toString());
                if (not detail.colour_ or (*detail.colour_) != str) {
                    detail.colour_ = bm.colour_ = str;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case createdRole: {
                auto datetime =
                    QLocale::system().toDateTime(value.toString(), QLocale::ShortFormat);
                auto created =
                    utility::sys_time_point(milliseconds(datetime.toMSecsSinceEpoch()));
                if (not detail.created_ or (*detail.created_) != created) {
                    detail.created_ = bm.created_ = created;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case ownerRole: {
                auto str = UuidFromQUuid(value.toUuid());
                if (not detail.owner_ or (*detail.owner_).uuid() != str) {
                    detail.owner_ = bm.owner_ = UuidActor(str, caf::actor());
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case startRole: {
                timebase::flicks val = timebase::to_flicks(value.toDouble());
                if (not detail.start_ or (*detail.start_) != val) {
                    detail.start_ = bm.start_ = val;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case durationRole: {
                timebase::flicks val = timebase::to_flicks(value.toDouble());
                if (not detail.duration_ or (*detail.duration_) != val) {
                    detail.duration_ = bm.duration_ = val;
                    sendDetail(bm);
                    result = true;
                }
            } break;

            case durationFrameRole: {
                if (detail.media_reference_) {
                    auto duration        = (*(detail.media_reference_)).rate() * value.toInt();
                    timebase::flicks val = std::min(
                        duration,
                        ((*(detail.media_reference_)).duration().duration() -
                         (*(detail.start_))) -
                            (*(detail.media_reference_)).rate());

                    if (not detail.duration_ or (*detail.duration_) != val) {
                        bm.duration_ = val;
                        sendDetail(bm);
                        result = true;
                    }
                }
            } break;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result) {
        emit dataChanged(index, index, roles);
    }

    return result;
}

void BookmarkModel::sendDetail(const bookmark::BookmarkDetail &detail) const {
    if (bookmark_actor_)
        anon_send(bookmark_actor_, bookmark::bookmark_detail_atom_v, detail.uuid_, detail);
}

Q_INVOKABLE bool BookmarkModel::insertRows(int row, int count, const QModelIndex &parent) {
    auto result = false;
    try {
        if (bookmark_actor_) {

            if (JSONTreeModel::insertRows(row, count, parent)) {
                scoped_actor sys{system()};
                // should have empty entry in json..
                // create new bookmarks..
                for (auto i = 0; i < count; i++) {
                    auto ua = request_receive<utility::UuidActor>(
                        *sys, bookmark_actor_, bookmark::add_bookmark_atom_v);
                    auto detail = getDetail(ua.actor());

                    bookmarks_[ua.uuid()] = detail;
                    auto ind              = index(row + i, 0);
                    JSONTreeModel::setData(
                        ind,
                        mapFromValue(createJsonFromDetail(detail)),
                        JSONTreeModel::Roles::JSONRole);
                }

                emit lengthChanged();
                result = true;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}


QFuture<QString> BookmarkModel::exportCSVFuture(const QUrl &path) {
    return QtConcurrent::run([=]() {
        auto failed = std::string("CSV Export failed: ");
        if (bookmark_actor_) {
            try {
                scoped_actor sys{system()};

                auto data = request_receive<std::pair<std::string, std::vector<std::byte>>>(
                    *sys,
                    bookmark_actor_,
                    session::export_atom_v,
                    session::ExportFormat::EF_CSV);
                // write data to path..
                // this maybe a symlink in which case we should resolve it.
                std::ofstream o(uri_to_posix_path(UriFromQUrl(path)));
                o.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                o << std::get<0>(data);
                o.close();

                return QStringFromStd("CSV Exported successfuly.");
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                return QStringFromStd(failed + err.what());
            }
        }
        return QStringFromStd(failed + "No backend");
    });
}
