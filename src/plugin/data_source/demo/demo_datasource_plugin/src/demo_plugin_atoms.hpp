// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "demo_plugin_enums.hpp"

// In many cases, having custom message data types and custom 'atoms' within
// a plugin is not required but examples here show how to do this if needed.

// If we want to pass custom types into the CAF messaging system we need to
// register the type with CAF like this. The FIRST_CUSTOM_ID+16000 sets the
// starting value for the type id - it must not clash with any types that
// xstudio (or any other plugins) register. So we check (or hope) that nobody
// else has used an offset of 16000
CAF_BEGIN_TYPE_ID_BLOCK(xstudio_demo_plugin, FIRST_CUSTOM_ID + 16000)
CAF_ADD_TYPE_ID(xstudio_demo_plugin, (xstudio::demo_plugin::DATA_MODEL_ROLE))
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, demo_plugin_custom_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, database_entry_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, set_database_value_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, database_row_count_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, database_model_reset_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, new_database_model_instance_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, shot_tree_selection_atom)
CAF_ADD_ATOM(xstudio_demo_plugin, xstudio::demo_plugin, database_record_from_uuid_atom)
CAF_END_TYPE_ID_BLOCK(xstudio_demo_plugin)

// We also require an 'inspect' template function that serialises/deserialises
// the custom data type. This is only really required if the data might be
// transmitted over a network connection. If it's only going to be used within
// the application you can instead use the macro CAF_ALLOW_UNSAFE_MESSAGE_TYPE().
// See atoms.hpp in xSTUDIO source for examples
namespace xstudio::demo_plugin {
template <class Inspector> bool inspect(Inspector &f, DATA_MODEL_ROLE &x) {
    using int_t = std::underlying_type_t<DATA_MODEL_ROLE>;
    auto getter = [&x] { return static_cast<int_t>(x); };
    auto setter = [&x](int_t val) { x = static_cast<DATA_MODEL_ROLE>(val); };
    return f.apply(getter, setter);
}
} // namespace xstudio::demo_plugin
