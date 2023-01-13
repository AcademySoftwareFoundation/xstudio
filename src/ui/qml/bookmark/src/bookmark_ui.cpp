// SPDX-License-Identifier: Apache-2.0

#include <iostream>

// CAF_PUSH_WARNINGS
// CAF_POP_WARNINGS

#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/bookmark_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/global_store/global_store.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::global_store;


BookmarkModel::BookmarkModel(BookmarksUI *controller, QObject *parent)
    : QAbstractListModel(parent), controller_(controller) {
    // QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

int BookmarkModel::startFrame(int row) const {
    int result = 0;

    if (row < data_.size()) {
        auto detail = data_[row]->detail_;

        if (detail.media_reference_) {
            auto fc = (*(detail.media_reference_))
                          .duration()
                          .frame(max(*(detail.start_), timebase::k_flicks_zero_seconds));
            // turn into timecode..
            result = fc;
        }
    }
    return result;
}

int BookmarkModel::endFrameExclusive(int row) const {
    auto detail = data_[row]->detail_;
    int result  = 0;
    if (detail.media_reference_) {
        auto fs = (*(detail.media_reference_))
                      .duration()
                      .frame(max(*(detail.start_), timebase::k_flicks_zero_seconds));
        auto fd = (*(detail.media_reference_)).duration().frame(*(detail.duration_));

        if (*(detail.duration_) == timebase::k_flicks_max) {
            fd = (*(detail.media_reference_))
                     .duration()
                     .frame(
                         (*(detail.media_reference_)).duration().duration() -
                         max(*(detail.start_), timebase::k_flicks_zero_seconds));
        }

        result = (fs + fd) - 1;
    }
    return result;
}


bool BookmarkModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (index.row() >= data_.size())
        return false;

    auto detail = data_[index.row()]->detail_;
    bookmark::BookmarkDetail bm;
    bm.uuid_ = detail.uuid_;

    switch (role) {
    case StartFrameRole:
        if (detail.media_reference_) {
            bm.start_ = value.toInt() * (*(detail.media_reference_)).rate();
            // adjust endFrame via duration
            if (*(detail.duration_) != timebase::k_flicks_max) {
                auto new_dur = *(detail.duration_) + (*(detail.start_) - *(bm.start_));
                bm.duration_ = max(new_dur, timebase::k_flicks_zero_seconds);
            }

            if (bm.start_ != detail.start_)
                return controller_->updateBookmark(bm);
        }
        break;
    case EndFrameRole:
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
            if (bm.duration_ != detail.duration_)
                return controller_->updateBookmark(bm);
        }
        break;

    case NoteRole: {
        auto note = value.toString().toStdString();

        if ((not detail.note_ and not note.empty()) or
            (detail.note_ and (*(detail.note_)) != note)) {
            bm.note_ = note;
            return controller_->updateBookmark(bm);
        }
    } break;

    case AuthorRole: {
        auto author = value.toString().toStdString();
        if (not detail.author_ or (*(detail.author_)) != author) {
            bm.author_ = author;
            return controller_->updateBookmark(bm);
        }
    } break;

    case SubjectRole: {
        auto subject = value.toString().toStdString();
        if (not detail.subject_ or (*(detail.subject_)) != subject) {
            bm.subject_ = subject;
            return controller_->updateBookmark(bm);
        }
    } break;

    case CategoryRole: {
        auto category = value.toString().toStdString();
        if (not detail.category_ or (*(detail.category_)) != category) {
            bm.category_ = category;
            return controller_->updateBookmark(bm);
        }
    } break;

    case ColourRole: {
        auto colour = value.toString().toStdString();
        if (not detail.colour_ or (*(detail.colour_)) != colour) {
            bm.colour_ = colour;
            return controller_->updateBookmark(bm);
        }
    } break;

    case CreatedRole: {
        // expects string..
        auto datetime = QLocale::system().toDateTime(value.toString(), QLocale::ShortFormat);
        auto created  = utility::sys_time_point(milliseconds(datetime.toMSecsSinceEpoch()));
        if (not detail.created_ or (*(detail.created_)) != created) {
            bm.created_ = created;
            return controller_->updateBookmark(bm);
        }
    } break;
    }
    return false;
}


QVariant BookmarkModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();
    if (index.row() < data_.size()) {
        auto detail = data_[index.row()]->detail_;
        switch (role) {
        case ObjectRole:
            result = QVariant::fromValue(dynamic_cast<QObject *>(data_[index.row()]));
            break;

        case UuidRole:
            result = QVariant::fromValue(QUuidFromUuid(detail.uuid_));
            break;

        case EnabledRole:
            result = QVariant::fromValue(*(detail.enabled_));
            break;
        case FocusRole:
            result = QVariant::fromValue(*(detail.has_focus_));
            break;
        case FrameRole:
            result = QVariant::fromValue(*(detail.logical_start_frame_) + 1);
            break;

        case StartTimecodeRole:
            result = QVariant::fromValue(QStringFromStd(detail.start_timecode()));
            break;

        case FrameFromTimecodeRole:
            result = QVariant::fromValue(detail.start_timecode_tc().total_frames());
            break;

        case StartFrameRole:
            result = QVariant::fromValue(startFrame(index.row()));
            break;

        case EndTimecodeRole:
            result = QVariant::fromValue(QStringFromStd(detail.end_timecode()));
            break;

        case EndFrameRole:
            result = QVariant::fromValue(endFrameExclusive(index.row()) + 1);
            break;

        case DurationTimecodeRole:
            result = QVariant::fromValue(QStringFromStd(detail.duration_timecode()));
            break;

        case HasNoteRole:
            result = QVariant::fromValue(*(detail.has_note_));
            break;
        case NoteRole:
            if (detail.note_)
                result = QVariant::fromValue(QStringFromStd(*(detail.note_)));
            break;
        case AuthorRole:
            if (detail.author_)
                result = QVariant::fromValue(QStringFromStd(*(detail.author_)));
            break;
        case SubjectRole:
            if (detail.subject_)
                result = QVariant::fromValue(QStringFromStd(*(detail.subject_)));
            break;
        case CategoryRole:
            if (detail.category_)
                result = QVariant::fromValue(QStringFromStd(*(detail.category_)));
            break;
        case ColourRole:
            if (detail.colour_)
                result = QVariant::fromValue(QStringFromStd(*(detail.colour_)));
            break;
        case CreatedRole:
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
        case HasAnnotationRole:
            result = QVariant::fromValue(*(detail.has_annotation_));
            break;
        case ThumbnailRole:
            if (data_[index.row()]->thumbnailURL_.isEmpty())
                result = QVariant::fromValue(QString("qrc:///feather_icons/film.svg"));
            else
                result = QVariant::fromValue(data_[index.row()]->thumbnailURL_);
            break;
        case OwnerRole:
            result = QVariant::fromValue(QUuidFromUuid((*(detail.owner_)).uuid()));
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> BookmarkModel::roleNames() const {
    QHash<int, QByteArray> roles;
    for (const auto &p : BookmarkModel::role_names) {
        roles[p.first] = p.second.c_str();
    }
    return roles;
}

int BookmarkModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return data_.size();
}


void BookmarkModel::populate(QList<BookmarkUI *> &items, const bool changed) {
    // Be a bit smarter..

    // remove missing entries
    for (int i = 0; i < data_.size(); i++) {
        if (not items.contains(data_[i])) {
            beginRemoveRows(QModelIndex(), i, i);
            data_.removeAt(i);
            endRemoveRows();
            i--;
        }
    }

    // add new entries.
    auto old_count = data_.size();
    for (auto item : items) {
        if (not data_.contains(item)) {
            data_.push_back(item);
        }
    }

    if (old_count != data_.size()) {
        beginInsertRows(QModelIndex(), old_count, data_.size() - 1);
        endInsertRows();
    }

    if (changed) {
        // need to be smart about what has changed...
        emit dataChanged(index(0, 0), index(data_.size() - 1, 0));
        // void QAbstractItemModel::dataChanged(const QModelIndex &topLeft, const QModelIndex
        // &bottomRight, const QVector<int> &roles = QVector<int>()) need to handle data updates
        // emit layoutAboutToBeChanged();
        // emit layoutChanged();
    }
}

BookmarkUI::BookmarkUI(QObject *parent) : QObject(parent) {}


QFuture<bool>
BookmarksUI::setJSONFuture(BookmarkUI *bm, const QString &json, const QString &path) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto result = request_receive<bool>(
                    *sys,
                    bm->backend_,
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

QFuture<QString> BookmarksUI::getJSONFuture(BookmarkUI *bm, const QString &path) {
    return QtConcurrent::run([=]() {
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto result = request_receive<JsonStore>(
                    *sys, bm->backend_, json_store::get_json_atom_v, StdFromQString(path));

                return QStringFromStd(result.dump());
            } catch (const std::exception &err) {
                // spdlog::warn("{} {}", __PRETTY_FUNCTION__,  err.what());
                return QString(); // QStringFromStd(err.what());
            }
        }
        return QString();
    });
}


bool BookmarksUI::setJSON(BookmarkUI *bm, const QString &json, const QString &path) {
    return setJSONFuture(bm, json, path).result();
}

QString BookmarksUI::getJSON(BookmarkUI *bm, const QString &path) {
    return getJSONFuture(bm, path).result();
}

BookmarksUI::BookmarksUI(QObject *parent)
    : QMLActor(parent),
      bookmarks_model_(new BookmarkModel(this, this)),
      backend_(),
      backend_events_() {}

void BookmarksUI::set_backend(caf::actor backend) {

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
            uuid_           = detail.uuid_;
            backend_events_ = detail.group_;
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
        } catch (const std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
        emit uuidChanged();
    }
    populateBookmarks();


    emit backendChanged();
    spdlog::debug("BookmarksUI set_backend {}", to_string(uuid_));
}

void BookmarksUI::init(actor_system &system_) {
    QMLActor::init(system_);
    emit systemInit(this);

    self()->set_down_handler([=](down_msg &msg) {
        if (msg.source == backend_)
            set_backend(caf::actor());
    });

    // access global data store to retrieve stored filters.
    try {
        scoped_actor sys{system()};
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        request_receive<bool>(
            *sys, prefs.get_group(js), broadcast::join_broadcast_atom_v, as_actor());
        auto category = preference_value<JsonStore>(js, "/core/bookmark/category");
        if (category.is_array()) {
            for (const auto &i : category) {
                addCategory(
                    i.value("text", i.value("value", "default")),
                    i.value("value", "default"),
                    i.value("colour", ""));
            }
        }
        emit categoriesChanged();
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    spdlog::debug("BookmarksUI init");

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](utility::event_atom, utility::last_changed_atom, const time_point &) {},
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &) {},
            [=](utility::event_atom, utility::change_atom) {},

            [=](utility::event_atom,
                bookmark::bookmark_change_atom,
                const utility::UuidActor &) { populateBookmarks(true); },
            [=](utility::event_atom, bookmark::remove_bookmark_atom, const utility::Uuid &) {
                populateBookmarks();
            },
            [=](utility::event_atom, bookmark::add_bookmark_atom, const utility::UuidActor &) {
                populateBookmarks();
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

bool BookmarksUI::addCategory(
    const std::string &text, const std::string &value, const std::string &colour) {
    auto changed = false;
    if (not cat_map_.count(value)) {
        categories_.push_back(new CategoryObject(
            QStringFromStd(text), QStringFromStd(value), QStringFromStd(colour), this));
        cat_map_.insert(value);
        changed = true;
    }
    return changed;
}


void BookmarksUI::populateBookmarks(const bool changed) {
    UuidVector new_bookmarks;
    auto new_category = false;
    // clear model
    ownedBookmarks_.clear();
    // get bookmarks.
    scoped_actor sys{system()};
    try {
        utility::UuidActorVector result;
        try {
            result = request_receive<utility::UuidActorVector>(
                *sys, backend_, bookmark::get_bookmarks_atom_v);
        } catch (const std::exception &err) {
            spdlog::error("{} {} {}", __PRETTY_FUNCTION__, "Bookmark Manager down", err.what());
            throw;
        }
        std::map<utility::Uuid, BookmarkUI *> bmmap;

        for (auto &i : bookmarks_)
            bmmap[i->uuid_] = i;

        for (const auto &i : result) {
            // check we don't have it..
            try {
                auto detail = request_receive<bookmark::BookmarkDetail>(
                    *sys, i.actor(), bookmark::bookmark_detail_atom_v);
                BookmarkUI *bm = nullptr;

                if (bmmap.count(i.uuid())) {
                    bm           = bmmap[i.uuid()];
                    bm->backend_ = i.actor();
                } else {
                    new_bookmarks.push_back(i.uuid());
                    bm = new BookmarkUI(this);
                    QQmlEngine::setObjectOwnership(bm, QQmlEngine::CppOwnership);

                    bookmarks_.push_back(bm);
                    bm->uuid_         = i.uuid();
                    bm->backend_      = i.actor();
                    bm->thumbnailURL_ = QString("qrc:///feather_icons/film.svg");

                    if (detail.category_)
                        new_category |= addCategory(
                            *(detail.category_), *(detail.category_), detail.colour());
                }

                bm->detail_ = detail;

                // has owner uuid add to media map..
                // and yes variantmaps are seriously inefficient.
                if (detail.owner_ and not(*(detail.owner_)).uuid().is_null()) {
                    QString key = QStringFromStd(to_string((*(detail.owner_)).uuid()));
                    QVariantList olist;
                    if (ownedBookmarks_.count(key))
                        olist = ownedBookmarks_[key].toList();
                    olist.push_back(QVariant::fromValue(QUuidFromUuid(bm->uuid_)));
                    ownedBookmarks_[key] = olist;
                }

                if (detail.owner_ and (*(detail.owner_)).actor() and
                    detail.logical_start_frame_) {
                    bm->thumbnailURL_ = getThumbnailURL(
                        system(),
                        (*(detail.owner_)).actor(),
                        *(detail.logical_start_frame_),
                        true);
                }
            } catch (const std::exception &err) {
                spdlog::warn(
                    "{} Failed to query bookmark detail {} {}",
                    __PRETTY_FUNCTION__,
                    to_string(i.uuid()),
                    err.what());
            }
        }
        // remove ones in use
        for (const auto &i : result) {
            if (bmmap.count(i.uuid()))
                bmmap.erase(i.uuid());
        }

        for (const auto &i : bmmap) {
            // remove from ownedBookmarks_
            // remove from cache
            for (auto j = 0; j < bookmarks_.size(); j++) {
                if (bookmarks_[j]->uuid_ == i.first) {
                    // remove from ownedBookmarks_
                    if (bookmarks_[j]->detail_.owner_ and
                        not(*(bookmarks_[j]->detail_.owner_)).uuid().is_null()) {
                        auto key = QStringFromStd(
                            to_string((*(bookmarks_[j]->detail_.owner_)).uuid()));
                        if (ownedBookmarks_.count(key)) {
                            // remove specific bookmark.
                            auto olist = ownedBookmarks_[key].toList();
                            for (auto k = 0; k < olist.size(); k++) {
                                if (UuidFromQUuid(olist[k].toUuid()) == i.first) {
                                    olist.removeAt(k);
                                    break;
                                }
                            }
                            ownedBookmarks_[key] = olist;
                        }
                    }

                    bookmarks_.removeAt(j);
                    break;
                }
            }
        }

        bookmarks_model_->populate(bookmarks_, changed);

        // free from memory
        for (const auto &i : bmmap) {
            delete i.second;
        }
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    if (new_category)
        emit categoriesChanged();

    // emit bookmarkModelChanged();
    emit ownedBookmarksChanged();
    if (not new_bookmarks.empty()) {
        emit newBookmarks();
        for (const auto &i : new_bookmarks)
            emit newBookmark(QUuidFromUuid(i));
    }
}

bool BookmarksUI::updateBookmark(const bookmark::BookmarkDetail &detail) {
    // stop spurious updates..
    scoped_actor sys{system()};
    bool result = false;
    try {
        result = request_receive<bool>(
            *sys, backend_, bookmark::bookmark_detail_atom_v, detail.uuid_, detail);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

QUuid BookmarksUI::addBookmark(MediaUI *src) {
    scoped_actor sys{system()};

    return QUuidFromUuid(addBookmark(src->backend_ua()).uuid());
}

void BookmarksUI::removeBookmarks(const QList<QUuid> &src) {
    for (const auto &i : src)
        anon_send(backend_, bookmark::remove_bookmark_atom_v, UuidFromQUuid(i));
}

utility::UuidActor BookmarksUI::addBookmark(const utility::UuidActor &src) {
    scoped_actor sys{system()};
    UuidActor result;
    try {
        result = request_receive<UuidActor>(*sys, backend_, bookmark::add_bookmark_atom_v, src);
    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return result;
}

double BookmarkDetailUI::start() const {
    if (detail_.start_)
        return timebase::to_seconds(*(detail_.start_));

    return 0.0;
}

QString BookmarkDetailUI::category() const {
    if (detail_.category_)
        return QStringFromStd(*(detail_.category_));

    return "";
}

QString BookmarkDetailUI::colour() const {
    if (detail_.colour_)
        return QStringFromStd(*(detail_.colour_));

    return "";
}

QString BookmarkDetailUI::subject() const {
    if (detail_.subject_)
        return QStringFromStd(*(detail_.subject_));

    return "";
}

double BookmarkDetailUI::duration() const {
    if (detail_.duration_)
        return timebase::to_seconds(*(detail_.duration_));

    return 0.0;
}

void BookmarkDetailUI::setStart(const double seconds) {
    detail_.start_ = timebase::to_flicks(seconds);
}

void BookmarkDetailUI::setDuration(const double seconds) {
    detail_.duration_ = timebase::to_flicks(seconds);
}

void BookmarkDetailUI::setColour(const QString &value) {
    detail_.colour_ = StdFromQString(value);
}

void BookmarkDetailUI::setSubject(const QString &value) {
    detail_.subject_ = StdFromQString(value);
}

void BookmarkDetailUI::setCategory(const QString &value) {

    {

        auto &system = xstudio::ui::qml::CafSystemObject::get_actor_system();

        // try and record the category as the default note category
        scoped_actor sys{system};
        auto session = request_receive<caf::actor>(
            *sys, system.registry().get<caf::actor>(studio_registry), session::session_atom_v);

        auto bookmark =
            utility::request_receive<caf::actor>(*sys, session, bookmark::get_bookmark_atom_v);

        anon_send(bookmark, bookmark::default_category_atom_v, StdFromQString(value));
    }

    detail_.category_ = StdFromQString(value);
}

void BookmarksUI::setDuration(const QUuid &uuid, const int frames) {
    auto buuid = UuidFromQUuid(uuid);
    for (const auto &i : bookmarks_) {
        if (i->uuid_ == buuid and i->detail_.media_reference_) {

            bookmark::BookmarkDetail bd;
            bd.uuid_ = buuid;

            auto new_duration = (*(i->detail_.media_reference_)).rate() * frames;


            bd.duration_ = std::min(
                new_duration,
                ((*(i->detail_.media_reference_)).duration().duration() -
                 (*(i->detail_.start_))) -
                    (*(i->detail_.media_reference_)).rate());
            updateBookmark(bd);
            break;
        }
    }
}

QFuture<QString> BookmarksUI::exportCSVFuture(const QUrl &path) {
    return QtConcurrent::run([=]() {
        auto failed = std::string("CSV Export failed: ");
        if (backend_) {
            try {
                scoped_actor sys{system()};

                auto data = request_receive<std::pair<std::string, std::vector<std::byte>>>(
                    *sys, backend_, session::export_atom_v, session::ExportFormat::EF_CSV);
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
