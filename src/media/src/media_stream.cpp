// SPDX-License-Identifier: Apache-2.0
#include <iostream>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::media;
using namespace xstudio::utility;

MediaStream::MediaStream(const JsonStore &jsn)
    : utility::Container(static_cast<JsonStore>(jsn["container"])),
      duration_(jsn["duration"]),
      key_format_(jsn["key_format"]),
      media_type_(media_type_from_string(jsn["media_type"])) {}

MediaStream::MediaStream(
    const std::string &name,
    utility::FrameRateDuration duration,
    const MediaType media_type,
    std::string key_format)
    : utility::Container(name, "MediaStream"),
      duration_(std::move(duration)),
      key_format_(std::move(key_format)),
      media_type_(media_type) {}

JsonStore MediaStream::serialise() const {
    JsonStore jsn;

    jsn["container"]  = Container::serialise();
    jsn["key_format"] = key_format_;
    jsn["media_type"] = to_readable_string(media_type_);
    jsn["duration"]   = duration_;

    return jsn;
}
