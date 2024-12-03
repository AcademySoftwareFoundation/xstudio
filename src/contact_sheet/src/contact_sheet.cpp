// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/contact_sheet/contact_sheet.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::contact_sheet;
using namespace xstudio::utility;

ContactSheet::ContactSheet(const std::string &name, const std::string &type)
    : Container(name, type), playhead_rate_(timebase::k_flicks_24fps) {}

ContactSheet::ContactSheet(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])),
      media_list_(static_cast<utility::JsonStore>(jsn["media"])) {
    playhead_rate_ = jsn["playhead_rate"];
}

JsonStore ContactSheet::serialise() const {
    JsonStore jsn;

    jsn["container"]     = Container::serialise();
    jsn["playhead_rate"] = playhead_rate_;

    // identify actors that are media..
    jsn["media"] = media_list_.serialise();

    return jsn;
}
