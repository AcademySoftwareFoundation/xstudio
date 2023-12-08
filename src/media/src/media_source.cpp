// SPDX-License-Identifier: Apache-2.0
#include <iostream>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::media;
using namespace xstudio::utility;

MediaSource::MediaSource(const JsonStore &jsn)
    : utility::Container(static_cast<JsonStore>(jsn["container"])), ref_(jsn["media_ref"]) {

    reader_ = jsn["reader"];

    size_     = jsn.value("size", 0);
    checksum_ = jsn.value("checksum", "");

    current_image_ = jsn["current_image"];
    for (const auto &i : jsn["image_streams"]) {
        image_streams_.push_back(i);
    }

    current_audio_ = jsn["current_audio"];
    for (const auto &i : jsn["audio_streams"]) {
        audio_streams_.push_back(i);
    }
}

MediaSource::MediaSource(
    const std::string &name, const caf::uri &_uri, const FrameList &frame_list)
    : utility::Container(name, "MediaSource"), ref_(_uri, frame_list, frame_list.empty()) {

    current_image_.clear();
    current_audio_.clear();
}

MediaSource::MediaSource(const std::string &name, const caf::uri &_uri)
    : utility::Container(name, "MediaSource"), ref_(_uri) {

    current_image_.clear();
    current_audio_.clear();
}

MediaSource::MediaSource(
    const std::string &name, const utility::MediaReference &media_reference_)
    : utility::Container(name, "MediaSource"), ref_(media_reference_) {

    current_image_.clear();
    current_audio_.clear();
}

void MediaSource::set_media_reference(const utility::MediaReference &ref) { ref_ = ref; }

JsonStore MediaSource::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();

    jsn["media_ref"] = ref_.serialise();

    jsn["reader"] = reader_;

    jsn["size"]     = size_;
    jsn["checksum"] = checksum_;

    jsn["current_image"] = current_image_;
    jsn["image_streams"] = {};
    for (const auto &i : image_streams_) {
        jsn["image_streams"].push_back(i);
    }

    jsn["current_audio"] = current_audio_;
    jsn["audio_streams"] = {};
    for (const auto &i : audio_streams_) {
        jsn["audio_streams"].push_back(i);
    }

    return jsn;
}

bool MediaSource::set_current(const MediaType media_type, const Uuid &uuid) {
    bool result = false;

    switch (media_type) {
    case MT_IMAGE:
        if (std::find(image_streams_.begin(), image_streams_.end(), uuid) !=
            std::end(image_streams_)) {
            current_image_ = uuid;
            result         = true;
        }
        break;
    case MT_AUDIO:
        if (std::find(audio_streams_.begin(), audio_streams_.end(), uuid) !=
            std::end(audio_streams_)) {
            current_audio_ = uuid;
            result         = true;
        }
        break;
    }
    return result;
}

Uuid MediaSource::current(const MediaType media_type) const {
    Uuid uuid;

    switch (media_type) {
    case MT_IMAGE:
        uuid = current_image_;
        break;
    case MT_AUDIO:
        uuid = current_audio_;
        break;
    }

    return uuid;
}

bool MediaSource::has_type(const MediaType media_type) const {
    bool result = false;

    switch (media_type) {
    case MT_IMAGE:
        result = not image_streams_.empty();
        break;
    case MT_AUDIO:
        result = not audio_streams_.empty();
        break;
    }

    return result;
}


void MediaSource::add_media_stream(
    const MediaType media_type, const Uuid &uuid, const Uuid &before_uuid) {
    switch (media_type) {
    case MT_IMAGE:
        if (not uuid.is_null()) {
            image_streams_.insert(
                std::find(image_streams_.begin(), image_streams_.end(), before_uuid), uuid);
            if (current_image_.is_null())
                current_image_ = uuid;
        }
        break;
    case MT_AUDIO:
        if (not uuid.is_null()) {
            audio_streams_.insert(
                std::find(audio_streams_.begin(), audio_streams_.end(), before_uuid), uuid);
            if (current_audio_.is_null())
                current_audio_ = uuid;
        }
        break;
    }
}

void MediaSource::remove_media_stream(const MediaType media_type, const Uuid &uuid) {
    switch (media_type) {
    case MT_IMAGE:
        image_streams_.erase(std::find(image_streams_.begin(), image_streams_.end(), uuid));

        if (uuid == current_image_) {
            if (not image_streams_.empty())
                current_image_ = *(image_streams_.begin());
            else
                current_image_.clear();
        }
        break;
    case MT_AUDIO:
        audio_streams_.erase(std::find(audio_streams_.begin(), audio_streams_.end(), uuid));

        if (uuid == current_audio_) {
            if (not audio_streams_.empty())
                current_audio_ = *(audio_streams_.begin());
            else
                current_audio_.clear();
        }
        break;
    }
}

const std::list<Uuid> &MediaSource::streams(const MediaType media_type) const {
    if (media_type == MT_IMAGE)
        return image_streams_;
    return audio_streams_;
}


void MediaSource::clear() {
    audio_streams_.clear();
    current_audio_.clear();
    image_streams_.clear();
    current_image_.clear();
}
