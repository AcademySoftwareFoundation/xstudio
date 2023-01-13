// SPDX-License-Identifier: Apache-2.0
// #include <iostream>
#include "xstudio/utility/json_store.hpp"

using namespace nlohmann;
using namespace xstudio::utility;

JsonStore::JsonStore(nlohmann::json json) : nlohmann::json(std::move(json)) {}

// JsonStore::JsonStore(const JsonStore &other) : json_(other) {}

nlohmann::json JsonStore::get(const std::string &path) const { return at(json_pointer(path)); }

void JsonStore::set(const nlohmann::json &json, const std::string &path) {
    (*this)[json_pointer(path)] = json;
}

// void JsonStore::set(const JsonStore &json, const std::string &path) { set(json, path); }

bool JsonStore::remove(const std::string &path) {
    try {
        *this = patch(json::parse(
            std::string(R"([{"op": "remove", "path": ")") + path + std::string("\"}]")));
    } catch (...) {
        return false;
    }
    return true;
}

// void JsonStore::merge(const JsonStore &json, const std::string &path) {
//     merge(json, path);
// }
void JsonStore::merge(const nlohmann::json &json, const std::string &path) {
    // merge json at path..
    try {
        // need to recurse down json and update at leaf..
        auto p = json_pointer(path);

        nlohmann::json result = at(p).flatten();
        nlohmann::json tmp    = json.flatten();
        for (auto it = tmp.begin(); it != tmp.end(); ++it)
            result[it.key()] = it.value();
        at(p) = result.unflatten();
    } catch (const nlohmann::json::out_of_range &) {
        set(json, path);
    }
}

std::string xstudio::utility::to_string(const xstudio::utility::JsonStore &x) {
    return x.dump();
}
