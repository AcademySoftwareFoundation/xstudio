// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <fstream>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/json_store/json_store_helper.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::global_store;
using namespace xstudio::json_store;
using namespace caf;
namespace fs = std::filesystem;


GlobalStoreHelper::GlobalStoreHelper(caf::actor_system &sys, const std::string &reg_name)
    : JsonStoreHelper(sys, caf::actor()) {
    auto gs_actor = sys.registry().get<caf::actor>(reg_name);
    if (not gs_actor)
        throw std::runtime_error("GlobalStore is not registered");

    store_actor_ = caf::actor_cast<caf::actor_addr>(gs_actor);
}

void GlobalStoreHelper::set(const GlobalStoreDef &gsd, const bool async) {
    JsonStoreHelper::set(
        static_cast<utility::JsonStore>(gsd), static_cast<std::string>(gsd), async);
}

bool GlobalStoreHelper::save(const std::string &context) {
    bool result = false;

    try {
        result = request_receive<bool>(
            *system_, caf::actor_cast<caf::actor>(store_actor_), save_atom_v, context);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool xstudio::global_store::check_preference_path() {
    return check_create_path(preference_path());
}

utility::JsonStore xstudio::global_store::global_store_builder(
    const std::vector<std::string> &paths, const utility::JsonStore &json) {
    // find json and merge non destructively.
    utility::JsonStore merged = json;

    for (const auto &path : paths)
        merged.merge(merge_json_from_path(path));

    return merged;
}

void xstudio::global_store::set_global_store_def(
    utility::JsonStore &js, const GlobalStoreDef &gsd) {
    js.set(static_cast<utility::JsonStore>(gsd), static_cast<std::string>(gsd));
}

bool xstudio::global_store::preference_load_defaults(
    utility::JsonStore &js, const std::string &path) {

    bool result = false;
    try {
        for (const auto &entry : fs::directory_iterator(path)) {
            if (not fs::is_regular_file(entry.status()) or
                not(entry.path().extension() == ".json")) {
                continue;
            }

            try {
                std::ifstream i(entry.path());
                utility::JsonStore j;
                i >> j;
                js.merge(j);
                result = true;
            } catch (const std::exception &e) {
                spdlog::warn(
                    "Failed to merge (some) preferences from path {} with error {}.",
                    entry.path().string(),
                    e.what());
            }
        }
        // set changed to false..
        for (const auto &i : get_preferences(js)) {
            try {
                set_preference_overridden_path(js, path, i);
                js.set(js.get(i + "/value"), i + "/overridden_value");
            } catch (const std::exception &e) {
                spdlog::warn("Failed to set override {} with error {}.", i, e.what());
            }
        }
    } catch (const std::exception &e) {
        spdlog::warn(
            "Failed preference_load_defaults from path {} with error {}.", path, e.what());
    }

    return result;
}

void load_from_list(const std::string &path, std::vector<fs::path> &overrides) {
    // load files from lst.
    // read file...
    try {
        std::string line;
        std::ifstream infile(path.c_str());
        fs::path rpath(fs::canonical(fs::path(path).remove_filename()));

        while (std::getline(infile, line)) {
            try {
                line = trim(line);
                if (not line.empty() and line[0] != '#') {
                    // allow relative ?
                    fs::path tmp(line);
                    if (tmp.is_relative()) {
                        tmp = fs::canonical(rpath / tmp);
                    }

                    if (fs::is_regular_file(tmp) and tmp.extension() == ".json") {
                        overrides.push_back(tmp);
                    } else {
                        spdlog::warn("Invalid pref entry {}", tmp.string());
                    }
                }
            } catch (const std::exception &err) {
                spdlog::warn("Failed to add entry {} {} {}", err.what(), path, line);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("Failed to read {} {}", err.what(), path);
    }
}
// parse json, should be jsonpointers and values..
// parse json, should be jsonpointers and values..
void load_override(utility::JsonStore &json, const fs::path &path) {
    std::ifstream i(path);
    nlohmann::json j;
    i >> j;

    spdlog::debug("load_override {}", path.string());

    // should be dict ..
    for (auto it : j.items()) {
        try {
            if (not ends_with(it.key(), "/value") and not ends_with(it.key(), "/locked")) {
                spdlog::warn("Property key is restricted {} {}", it.key(), path.string());
                continue;
            }

            // check it exists, with throw if not... unless it is a plugin preference,
            // because plugins are loaded after the prefs are built and plugins
            // can insert new preferences at runtime.
            nlohmann::json jj;
            bool set_as_overridden = true;
            try {
                jj = json.get(it.key());
            } catch (...) {

                if (starts_with(it.key(), "/plugin")) {
                    set_as_overridden = false;
                } else if (ends_with(it.key(), "/value")) {
                    try {
                        jj = json.get(it.value()["template_key"]);
                    } catch (...) {
                        std::string err =
                            std::string("Property \"") + it.key() +
                            std::string("\" doesn't exist, and no template specified.");
                        throw std::runtime_error(err.c_str());
                    }
                }
            }

            if (ends_with(it.key(), "/locked") and it.value() != true)
                throw std::runtime_error("Cannot unlock, locked property.");

            std::string property = it.key();
            property.resize(property.find_last_of('/'));

            // override it.
            json.set(it.value(), it.key());

            spdlog::debug(
                "Property overriden {} {} {}", it.key(), to_string(it.value()), path.string());
            // tag it.
            set_preference_overridden_path(json, path.string(), property);
            if (set_as_overridden) json.set(it.value(), property + "/overridden_value");

        } catch (const std::exception &err) {
            spdlog::warn("{} {} {}", err.what(), it.key(), to_string(it.value()));
        }
    }
}
void xstudio::global_store::preference_load_overrides(
    utility::JsonStore &js, const std::vector<std::string> &paths) {
    // we get a collection of JSONPOINTERS and values.
    // apply them and store their origins..
    // probably in the same json doc ?
    // if we get a dir.. look for lst, or read in case order.
    // if we get a json use it.
    // if we get a lst read it.
    std::vector<fs::path> overrides;

    for (const auto &i : paths) {

        try {
            fs::path p(i);
            if (fs::is_regular_file(p)) {
                if (p.extension() == ".json")
                    overrides.push_back(p);
                else if (p.extension() == ".lst")
                    load_from_list(i, overrides);
                else
                    throw std::runtime_error("Unrecognised extension");
            } else if (fs::is_directory(p)) {
                std::set<fs::path> tmp;
                for (const auto &entry : fs::directory_iterator(p)) {
                    if (not fs::is_regular_file(entry.status()) or
                        not(entry.path().extension() == ".json")) {
                        continue;
                    }
                    tmp.insert(entry.path());
                }
                for (const auto &i : tmp)
                    overrides.push_back(i);
            } else {
                throw std::runtime_error(fmt::format("Invalid override path {}", i));
            }
        } catch (const std::exception &err) {
            spdlog::warn("Failed to read preference override {} {}", err.what(), i);
        }
    }

    for (const auto &i : overrides) {
        try {
            load_override(js, i);
        } catch (const std::exception &err) {
            spdlog::warn("Failed to read preference override {} {}", err.what(), i.string());
        }
    }
}


template <class UnaryFunction>
void recursive_iterate_path(
    const nlohmann::json &j, const std::set<std::string> &context, UnaryFunction f) {
    for (const auto &it : j) {
        if (it.is_structured()) {
            if (it.contains("value")) {
                if (context.size()) {
                    if (not it.contains("context"))
                        continue;
                    bool found = false;
                    for (const auto &i : it["context"]) {
                        if (context.count(i)) {
                            found = true;
                            break;
                        }
                    }
                    if (not found)
                        continue;
                }
                f(it["path"]);
            } else {
                recursive_iterate_path(it, context, f);
            }
        }
    }
}

std::set<std::string> xstudio::global_store::get_preferences(
    const utility::JsonStore &js, const std::set<std::string> &context) {
    std::set<std::string> prefs;

    recursive_iterate_path(
        js, context, [&prefs](const std::string &str) { prefs.insert(str); });

    return prefs;
}

utility::JsonStore xstudio::global_store::get_preference_values(
    const utility::JsonStore &json,
    const std::set<std::string> &context,
    const bool only_changed,
    const std::string &override_path) {
    utility::JsonStore js(nlohmann::json({}));
    std::set<std::string> prefs = get_preferences(json, context);

    for (auto i : prefs) {
        try {
            /*
            // For now putting the get(i + "/overridden_value") in a try
            // block, otherwise get spurious errors. Not sure if overridden_value
            // should always be there or not!
            */
            bool is_overridden = false;
            try {
                is_overridden = json.get(i + "/overridden_value") != json.get(i + "/value");
            } catch (...) {
            }

            std::string tmp_path;
            try {
                tmp_path = preference_overridden_path(json, i);
            } catch (...) {
            }


            if (not only_changed or is_overridden or override_path == tmp_path)
                js[i + "/value"] = json.get(i + "/value");

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    return js;
}

void xstudio::global_store::to_json(nlohmann::json &j, const GlobalStoreDef &gsd) { j = gsd; }

void xstudio::global_store::from_json(const nlohmann::json &j, GlobalStoreDef &gsd) {
    j.at("path").get_to(gsd.path_);
    gsd.value_ = j.at("value");
    if (gsd.value_.is_null())
        gsd.value_ = j.at("default_value");
    gsd.default_value_   = j.at("default_value");
    gsd.maximum_value_   = j.at("maximum");
    gsd.minimum_value_   = j.at("minimum");
    gsd.datatype_        = j.value("datatype", "");
    gsd.description_     = j.value("description", "");
    gsd.overridden_path_ = j.value("overridden_path", "");
}


GlobalStore::GlobalStore(const JsonStore &jsn)
    : Container(static_cast<utility::JsonStore>(jsn["container"])),
      preferences_(static_cast<utility::JsonStore>(jsn["store"])) {}

GlobalStore::GlobalStore(const std::string &name) : Container(name, "GlobalStore") {}

JsonStore GlobalStore::serialise() const {
    JsonStore jsn;

    jsn["container"] = Container::serialise();
    jsn["store"]     = preferences_;

    return jsn;
}

/*If a preference is found at path return the value. Otherwise build
a preference at path and return default.*/
utility::JsonStore GlobalStoreHelper::get_existing_or_create_new_preference(
    const std::string &path,
    const utility::JsonStore &default_,
    const bool async,
    const bool broacast_change,
    const std::string &context) 
{
    try {

        utility::JsonStore v = get(path);
        if (!v.contains("overridden_value")) {
            v["overridden_value"] = default_;
            v["path"] = path;
            v["context"] = std::vector<std::string>({"APPLICATION"});
            JsonStoreHelper::set(v, path, async, broacast_change);
        }
        return v["value"];

    } catch (...) {

        utility::JsonStore v;
        v["value"] = default_;
        v["overridden_value"] = default_;
        v["path"] = path;
        v["context"] = std::vector<std::string>({"APPLICATION"});
        JsonStoreHelper::set(v, path, async, broacast_change);
    }
    return default_;

}