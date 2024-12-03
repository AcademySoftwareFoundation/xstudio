// SPDX-License-Identifier: Apache-2.0
#include <iostream>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::media;
using namespace xstudio::utility;

MediaKey::MediaKey(const std::string &o) : std::string(o) {
    hash_ = std::hash<std::string>{}(o);
}

MediaKey::MediaKey(
    const std::string &key_format,
    const caf::uri &uri,
    const int frame,
    const std::string &stream_id)
    : std::string(fmt::format(
            fmt::runtime(key_format),
            to_string(uri),
            (frame == std::numeric_limits<int>::min() ? 0 : frame),
            stream_id)) 
{
    hash_ = std::hash<std::string>{}(static_cast<const std::string&>(*this));
}

Media::Media(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])) {

    flag_      = jsn.value("flag", "#00000000");
    flag_text_ = jsn.value("flag_text", "");
    for (const auto &i : jsn["sub_media"]) {
        media_sources_.push_back(i);
    }
    set_current(jsn["current"]);
    if (jsn.find("current_audio") != jsn.end()) {
        set_current(jsn["current_audio"], MediaType::MT_AUDIO);
    }
}

Media::Media(
    const std::string &name,
    const utility::Uuid &current_image_source,
    const utility::Uuid &current_audio_source)
    : Container(name, "Media") {
    add_media_source(current_image_source);
    if (!current_audio_source.is_null()) {
        add_media_source(current_audio_source);
        set_current(current_audio_source, MediaType::MT_AUDIO);
    }
}

void Media::add_media_source(const Uuid &uuid, const Uuid &before_uuid) {
    if (not uuid.is_null()) {
        media_sources_.insert(
            std::find(media_sources_.begin(), media_sources_.end(), before_uuid), uuid);
    }
}

void Media::remove_media_source(const Uuid &uuid) {
    media_sources_.erase(std::find(media_sources_.begin(), media_sources_.end(), uuid));

    if (uuid == current_image_source_) {
        if (not media_sources_.empty())
            current_image_source_ = *(media_sources_.begin());
        else
            current_image_source_ = Uuid();
    }
    if (uuid == current_audio_source_) {
        if (not media_sources_.empty())
            current_audio_source_ = *(media_sources_.begin());
        else
            current_audio_source_ = Uuid();
    }
}

bool Media::set_current(const Uuid &uuid, const MediaType mt) {
    if (std::find(media_sources_.begin(), media_sources_.end(), uuid) !=
        std::end(media_sources_)) {
        if (mt == MediaType::MT_IMAGE) {
            current_image_source_ = uuid;
        } else {
            current_audio_source_ = uuid;
        }
        return true;
    }
    return false;
}

void Media::deserialise(const utility::JsonStore &jsn) {
    Container::deserialise(jsn.at("container"));
    media_sources_.clear();

    flag_      = jsn.value("flag", "#00000000");
    flag_text_ = jsn.value("flag_text", "");

    for (const auto &i : jsn.at("sub_media")) {
        media_sources_.push_back(i);
    }

    set_current(jsn.at("current"));

    if (jsn.find("current_audio") != jsn.end()) {
        set_current(jsn.at("current_audio"), MediaType::MT_AUDIO);
    }
}


JsonStore Media::serialise() const {
    JsonStore jsn;

    jsn["container"]     = Container::serialise();
    jsn["current"]       = current_image_source_;
    jsn["current_audio"] = current_audio_source_;
    jsn["flag"]          = flag_;
    jsn["flag_text"]     = flag_text_;
    jsn["sub_media"]     = {};
    for (const auto &i : media_sources_) {
        jsn["sub_media"].push_back(i);
    }

    return jsn;
}

MediaType xstudio::media::media_type_from_string(const std::string &str) {
    MediaType mt = MT_IMAGE;
    if (str == "Image")
        mt = MT_IMAGE;
    else if (str == "Audio")
        mt = MT_AUDIO;
    return mt;
}
