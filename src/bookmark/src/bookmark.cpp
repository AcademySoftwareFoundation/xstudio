// SPDX-License-Identifier: Apache-2.0

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::bookmark;
using namespace xstudio::utility;


Note::Note(const JsonStore &jsn) {
    author_   = jsn.value("author", "Anonymous");
    category_ = jsn.value("category", "");
    colour_   = jsn.value("colour", "");
    note_     = jsn.value("note", "");
    subject_  = jsn.value("subject", "");
    created_  = jsn.value("created", utility::sysclock::now());
}

JsonStore Note::serialise() const {
    JsonStore jsn;

    jsn["subject"]  = subject_;
    jsn["author"]   = author_;
    jsn["category"] = category_;
    jsn["colour"]   = colour_;
    jsn["note"]     = note_;
    jsn["created"]  = created_;

    return jsn;
}
Bookmark::Bookmark(const utility::Uuid &uuid) : Container("Bookmark", "Bookmark") {
    set_uuid(uuid);
}

Bookmark::Bookmark(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])) {

    if (jsn.count("note") and not jsn["note"].is_null())
        note_ = std::make_shared<Note>(jsn["note"]);

    start_     = jsn.value("start", timebase::k_flicks_low);
    duration_  = jsn.value("duration", timebase::k_flicks_max);
    enabled_   = jsn.value("enabled", true);
    owner_     = jsn.value("owner", utility::Uuid());
    visible_   = jsn.value("visible", true);
    user_type_ = jsn.value("user_type", std::string());
    user_data_ = jsn.value("user_data", utility::JsonStore());
    created_   = jsn.value("created", utility::sysclock::now());

    // N.B. AnnotationBase creation requires caf api comms with plugins and
    // is handled by the bookmark actor
}

Bookmark::Bookmark(const Bookmark &src) : Bookmark(src.serialise()) {

    // N.B. AnnotationBase creation requires caf api comms with plugins and
    // therefore annotation copying is handled by the bookmark actor
}

void Bookmark::create_note() { note_ = std::make_shared<Note>(); }
void Bookmark::create_annotation() { annotation_ = std::make_shared<AnnotationBase>(); }

JsonStore Bookmark::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    if (note_)
        jsn["note"] = note_->serialise();
    else
        jsn["note"] = nullptr;
    if (annotation_) {
        utility::Uuid plugin_uuid;
        jsn["annotation"]                = annotation_->serialise(plugin_uuid);
        jsn["annotation"]["plugin_uuid"] = plugin_uuid;
    } else
        jsn["annotation"] = nullptr;
    jsn["start"]     = start_;
    jsn["duration"]  = duration_;
    jsn["enabled"]   = enabled_;
    jsn["owner"]     = owner_;
    jsn["visible"]   = visible_;
    jsn["user_type"] = user_type_;
    jsn["user_data"] = user_data_;
    jsn["created"]   = created_;

    return jsn;
}

bool Bookmark::update(const BookmarkDetail &detail) {
    auto changed = false;

    if (detail.enabled_) {
        enabled_ = *(detail.enabled_);
        changed  = true;
    }

    if (detail.has_focus_) {
        has_focus_ = *(detail.has_focus_);
        changed    = true;
    }

    if (detail.visible_) {
        visible_ = *(detail.visible_);
        changed  = true;
    }

    if (detail.start_) {
        start_  = *(detail.start_);
        changed = true;
    }

    if (detail.owner_) {
        owner_  = (*(detail.owner_)).uuid();
        changed = true;
    }

    if (detail.duration_) {
        duration_ = *(detail.duration_);
        changed   = true;
    }

    if (detail.user_type_) {
        user_type_ = *(detail.user_type_);
        changed    = true;
    }

    if (detail.user_data_) {
        user_data_ = *(detail.user_data_);
        changed    = true;
    }

    if (detail.created_) {
        created_ = *(detail.created_);
        changed  = true;
    }

    if (detail.author_) {
        if (not has_note())
            create_note();
        note_->set_author(*(detail.author_));
        changed = true;
    }

    if (detail.note_) {
        if (not has_note()) {
            create_note();
            note_->set_author(get_user_name());
        }
        note_->set_note(*(detail.note_));
        changed = true;
    }

    if (detail.subject_) {
        if (not has_note()) {
            create_note();
            note_->set_author(get_user_name());
        }
        note_->set_subject(*(detail.subject_));
        changed = true;
    }


    if (detail.category_) {
        if (not has_note()) {
            create_note();
            note_->set_author(get_user_name());
        }
        note_->set_category(*(detail.category_));
        changed = true;
    }

    if (detail.colour_) {
        if (not has_note()) {
            create_note();
            note_->set_author(get_user_name());
        }
        note_->set_colour(*(detail.colour_));
        changed = true;
    }

    if (detail.created_) {
        if (not has_note()) {
            create_note();
            note_->set_author(get_user_name());
        }
        note_->set_created(*(detail.created_));
        changed = true;
    }

    return changed;
}


Bookmarks::Bookmarks(const std::string &name) : Container(name, "Bookmarks") {}

Bookmarks::Bookmarks(const utility::JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])) {
    // if(jsn.count("associations")) {
    //     for(const auto &i : jsn["associations"].items()) {
    //         auto uuid = utility::Uuid(i.key());
    //         for(const auto &ii : i.value() ) {
    //             associate(uuid, ii);
    //         }
    //     }
    // }
}

JsonStore Bookmarks::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    // jsn["associations"] = nlohmann::json::object();

    // for(const auto &i : bookmark_associations_) {
    //     auto uuid_str = to_string(i.first);
    //     jsn["associations"][uuid_str] = nlohmann::json::array();
    //     for(const auto &ii: i.second)
    //         jsn["associations"][uuid_str].push_back(ii);

    // }

    return jsn;
}

BookmarkDetail &BookmarkDetail::operator=(const Bookmark &other) {
    uuid_      = other.uuid();
    enabled_   = other.enabled_;
    has_focus_ = other.has_focus_;
    visible_   = other.visible_;
    start_     = other.start_;
    duration_  = other.duration_;
    user_type_ = other.user_type_;
    user_data_ = other.user_data_;
    created_   = other.created_;

    has_note_       = other.has_note();
    has_annotation_ = other.has_annotation();
    owner_          = UuidActor(other.owner_, caf::actor());

    if (*(has_note_)) {
        author_   = other.note_->author();
        subject_  = other.note_->subject();
        note_     = other.note_->note();
        category_ = other.note_->category();
        colour_   = other.note_->colour();
        created_  = other.note_->created();
    }
    return *this;
}


// void Bookmarks::associate(const utility::Uuid &bookmark, const utility::Uuid &src) {
//     // mark that we've had an connection, this will be used for garbage collection.
//     // if(not associations_.count(bookmark))
//     //     associations_.insert(std::make_pair(bookmark, utility::Uuid()));
//     // associations_.insert(std::make_pair(bookmark, src));
//     if(not bookmark_associations_.count(bookmark))
//         bookmark_associations_[bookmark] = std::set<utility::Uuid>();

//     if(not src_associations_.count(src))
//         src_associations_[src] = std::set<utility::Uuid>();

//     bookmark_associations_[bookmark].insert(src);
//     src_associations_[src].insert(bookmark);
// }

// void Bookmarks::disassociate(const utility::Uuid &bookmark, const utility::Uuid &src) {
//     if(bookmark_associations_.count(bookmark) and
//     bookmark_associations_[bookmark].count(src))
//         bookmark_associations_[bookmark].erase(src);

//     if(src_associations_.count(src) and bookmark_associations_[src].count(bookmark))
//         src_associations_[src].erase(bookmark);
// }


// void Bookmarks::disassociate_from_src(const utility::Uuid &src) {
//     // remove all associations to src.
//     // search all...
//     if(src_associations_.count(src)) {
//         for(auto i: src_associations_[src])
//             bookmark_associations_[i].erase(src);
//         src_associations_.erase(src);
//     }
// }

// void Bookmarks::disassociate_from_bookmark(const utility::Uuid &bookmark) {
//     // remove all associations to src.
//     // search all...
//     if(bookmark_associations_.count(bookmark)) {
//         for(auto i: bookmark_associations_[bookmark])
//             src_associations_[i].erase(bookmark);
//         bookmark_associations_.erase(bookmark);
//     }
// }

// bool Bookmarks::can_retire_bookmark(const utility::Uuid &bookmark) const {
//     return bookmark_associations_.count(bookmark) and
//     bookmark_associations_.at(bookmark).empty();
// }

// bool Bookmarks::can_retire_src(const utility::Uuid &src) const {
//     return src_associations_.count(src) and src_associations_.at(src).empty();
// }

// UuidVector Bookmarks::can_retire_srcs() const {
//     UuidVector result;

//     for(const auto &i: src_associations_) {
//         if(i.second.empty())
//             result.push_back(i.first);
//     }

//     return result;
// }

// UuidVector Bookmarks::can_retire_bookmarks() const {
//     UuidVector result;

//     for(const auto &i: bookmark_associations_) {
//         if(i.second.empty())
//             result.push_back(i.first);
//     }

//     return result;
// }

// bool Bookmarks::has_association(const utility::Uuid &src) const {
//     return src_associations_.count(src) ? true : false;
// }

// utility::UuidVector Bookmarks::get_bookmark_associations(const utility::Uuid &bookmark) const
// {
//     utility::UuidVector result;

//     if(bookmark_associations_.count(bookmark)) {
//         for(const auto &i : bookmark_associations_.at(bookmark))
//             result.push_back(i);
//     }

//     return result;
// }
