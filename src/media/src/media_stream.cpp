// SPDX-License-Identifier: Apache-2.0
#include <iostream>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::media;
using namespace xstudio::utility;

MediaStream::MediaStream(const JsonStore &jsn)
    : utility::Container(static_cast<JsonStore>(jsn["container"])) {
    detail_.duration_   = jsn["duration"];
    detail_.key_format_ = jsn["key_format"];
    detail_.media_type_ = media_type_from_string(jsn["media_type"]);
    detail_.name_       = name();

    // older versions of xstudio did not serialise these values. MediaStreamActor
    // takes care of re-scanning for the data in this case
    if (jsn.contains("resolution"))
        detail_.resolution_ = jsn["resolution"];
    if (jsn.contains("pixel_aspect") && jsn["pixel_aspect"].is_number())
        detail_.pixel_aspect_ = jsn["pixel_aspect"];
    if (jsn.contains("stream_index"))
        detail_.index_ = jsn["stream_index"];
}

MediaStream::MediaStream(const StreamDetail &detail)
    : utility::Container(detail.name_, "MediaStream"), detail_(detail) {}

JsonStore MediaStream::serialise() const {
    JsonStore jsn;

    jsn["container"]    = Container::serialise();
    jsn["key_format"]   = detail_.key_format_;
    jsn["media_type"]   = to_readable_string(detail_.media_type_);
    jsn["duration"]     = detail_.duration_;
    jsn["resolution"]   = detail_.resolution_;
    jsn["pixel_aspect"] = detail_.pixel_aspect_;
    jsn["stream_index"] = detail_.index_;

    return jsn;
}
