// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/ui/qml/module_data_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/module/attribute.hpp"

using namespace xstudio;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;

ModulesModelData::ModulesModelData(QObject *parent) : UIModelData(parent) {

    setRoleNames(utility::map_value_to_vec(module::Attribute::role_names));
}
