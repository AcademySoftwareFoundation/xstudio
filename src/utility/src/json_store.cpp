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

    // EMPTY OBJECTS AND ARRAYS become null when flattened..
    // and are not reconstructed..

    try {
        // need to recurse down json and update at leaf..
        auto p = json_pointer(path);

        std::vector<std::pair<json_pointer, nlohmann::json>> fixes;

        // orig flat
        nlohmann::json result = at(p).flatten();
        // changes flat
        nlohmann::json tmp = json.flatten();

        // iterate and capture old value for nulls..
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (it.value().is_null() and not at(json_pointer(it.key())).is_null()) {
                fixes.emplace_back(
                    std::make_pair(json_pointer(it.key()), at(json_pointer(it.key()))));
            }
        }

        // capture new values that are null..
        for (auto it = tmp.begin(); it != tmp.end(); ++it) {
            if (it.value().is_null() and not json.at(json_pointer(it.key())).is_null()) {
                fixes.emplace_back(
                    std::make_pair(json_pointer(it.key()), json.at(json_pointer(it.key()))));
            }
        }

        for (auto it = tmp.begin(); it != tmp.end(); ++it) {
            result[it.key()] = it.value();
        }
        at(p) = result.unflatten();

        // replay null fixes.
        for (const auto &i : fixes) {
            at(i.first) = i.second;
        }

    } catch (const nlohmann::json::out_of_range &) {
        set(json, path);
    }
}

std::string xstudio::utility::to_string(const xstudio::utility::JsonStore &x) {
    return x.dump();
}


void xstudio::utility::to_json(nlohmann::json &j, const JsonStore &c) { j = c.get(""); }

void xstudio::utility::from_json(const nlohmann::json &j, JsonStore &c) { c = JsonStore(j); }
