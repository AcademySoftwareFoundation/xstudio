// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/timeline/marker.hpp"

using namespace xstudio;
using namespace xstudio::timeline;
using namespace xstudio::utility;

Marker::Marker(const utility::JsonStore &jsn) {
    uuid_  = jsn.at("uuid");
    range_ = jsn.at("range");
    name_  = jsn.value("name", "");
    flag_  = jsn.value("flag", "");
    prop_  = jsn.value("prop", JsonStore());
}

utility::JsonStore Marker::serialise() const {
    utility::JsonStore jsn;

    jsn["uuid"]  = uuid_;
    jsn["range"] = range_;
    jsn["name"]  = name_;
    jsn["flag"]  = flag_;
    jsn["prop"]  = prop_;

    return jsn;
}
