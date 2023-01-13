// SPDX-License-Identifier: Apache-2.0
#include <algorithm>

#include "xstudio/studio/studio.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::studio;
using namespace xstudio::utility;

Studio::Studio(const std::string &name) : Container(name, "Studio") {}

Studio::Studio(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])) {}

JsonStore Studio::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();

    return jsn;
}
