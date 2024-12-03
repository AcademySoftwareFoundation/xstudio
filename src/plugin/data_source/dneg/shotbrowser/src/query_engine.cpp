// SPDX-License-Identifier: Apache-2.0

#include "definitions.hpp"
#include "query_engine.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::utility;

namespace {
std::chrono::system_clock::duration duration_since_midnight() {
    auto now = std::chrono::system_clock::now();

    time_t tnow   = std::chrono::system_clock::to_time_t(now);
    tm *date      = std::localtime(&tnow);
    date->tm_hour = 0;
    date->tm_min  = 0;
    date->tm_sec  = 0;
    auto midnight = std::chrono::system_clock::from_time_t(std::mktime(date));

    return now - midnight;
}
}; // namespace

QueryEngine::QueryEngine() {
    set_lookup(cache_name("Completion Location"), utility::JsonStore(locationsJSON), lookup_);
    set_lookup(cache_name("Twig Type"), TwigTypeCodes, lookup_);

    set_cache(cache_name("Twig Type"), TwigTypeCodes);
    set_cache(cache_name("Client Note"), BoolTermValues);
    set_cache(cache_name("Completion Location"), locationsJSON);
    set_cache(cache_name("Flag Media"), FlagTermValues);
    set_cache(cache_name("Has Attachments"), BoolTermValues);
    set_cache(cache_name("Has Contents"), BoolTermValues);
    set_cache(cache_name("Has Notes"), BoolTermValues);
    set_cache(cache_name("Is Hero"), BoolTermValues);
    set_cache(cache_name("Latest Version"), BoolTermValues);
    set_cache(cache_name("Lookback"), LookbackTermValues);
    set_cache(cache_name("On Disk"), OnDiskTermValues);
    set_cache(cache_name("Order By"), OrderByTermValues);
    set_cache(cache_name("Preferred Audio"), SourceTermValues);
    set_cache(cache_name("Preferred Visual"), SourceTermValues);
    set_cache(cache_name("Result Limit"), ResultLimitTermValues);
    set_cache(cache_name("Sent To Client"), BoolTermValues);
    set_cache(cache_name("Sent To"), SubmittedTermValues);
    set_cache(cache_name("Sent To Dailies"), BoolTermValues);
    set_cache(cache_name("Operator"), OperatorTermValues);

    for (const auto &i : ValidTerms.items()) {
        auto key    = std::string("Disable Global-") + i.key();
        auto values = R"([])"_json;
        for (const auto &j : i.value()) {
            auto v    = R"({"name":null})"_json;
            v["name"] = j;
            values.emplace_back(v);
        }
        set_cache(cache_name(key), values);
    }

    initialise_presets();
}


std::optional<nlohmann::json>
QueryEngine::find_by_id(const utility::Uuid &uuid, const utility::JsonStore &presets) {
    if (presets.is_object()) {
        if (presets.count("id") && presets.at("id").get<utility::Uuid>() == uuid)
            return presets;
        else {
            if (presets.count("children")) {
                for (const auto &i : presets.at("children")) {
                    auto r = find_by_id(uuid, i);
                    if (r)
                        return *r;
                }
            }
        }
    } else if (presets.is_array()) {
        for (const auto &i : presets) {
            auto r = find_by_id(uuid, i);
            if (r)
                return *r;
        }
    }

    return {};
}


template <typename T>
T QueryEngine::to_value(const nlohmann::json &jsn, const std::string &key, const T &fallback) {
    auto result = fallback;

    try {
        if (jsn.count(key) and not jsn.at(key).is_null())
            result = jsn.at(key).get<T>();
    } catch (...) {
    }

    return result;
}

void QueryEngine::initialise_presets() {
    auto user_tmp  = RootTemplate;
    user_tmp["id"] = Uuid::generate();
    user_presets_  = JsonStoreSync(user_tmp);

    auto system_tmp  = RootTemplate;
    system_tmp["id"] = Uuid::generate();
    system_presets_  = JsonStoreSync(system_tmp);
}

utility::JsonStore QueryEngine::validate_presets(
    const utility::JsonStore &data,
    const bool is_user,
    const utility::JsonStore &parent,
    const size_t index,
    const bool export_as_system) {
    auto result = R"({})"_json;

    auto type  = data.value("type", std::string());
    auto ptype = parent.is_null() ? std::string() : parent.value("type", std::string());
    if (type == "root") {
        result = RootTemplate;
        result.update(data);

        if (not parent.is_null() and not export_as_system) {
            spdlog::warn("{} parent of root should be null", __PRETTY_FUNCTION__);
        }

        auto new_children = json::array();
        for (size_t i = 0; i < result["children"].size(); i++) {
            auto child =
                validate_presets(result["children"][i], is_user, data, i, export_as_system);
            if (not child.is_null())
                new_children.push_back(child);
        }
        result["children"] = new_children;
    } else if (type == "group") {
        result = GroupTemplate;
        result.update(data);

        if (ptype != "root" and not export_as_system) {
            spdlog::warn(
                "{} parent of group should be root {} {}",
                __PRETTY_FUNCTION__,
                data.value("id", ""),
                ptype);
        }

        if (result["children"].size() != 2 and not export_as_system) {
            spdlog::warn(
                "{} groups must have 2 children {} {}",
                __PRETTY_FUNCTION__,
                data.value("id", ""),
                result["children"].size());
        }

        if (not is_user and (result["update"].is_null() or result["update"] != false)) {
            if (not export_as_system)
                spdlog::warn("Fix group update flag {}", result["name"].get<std::string>());
            result["update"] = false;
        }

        auto userdata = data.value("userdata", std::string());
        if (userdata != "recent" and userdata != "menus" and userdata != "tree" and
            not export_as_system)
            spdlog::warn(
                "{} invalid group userdata {} {}",
                __PRETTY_FUNCTION__,
                data.value("id", ""),
                userdata);

        // check for hidden systen preset == purge when export as system
        if (export_as_system and not is_user and result["hidden"] == true)
            result = JsonStore();
        else {
            for (size_t i = 0; i < result["children"].size(); i++)
                result["children"][i] =
                    validate_presets(result["children"][i], is_user, data, i, export_as_system);
        }
    } else if (type == "presets") {
        result = data;

        if (ptype != "group" and not export_as_system) {
            spdlog::warn(
                "{} parent of presets should be group {} {}",
                __PRETTY_FUNCTION__,
                data.value("id", ""),
                ptype);
        }

        if (index != 1 and not export_as_system) {
            spdlog::warn(
                "{} index of presets must be 1 {} {} {}",
                __PRETTY_FUNCTION__,
                data.value("id", ""),
                index,
                ptype);
        }

        auto new_children = json::array();
        for (size_t i = 0; i < result["children"].size(); i++) {
            auto child =
                validate_presets(result["children"][i], is_user, data, i, export_as_system);
            if (not child.is_null())
                new_children.push_back(child);
        }
        result["children"] = new_children;
    } else if (type == "term") {
        result = TermTemplate;
        result.update(data);

        if (ptype == "term" and parent.value("term", "") != "Operator") {
            if (not export_as_system)
                spdlog::warn(
                    "{} parent of term invalid {} {}",
                    __PRETTY_FUNCTION__,
                    data.value("id", ""),
                    ptype);
        } else if (ptype != "preset" and ptype != "term") {
            if (not export_as_system)
                spdlog::warn(
                    "{} parent of term should be preset {} {}",
                    __PRETTY_FUNCTION__,
                    data.value("id", ""),
                    ptype);
        }

        if (result.value("term", "") != "Operator") {
            if (result.contains("children"))
                result.erase("children");
        } else {
            for (size_t i = 0; i < result["children"].size(); i++)
                result["children"][i] =
                    validate_presets(result["children"][i], is_user, data, i, export_as_system);
        }

    } else if (type == "preset") {
        result = PresetTemplate;
        result.update(data);

        if (ptype != "presets" and ptype != "group") {
            if (not export_as_system)
                spdlog::warn(
                    "{} parent of preset should be presets or group {} {}",
                    __PRETTY_FUNCTION__,
                    data.value("id", ""),
                    ptype);
        }

        if (ptype == "group" and index != 0) {
            if (not export_as_system)
                spdlog::warn(
                    "{} index of group override must be 0 {} {} {}",
                    __PRETTY_FUNCTION__,
                    data.value("id", ""),
                    index,
                    ptype);
        }

        // fix overrides.
        if (ptype == "group") {
            result.update(R"({"hidden": true, "favourite": false})"_json);
        }

        if (not is_user and (result["update"].is_null() or result["update"] != false)) {
            if (not export_as_system)
                spdlog::warn("Fix preset update flag {}", result["name"].get<std::string>());
            result["update"] = false;
        }

        if (export_as_system and not is_user and result["hidden"] == true and ptype != "group")
            result = JsonStore();
        else {
            for (size_t i = 0; i < result["children"].size(); i++)
                result["children"][i] =
                    validate_presets(result["children"][i], is_user, data, i, export_as_system);
        }

    } else {
        result = data;
    }

    return result;
}

void QueryEngine::set_presets(
    const utility::JsonStore &user, const utility::JsonStore &system) {
    // purge children
    nlohmann::json user_tmp = user_presets_.as_json();
    user_tmp["children"]    = user;

    nlohmann::json system_tmp = system_presets_.as_json();
    system_tmp["children"]    = system;

    // fix presets..
    // recursively validate entries.
    system_tmp = validate_presets(system_tmp);

    system_presets_.reset_data(system_tmp);

    // fix presets..
    user_tmp = validate_presets(user_tmp, true);

    // we need to merge system presets into user
    merge_presets(user_tmp["children"], system_tmp["children"]);

    user_presets_.reset_data(user_tmp);
}

// iterate over system preset groups.
void QueryEngine::merge_presets(nlohmann::json &destination, const nlohmann::json &source) {
    for (const auto &i : source) {
        if (i.value("type", "") == "group")
            merge_group(destination, i);
        else
            spdlog::warn("{} Invalid group {}", __PRETTY_FUNCTION__, i.dump(2));
    }
}

void QueryEngine::merge_group(nlohmann::json &destination, const nlohmann::json &source) {
    // does group exist in user_presets..
    try {
        bool group_exists = false;
        auto group_id     = source.value("id", Uuid());

        for (auto &i : destination) {
            if (i.value("type", "") == "group" and i.value("id", Uuid()) == group_id) {
                group_exists = true;

                // update group name / entity?

                // validate group overrides
                if (not i.at("children").at(0).value("update", false) and
                    preset_diff(i.at("children").at(0), source.at("children").at(0))) {
                    // spdlog::warn("overrides differ");
                    // replace content.. as update flag not set by user change.
                    i["children"][0] = source.at("children").at(0);
                }

                // validate group presets. add or update
                std::set<utility::Uuid> found_system_presets;
                for (const auto &gp : source.at("children").at(1).at("children")) {
                    auto preset_id = gp.value("id", Uuid());
                    found_system_presets.insert(preset_id);
                    bool preset_exists = false;

                    for (auto &up : i["children"][1]["children"]) {
                        if (up.value("type", "") == "preset" and
                            up.value("id", Uuid()) == preset_id) {
                            preset_exists = true;

                            // validate group preset
                            if (not up.value("update", false) and preset_diff(up, gp)) {
                                // replace content.. as update flag not set by user change.
                                up["name"]     = gp.at("name");
                                up["children"] = gp.at("children");
                            }
                        }
                    }

                    if (not preset_exists)
                        i["children"][1]["children"].push_back(gp);
                }

                // do a reverse validation.
                // if the user has a system preset that hasn't been modified and doesn't exist
                // in this list.. but this will then break project presets..
                auto tmp = R"([])"_json;

                for (const auto &up : i["children"][1]["children"]) {
                    // spdlog::warn("checking {}", up.at("name").get<std::string>());
                    if (up.contains("update") and not up.at("update").is_null() and
                        up.at("update") == false) {
                        // is system preset, check it's in source.
                        if (found_system_presets.count(up.value("id", Uuid())))
                            tmp.push_back(up);
                        else
                            spdlog::debug(
                                "Removing retired system preset {}",
                                up.at("name").get<std::string>());
                    } else {
                        // orpahaned system presets should change to user presets.
                        if (up.contains("update") and not up.at("update").is_null() and
                            up.at("update") == true and
                            not found_system_presets.count(up.value("id", Uuid()))) {

                            auto tup      = up;
                            tup["update"] = nullptr;
                            tmp.push_back(tup);
                        } else
                            tmp.push_back(up);
                    }
                }


                i["children"][1]["children"] = tmp;
            }
        }

        if (not group_exists) {
            // spdlog::warn("Add new group {}", system_group.value("name", ""));
            destination.push_back(source);
        }
    } catch (const std::exception &err) {
        spdlog::warn(
            "{} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            destination.dump(2),
            source.dump(2));
    }
}

std::set<std::string>
QueryEngine::precache_needed(const int project_id, const utility::JsonStore &lookup) {
    // check expected keys exist..
    auto result = std::set<std::string>();

    static const auto lookup_names = std::vector<std::string>({
        "Project",
        "Department",
        "Pipeline Status",
        "Production Status",
        "Sequence Status",
        "Shot Status",
    });

    static const auto lookup_project_names =
        std::vector<std::string>({"User", "Unit", "Stage", "Playlist", "Shot", "Sequence"});

    for (const auto &i : lookup_names) {
        if (not lookup.count(cache_name(i)))
            result.insert(i);
    }

    if (project_id > 0) {
        for (const auto &i : lookup_project_names) {
            if (not lookup.count(cache_name(i, project_id)))
                result.insert(i);
        }
    }

    return result;
}

void QueryEngine::set_shot_sequence_list_cache(
    const std::string &key, const utility::JsonStore &data, utility::JsonStore &cache) {

    auto result = R"([])"_json;

    try {
        auto row = R"({"id": null, "name": null, "type": null})"_json;
        for (const auto &i : data) {
            row["id"]   = i.at("id");
            row["type"] = i.at("type");
            row["name"] = i.at(json::json_pointer("/attributes/code"));
            result.push_back(row);

            for (const auto &s : i.at(json::json_pointer("/relationships/shots/data"))) {
                row["id"]   = s.at("id");
                row["type"] = s.at("type");
                row["name"] = s.at("name");

                result.push_back(row);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    // spdlog::warn("{} {}", key, result.dump(2));

    cache[key] = result;
}

void QueryEngine::set_shot_sequence_list_cache(
    const std::string &key, const utility::JsonStore &data) {
    set_shot_sequence_list_cache(key, data, cache_);
    if (cache_changed_callback_)
        cache_changed_callback_(key);
}

bool QueryEngine::preset_diff(const nlohmann::json &a, const nlohmann::json &b) {
    auto result = true;

    try { // term count check (quick)
        if (a.at("name") == b.at("name") and
            a.at("children").size() == b.at("children").size()) {
            // term comparison..
            bool mismatch = false;

            for (const auto &ta : a.at("children")) {
                auto ta_id = ta.value("id", Uuid());
                mismatch   = true;

                for (const auto &tb : b.at("children")) {
                    if (tb.value("id", Uuid()) == ta_id) {
                        if (ta.at("term") == tb.at("term") and
                            ta.at("negated") == tb.at("negated") and
                            ta.at("enabled") == tb.at("enabled") and
                            ta.at("livelink") == tb.at("livelink") and
                            (ta.at("value") == tb.at("value") or
                             (not ta.at("livelink").is_null() and ta.value("livelink", false))))
                            mismatch = false;

                        break;
                    }
                }

                if (mismatch)
                    break;
            }

            if (not mismatch)
                result = false;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

shotgun_client::FilterBy QueryEngine::terms_to_query(
    const JsonStore &terms,
    const int project_id,
    const std::string &entity,
    const utility::JsonStore &lookup,
    const bool and_mode,
    const bool initial) {

    auto result = FilterBy(BoolOperator::AND);

    if (initial) {
        // add terms we always want.
        result.emplace_back(Number("project.Project.id").is(project_id));

        if (entity == "Playlists") {
        } else if (entity == "Versions") {
            result.emplace_back(Text("sg_deleted").is_null());
            result.emplace_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
        } else if (entity == "Notes") {
        }
    } else if (not and_mode) {
        result = FilterBy(BoolOperator::OR);
    }

    for (const auto &i : terms) {
        if (i.at("term") == "Operator") {
            result.emplace_back(terms_to_query(
                i.at("children"), project_id, entity, lookup, i.at("value") == "And", false));
        } else {
            try {
                add_term_to_filter(entity, i, project_id, lookup, &result);
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), i.dump(2));
            }
        }
    }

    return result;
}

utility::JsonStore QueryEngine::apply_livelinks(
    const utility::JsonStore &terms,
    const utility::JsonStore &metadata,
    const utility::JsonStore &lookup) {

    auto result = utility::JsonStore(R"([])"_json);

    // apply livelinks..
    // update livelink data
    for (auto &i : terms) {
        auto t = i;
        if (t.at("term") == "Operator") {
            t["children"] = apply_livelinks(t["children"], metadata, lookup);
        } else if (to_value(t, "livelink", false)) {
            auto linkvalue = get_livelink_value(t.value("term", ""), metadata, lookup);
            if (not linkvalue.is_null() and t["value"] != linkvalue)
                t["value"] = linkvalue;
        }
        result.emplace_back(t);
    }

    return result;
}

utility::JsonStore QueryEngine::get_default_env() {
    utility::JsonStore result(R"({})"_json);

    result["USER"]                  = get_login_name();
    result["USERFULLNAME"]          = get_user_name();
    result["DNSITEDATA_SHORT_NAME"] = get_env("DNSITEDATA_SHORT_NAME", "");

    return result;
}

void QueryEngine::replace_with_env(
    nlohmann::json &value, const utility::JsonStore &env, const utility::JsonStore &lookup) {
    const static std::regex env_regex(R"(\$\{.+?\})");

    if (value.is_string()) {
        auto s       = value.get<std::string>();
        auto done    = false;
        auto env_end = std::sregex_iterator();

        while (true) {
            auto env_begin = std::sregex_iterator(s.begin(), s.end(), env_regex);
            if (env_begin != env_end) {
                std::smatch match = *env_begin;
                auto key          = match.str().substr(2, match.str().size() - 3);
                replace_string_in_place(
                    s, match.str(), QueryEngine::to_value(env, key, std::string("")));
            } else
                break;
        }
        if (value.get<std::string>() != s)
            value = s;
    } else if (value.is_array()) {
        // spdlog::warn("{}",value.dump(2));
    }
}

utility::JsonStore QueryEngine::apply_env(
    const utility::JsonStore &terms,
    const utility::JsonStore &env,
    const utility::JsonStore &lookup) {

    auto result = utility::JsonStore(R"([])"_json);

    // apply livelinks..
    // update livelink data
    for (auto &i : terms) {
        auto t = i;
        if (t.at("term") == "Operator") {
            t["children"] = apply_env(t["children"], env, lookup);
        } else if (t.count("value") and not to_value(t, "livelink", false)) {
            // check for env var in value field.
            // substitue, need regexp search replace.
            replace_with_env(t.at("value"), env, lookup);
        }
        result.emplace_back(t);
    }

    return result;
}

// handle expansion into OR/AND
utility::JsonStore QueryEngine::preprocess_terms(
    const utility::JsonStore &terms,
    const std::string &entity,
    utility::JsonStore &query,
    const utility::JsonStore &lookup,
    const utility::JsonStore &metadata,
    const bool and_mode,
    const bool initial) {
    auto result = utility::JsonStore(R"([])"_json);

    try {

        // special handling for top level.
        std::map<std::string, std::vector<utility::JsonStore>> dup_terms;
        std::vector<std::string> order_by;

        if (initial) {
            query["entity"] = entity;
            if (entity == "Versions")
                query["fields"] = VersionFields;
            else if (entity == "Notes")
                query["fields"] = NoteFields;
            else if (entity == "Playlists")
                query["fields"] = PlaylistFields;
        }

        // duplicate term if array.
        auto expanded = R"([])"_json;
        for (const auto &i : terms) {
            if (i.at("value").is_array()) {
                auto tmp = i;
                for (const auto &ii : i.at("value")) {
                    tmp["value"] = ii;
                    expanded.push_back(tmp);
                }
            } else {
                expanded.push_back(i);
            }
        }

        for (const auto &i : expanded) {
            if (i.value("enabled", true)) {
                auto term = i.value("term", "");
                if (term == "Operator") {
                    auto op        = i;
                    op["children"] = preprocess_terms(
                        i.at("children"),
                        entity,
                        query,
                        lookup,
                        metadata,
                        i.value("value", "And") == "And",
                        false);
                    result.push_back(op);
                } else if (term == "Disable Global") {
                    // ignored
                } else if (term == "Result Limit") {
                    query["max_result"] = std::stoi(i.at("value").get<std::string>());
                } else if (term == "Project") {
                    // need to also handle livelinked project...
                    if (not i.contains("livelink") or not i.value("livelink", false))
                        query["project_id"] =
                            resolve_query_value("Project", i.at("value"), lookup).get<int>();
                    else {
                        auto linkvalue = get_livelink_value("Project", metadata, lookup);
                        if (not linkvalue.is_null() and i.at("value") != linkvalue)
                            query["project_id"] =
                                resolve_query_value("Project", linkvalue, lookup).get<int>();
                    }
                } else if (term == "Preferred Visual") {
                    query["context"]["visual_source"].push_back(
                        i.at("value").get<std::string>());
                } else if (term == "Preferred Audio") {
                    query["context"]["audio_source"].push_back(
                        i.at("value").get<std::string>());
                } else if (term == "Flag Media") {
                    auto flag_text                = i.at("value").get<std::string>();
                    query["context"]["flag_text"] = flag_text;
                    if (flag_text == "Red")
                        query["context"]["flag_colour"] = "#FFFF0000";
                    else if (flag_text == "Green")
                        query["context"]["flag_colour"] = "#FF00FF00";
                    else if (flag_text == "Blue")
                        query["context"]["flag_colour"] = "#FF0000FF";
                    else if (flag_text == "Yellow")
                        query["context"]["flag_colour"] = "#FFFFFF00";
                    else if (flag_text == "Orange")
                        query["context"]["flag_colour"] = "#FFFFA500";
                    else if (flag_text == "Purple")
                        query["context"]["flag_colour"] = "#FF800080";
                    else if (flag_text == "Black")
                        query["context"]["flag_colour"] = "#FF000000";
                    else if (flag_text == "White")
                        query["context"]["flag_colour"] = "#FFFFFFFF";
                } else if (term == "Order By") {
                    auto val        = i.at("value").get<std::string>();
                    bool descending = false;

                    if (ends_with(val, " ASC")) {
                        val = val.substr(0, val.size() - 4);
                    } else if (ends_with(val, " DESC")) {
                        val        = val.substr(0, val.size() - 5);
                        descending = true;
                    }

                    std::string field = "";
                    // get sg term..
                    if (entity == "Playlists") {
                        if (val == "Date And Time")
                            field = "sg_date_and_time";
                        else if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                    } else if (entity == "Versions") {
                        if (val == "Date And Time")
                            field = "created_at";
                        else if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                        else if (val == "Client Submit")
                            field = "sg_date_submitted_to_client";
                        else if (val == "Version")
                            field = "sg_dneg_version";
                    } else if (entity == "Notes") {
                        if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                    }

                    if (not field.empty())
                        order_by.push_back(descending ? "-" + field : field);
                } else {
                    // add normal term to map.
                    auto key = std::string(to_value(i, "negated", false) ? "Not " : "") + term;
                    if (not dup_terms.count(key))
                        dup_terms[key] = std::vector<utility::JsonStore>();

                    dup_terms[key].push_back(i);
                }
            }
        }

        // we've got list of terms in multi map
        // we now need to encapsulate in OR/AND groups.
        for (const auto &i : dup_terms) {
            if (i.second.size() == 1)
                result.push_back(i.second.front());
            else {
                if (i.first == "Operator" or not initial) {
                    for (const auto &j : i.second)
                        result.push_back(j);
                } else {
                    auto mode = "";
                    auto inverted =
                        starts_with(i.first, "Not ") or starts_with(i.first, "Exclude ");
                    if (and_mode) {
                        if (inverted) {
                            mode = "And";
                        } else {
                            mode = "Or";
                        }
                    } else {
                        // not sure if it should be inverted ?
                        if (inverted) {
                            mode = "And";
                        } else {
                            mode = "Or";
                        }
                    }
                    auto op        = JsonStore(OperatorTermTemplate);
                    op["value"]    = mode;
                    op["children"] = i.second;
                    result.emplace_back(op);
                }
            }
        }

        if (initial) {
            // set defaults if not specified
            if (query["context"]["visual_source"].empty())
                query["context"]["visual_source"] = json::array({"SG Movie"});
            if (query["context"]["audio_source"].empty())
                query["context"]["audio_source"] = query["context"]["visual_source"];

            // set order by
            if (order_by.empty()) {
                order_by.emplace_back("-created_at");
            }

            query["order"] = order_by;
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        throw;
    }

    return result;
}

utility::JsonStore QueryEngine::build_query(
    const int project_id,
    const std::string &entity,
    const utility::Uuid &group_id,
    const utility::JsonStore &group_terms,
    const utility::JsonStore &terms,
    const utility::JsonStore &custom_terms,
    const utility::JsonStore &context,
    const utility::JsonStore &metadata,
    const utility::JsonStore &env,
    const utility::JsonStore &lookup) {
    auto query                    = utility::JsonStore(GetQueryResult);
    static const auto default_env = get_default_env();

    query["group_id"]   = group_id;
    query["context"]    = context;
    query["env"]        = default_env;
    query["project_id"] = project_id;
    if (env.is_object())
        query["env"].update(env);

    auto merged_preset = utility::JsonStore(R"([])"_json);
    for (size_t i = 0; i < terms.size(); i++)
        merged_preset = merge_query(merged_preset, terms.at(i), false);

    merged_preset = merge_query(merged_preset, group_terms, true);

    merged_preset = merge_query(merged_preset, custom_terms, false);

    merged_preset = apply_env(merged_preset, query["env"], lookup);

    merged_preset = apply_livelinks(merged_preset, metadata, lookup);

    // spdlog::warn("merged_preset\n{}", merged_preset.dump(2));

    auto preprocessed = JsonStore(R"([])"_json);
    preprocessed.push_back(OperatorTermTemplate);
    preprocessed[0]["value"] = "And";
    preprocessed[0]["children"] =
        preprocess_terms(merged_preset, entity, query, lookup, metadata, true, true);

    // spdlog::warn("preprocess_terms\n{}", preprocessed.dump(2));

    try {
        query["query"] = terms_to_query(preprocessed, query["project_id"], entity, lookup);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        throw;
    }

    // spdlog::warn("terms_to_query {}", query.dump(2));

    return query;
}

utility::JsonStore QueryEngine::merge_query(
    const utility::JsonStore &base,
    const utility::JsonStore &override,
    const bool ignore_duplicates) {
    auto result = base;

    // we need to preprocess for Disable Global flags..
    auto disable_globals = std::set<std::string>();

    try {
        for (const auto &i : result) {
            if (i.at("enabled").get<bool>() and i.at("term") == "Disable Global")
                disable_globals.insert(i.at("value").get<std::string>());
        }

        // if term already exists in dst, then don't append.
        if (ignore_duplicates) {
            auto dup = std::set<std::string>();
            for (const auto &i : result)
                if (i.at("enabled").get<bool>())
                    dup.insert(i.at("term").get<std::string>());

            for (const auto &i : override) {
                auto term = i.at("term").get<std::string>();
                if (not dup.count(term) and not disable_globals.count(term))
                    result.push_back(i);
            }
        } else {
            for (const auto &i : override) {
                auto term = i.at("term").get<std::string>();
                if (not disable_globals.count(term))
                    result.push_back(i);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        throw;
    }

    return result;
}

Text QueryEngine::add_text_value(
    const std::string &filter, const std::string &value, const bool negated) {
    if (starts_with(value, "^") and ends_with(value, "$")) {
        if (negated)
            return Text(filter).is_not(value.substr(0, value.size() - 1).substr(1));

        return Text(filter).is(value.substr(0, value.size() - 1).substr(1));
    } else if (ends_with(value, "$")) {
        return Text(filter).ends_with(value.substr(0, value.size() - 1));
    } else if (starts_with(value, "^")) {
        return Text(filter).starts_with(value.substr(1));
    }
    if (negated)
        return Text(filter).not_contains(value);

    return Text(filter).contains(value);
}

void QueryEngine::add_playlist_term_to_filter(
    const std::string &term,
    const std::string &value,
    const bool negated,
    const int project_id,
    const JsonStore &lookup,
    FilterBy *qry) {
    if (term == "Lookback") {
        if (value == "Today") {
            auto since_midnight = duration_since_midnight();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(since_midnight).count();
            qry->push_back(DateTime("updated_at").in_last(hours, Period::HOUR));
        } else if (value == "1 Day")
            qry->push_back(DateTime("updated_at").in_last(1, Period::DAY));
        else if (value == "3 Days")
            qry->push_back(DateTime("updated_at").in_last(3, Period::DAY));
        else if (value == "7 Days")
            qry->push_back(DateTime("updated_at").in_last(7, Period::DAY));
        else if (value == "20 Days")
            qry->push_back(DateTime("updated_at").in_last(20, Period::DAY));
        else if (value == "30 Days")
            qry->push_back(DateTime("updated_at").in_last(30, Period::DAY));
        else if (value == "30-60 Days") {
            qry->push_back(DateTime("updated_at").not_in_last(30, Period::DAY));
            qry->push_back(DateTime("updated_at").in_last(60, Period::DAY));
        } else if (value == "60-90 Days") {
            qry->push_back(DateTime("updated_at").not_in_last(60, Period::DAY));
            qry->push_back(DateTime("updated_at").in_last(90, Period::DAY));
        } else if (value == "100-150 Days") {
            qry->push_back(DateTime("updated_at").not_in_last(100, Period::DAY));
            qry->push_back(DateTime("updated_at").in_last(150, Period::DAY));
        } else if (value == "Future Only") {
            qry->push_back(DateTime("sg_date_and_time").in_next(30, Period::DAY));
        } else {
            throw XStudioError("Invalid query term " + term + " " + value);
        }
    } else if (term == "Playlist Type") {
        if (negated)
            qry->push_back(Text("sg_type").is_not(value));
        else
            qry->push_back(Text("sg_type").is(value));
    } else if (term == "Has Contents") {
        if (value == "False")
            qry->push_back(Text("versions").is_null());
        else if (value == "True")
            qry->push_back(Text("versions").is_not_null());
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Site") {
        if (negated)
            qry->push_back(Text("sg_location").is_not(value));
        else
            qry->push_back(Text("sg_location").is(value));
    } else if (term == "Review Location") {
        if (negated)
            qry->push_back(Text("sg_review_location_1").is_not(value));
        else
            qry->push_back(Text("sg_review_location_1").is(value));
    } else if (term == "Department") {
        if (negated)
            qry->push_back(
                Number("sg_department_unit.Department.id")
                    .is_not(resolve_query_value(term, JsonStore(value), lookup).get<int>()));
        else
            qry->push_back(
                Number("sg_department_unit.Department.id")
                    .is(resolve_query_value(term, JsonStore(value), lookup).get<int>()));
    } else if (term == "Author") {
        qry->push_back(Number("created_by.HumanUser.id")
                           .is(resolve_query_value(term, JsonStore(value), project_id, lookup)
                                   .get<int>()));
    } else if (term == "Id") {
        if (negated)
            qry->push_back(Number("id").is_not(std::stoi(value)));
        else
            qry->push_back(Number("id").is(std::stoi(value)));
    } else if (term == "Filter") {
        qry->push_back(QueryEngine::add_text_value("code", value, negated));
    } else if (term == "Tag") {
        qry->push_back(QueryEngine::add_text_value("tags.Tag.name", value, negated));
    } else if (term == "Has Notes") {
        if (value == "False")
            qry->push_back(Text("notes").is_null());
        else if (value == "True")
            qry->push_back(Text("notes").is_not_null());
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Unit") {
        auto tmp  = R"({"type": "CustomEntity24", "id":0})"_json;
        tmp["id"] = resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        if (negated)
            qry->push_back(RelationType("sg_unit2").in({JsonStore(tmp)}));
        else
            qry->push_back(RelationType("sg_unit2").not_in({JsonStore(tmp)}));
    } else {
        spdlog::warn("{} Unhandled Term {}", __PRETTY_FUNCTION__, term);
    }
}

void QueryEngine::add_version_term_to_filter(
    const std::string &term,
    const std::string &value,
    const bool negated,
    const int project_id,
    const JsonStore &lookup,
    FilterBy *qry) {
    if (term == "Lookback") {
        if (value == "Today") {
            auto since_midnight = duration_since_midnight();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(since_midnight).count();
            qry->push_back(DateTime("created_at").in_last(hours, Period::HOUR));
        } else if (value == "1 Day")
            qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
        else if (value == "3 Days")
            qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
        else if (value == "7 Days")
            qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
        else if (value == "20 Days")
            qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
        else if (value == "30 Days")
            qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
        else if (value == "30-60 Days") {
            qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
        } else if (value == "60-90 Days") {
            qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
        } else if (value == "100-150 Days") {
            qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
        } else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Playlist") {
        auto tmp  = R"({"type": "Playlist", "id":0})"_json;
        tmp["id"] = resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        qry->push_back(RelationType("playlists").in({JsonStore(tmp)}));
    } else if (term == "Author") {
        qry->push_back(Number("created_by.HumanUser.id")
                           .is(resolve_query_value(term, JsonStore(value), project_id, lookup)
                                   .get<int>()));
    } else if (term == "Older Version") {
        qry->push_back(Number("sg_dneg_version").less_than(std::stoi(value)));
    } else if (term == "Newer Version") {
        qry->push_back(Number("sg_dneg_version").greater_than(std::stoi(value)));
    } else if (term == "Id") {
        if (negated)
            qry->push_back(Number("id").is_not(std::stoi(value)));
        else
            qry->push_back(Number("id").is(std::stoi(value)));
    } else if (term == "Site") {
        if (negated)
            qry->push_back(Text("sg_location").is_not(value));
        else
            qry->push_back(Text("sg_location").is(value));
    } else if (term == "On Disk") {
        std::string prop = std::string("sg_on_disk_") + value;
        if (negated)
            qry->push_back(Text(prop).is("None"));
        else
            qry->push_back(FilterBy().Or(Text(prop).is("Full"), Text(prop).is("Partial")));
    } else if (term == "Pipeline Step") {
        if (negated) {
            if (value == "None")
                qry->push_back(Text("sg_pipeline_step").is_not_null());
            else
                qry->push_back(Text("sg_pipeline_step").is_not(value));
        } else {
            if (value == "None")
                qry->push_back(Text("sg_pipeline_step").is_null());
            else
                qry->push_back(Text("sg_pipeline_step").is(value));
        }
    } else if (term == "Pipeline Status") {
        if (negated)
            qry->push_back(Text("sg_status_list")
                               .is_not(resolve_query_value(term, JsonStore(value), lookup)
                                           .get<std::string>()));
        else
            qry->push_back(Text("sg_status_list")
                               .is(resolve_query_value(term, JsonStore(value), lookup)
                                       .get<std::string>()));
    } else if (term == "Production Status") {
        if (negated)
            qry->push_back(Text("sg_production_status")
                               .is_not(resolve_query_value(term, JsonStore(value), lookup)
                                           .get<std::string>()));
        else
            qry->push_back(Text("sg_production_status")
                               .is(resolve_query_value(term, JsonStore(value), lookup)
                                       .get<std::string>()));
    } else if (term == "Unit") {
        auto tmp  = R"({"type": "CustomEntity24", "id":0})"_json;
        tmp["id"] = resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        if (negated)
            qry->push_back(RelationType("entity.Shot.sg_unit").is_not({JsonStore(tmp)}));
        else
            qry->push_back(RelationType("entity.Shot.sg_unit").is({JsonStore(tmp)}));

    } else if (term == "Stage") {
        qry->push_back(RelationType("sg_client_send_stage").name_is(value));
    } else if (term == "Shot Status") {
        if (negated)
            qry->push_back(Text("entity.Shot.sg_status_list")
                               .is_not(resolve_query_value(term, JsonStore(value), lookup)
                                           .get<std::string>()));
        else
            qry->push_back(Text("entity.Shot.sg_status_list")
                               .is(resolve_query_value(term, JsonStore(value), lookup)
                                       .get<std::string>()));
    } else if (term == "Exclude Shot Status") {
        qry->push_back(
            Text("entity.Shot.sg_status_list")
                .is_not(
                    resolve_query_value(term, JsonStore(value), lookup).get<std::string>()));
    } else if (term == "Latest Version") {
        if (value == "False")
            qry->push_back(Text("sg_latest").is_null());
        else if (value == "True")
            qry->push_back(Text("sg_latest").is("Yes"));
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Is Hero") {
        if (value == "False")
            qry->push_back(Checkbox("sg_is_hero").is(false));
        else if (value == "True")
            qry->push_back(Checkbox("sg_is_hero").is(true));
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Entity") {
        auto args = split(value, '-');
        if (args.size() == 2) {
            auto rel    = R"({"type": null, "id":null})"_json;
            rel["type"] = args.at(0);
            rel["id"]   = std::stoi(args.at(1));
            qry->push_back(RelationType("entity").is(JsonStore(rel)));
        }
    } else if (term == "Shot") {
        auto rel = R"({"type": "Shot", "id":0})"_json;
        // if no match force failing query. or we'll get EVERYTHING
        try {
            rel["id"] =
                resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        } catch (...) {
        }
        qry->push_back(RelationType("entity").is(JsonStore(rel)));
    } else if (term == "Sequence") {
        try {
            auto seq  = R"({"type": "Sequence", "id":0})"_json;
            auto seqs = std::vector<JsonStore>();
            // if no match force failing query. or we'll get EVERYTHING
            try {
                seq["id"] =
                    resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
                seqs.push_back(seq);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                seqs.push_back(seq);
            }
            qry->push_back(RelationType("entity").in(seqs));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw XStudioError("Invalid query term " + term + " " + value);
        }
    } else if (term == "Sent To") {
        if (value == "Client")
            qry->push_back(DateTime("sg_date_submitted_to_client").is_not_null());
        else if (value == "Dailies") {
            qry->push_back(FilterBy().Or(
                DateTime("sg_submit_dailies").is_not_null(),
                DateTime("sg_submit_dailies_chn").is_not_null(),
                DateTime("sg_submit_dailies_mtl").is_not_null(),
                DateTime("sg_submit_dailies_van").is_not_null(),
                DateTime("sg_submit_dailies_mum").is_not_null()));
        } else if (value == "Any") {
            qry->push_back(FilterBy().Or(
                DateTime("sg_date_submitted_to_client").is_not_null(),
                DateTime("sg_submit_dailies").is_not_null(),
                DateTime("sg_submit_dailies_chn").is_not_null(),
                DateTime("sg_submit_dailies_mtl").is_not_null(),
                DateTime("sg_submit_dailies_van").is_not_null(),
                DateTime("sg_submit_dailies_mum").is_not_null()));
        }
    } else if (term == "Sent To Client") {
        if (value == "False")
            qry->push_back(DateTime("sg_date_submitted_to_client").is_null());
        else if (value == "True")
            qry->push_back(DateTime("sg_date_submitted_to_client").is_not_null());
        else
            throw XStudioError("Invalid query term " + term + " " + value);


    } else if (term == "Sent To Dailies") {
        if (value == "False")
            qry->push_back(FilterBy().And(
                DateTime("sg_submit_dailies").is_null(),
                DateTime("sg_submit_dailies_chn").is_null(),
                DateTime("sg_submit_dailies_mtl").is_null(),
                DateTime("sg_submit_dailies_van").is_null(),
                DateTime("sg_submit_dailies_mum").is_null()));
        else if (value == "True")
            qry->push_back(FilterBy().Or(
                DateTime("sg_submit_dailies").is_not_null(),
                DateTime("sg_submit_dailies_chn").is_not_null(),
                DateTime("sg_submit_dailies_mtl").is_not_null(),
                DateTime("sg_submit_dailies_van").is_not_null(),
                DateTime("sg_submit_dailies_mum").is_not_null()));
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Has Notes") {
        if (value == "False")
            qry->push_back(Text("notes").is_null());
        else if (value == "True")
            qry->push_back(Text("notes").is_not_null());
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Filter") {
        qry->push_back(QueryEngine::add_text_value("code", value, negated));
    } else if (term == "Client Filename") {
        qry->push_back(QueryEngine::add_text_value("sg_client_filename", value, negated));
    } else if (term == "Tag") {
        qry->push_back(
            QueryEngine::add_text_value("entity.Shot.tags.Tag.name", value, negated));
    } else if (term == "dnTag") {
        qry->push_back(QueryEngine::add_text_value(
            "entity.Shot.sg_dnbreakdown_tags.Tag.name", value, negated));
    } else if (term == "Reference Tag" or term == "Reference Tags") {

        if (value.find(',') != std::string::npos) {
            // split ...
            for (const auto &i : split(value, ',')) {
                if (negated)
                    qry->push_back(RelationType("tags").name_not_contains(i + ".REFERENCE"));
                else
                    qry->push_back(RelationType("tags").name_is(i + ".REFERENCE"));
            }
        } else {
            if (negated)
                qry->push_back(RelationType("tags").name_not_contains(value + ".REFERENCE"));
            else
                qry->push_back(RelationType("tags").name_is(value + ".REFERENCE"));
        }
    } else if (term == "Tag (Version)") {
        qry->push_back(QueryEngine::add_text_value("tags.Tag.name", value, negated));
    } else if (term == "Twig Name") {
        qry->push_back(QueryEngine::add_text_value("sg_twig_name", value, negated));
    } else if (term == "Twig Type") {
        if (negated)
            qry->push_back(Text("sg_twig_type_code")
                               .is_not(resolve_query_value(term, JsonStore(value), lookup)
                                           .get<std::string>()));
        else
            qry->push_back(Text("sg_twig_type_code")
                               .is(resolve_query_value(term, JsonStore(value), lookup)
                                       .get<std::string>()));
    } else if (term == "Completion Location") {
        auto rel  = R"({"type": "CustomNonProjectEntity16", "id":0})"_json;
        rel["id"] = resolve_query_value(term, JsonStore(value), lookup).get<int>();
        if (negated)
            qry->push_back(
                RelationType("entity.Shot.sg_primary_shot_location").is_not(JsonStore(rel)));
        else
            qry->push_back(
                RelationType("entity.Shot.sg_primary_shot_location").is(JsonStore(rel)));

    } else {
        spdlog::warn("{} Unhandled Term {}", __PRETTY_FUNCTION__, term);
    }
}

void QueryEngine::add_note_term_to_filter(
    const std::string &term,
    const std::string &value,
    const bool negated,
    const int project_id,
    const JsonStore &lookup,
    FilterBy *qry) {
    if (term == "Lookback") {
        if (value == "Today") {
            auto since_midnight = duration_since_midnight();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(since_midnight).count();
            qry->push_back(DateTime("created_at").in_last(hours, Period::HOUR));
        } else if (value == "1 Day")
            qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
        else if (value == "3 Days")
            qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
        else if (value == "7 Days")
            qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
        else if (value == "20 Days")
            qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
        else if (value == "30 Days")
            qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
        else if (value == "30-60 Days") {
            qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
        } else if (value == "60-90 Days") {
            qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
        } else if (value == "100-150 Days") {
            qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
            qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
        } else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Filter") {
        qry->push_back(QueryEngine::add_text_value("subject", value, negated));
    } else if (term == "Note Type") {
        if (negated)
            qry->push_back(Text("sg_note_type").is_not(value));
        else
            qry->push_back(Text("sg_note_type").is(value));
    } else if (term == "Author") {
        qry->push_back(Number("created_by.HumanUser.id")
                           .is(resolve_query_value(term, JsonStore(value), project_id, lookup)
                                   .get<int>()));
    } else if (term == "Id") {
        if (negated)
            qry->push_back(Number("id").is_not(std::stoi(value)));
        else
            qry->push_back(Number("id").is(std::stoi(value)));
    } else if (term == "Recipient") {
        auto tmp  = R"({"type": "HumanUser", "id":0})"_json;
        tmp["id"] = resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        qry->push_back(RelationType("addressings_to").in({JsonStore(tmp)}));
    } else if (term == "Shot") {
        auto tmp = R"({"type": "Shot", "id":0})"_json;
        // if no match force failing query. or we'll get EVERYTHING
        try {
            tmp["id"] =
                resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        } catch (...) {
        }

        qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
    } else if (term == "Sequence") {
        try {
            auto seq  = R"({"type": "Sequence", "id":0})"_json;
            auto seqs = std::vector<JsonStore>();
            // if no match force failing query. or we'll get EVERYTHING
            try {
                seq["id"] =
                    resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
                seqs.push_back(seq);
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                seqs.push_back(seq);
            }
            qry->push_back(RelationType("note_links").in(seqs));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            throw XStudioError("Invalid query term " + term + " " + value);
        }
    } else if (term == "Playlist") {
        auto tmp  = R"({"type": "Playlist", "id":0})"_json;
        tmp["id"] = resolve_query_value(term, JsonStore(value), project_id, lookup).get<int>();
        qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
    } else if (term == "Version Name") {
        qry->push_back(QueryEngine::add_text_value("note_links.Version.code", value, negated));
    } else if (term == "Tag") {
        qry->push_back(QueryEngine::add_text_value("tags.Tag.name", value, negated));
    } else if (term == "Has Attachments") {
        if (value == "False")
            qry->push_back(Text("attachments").is_null());
        else if (value == "True")
            qry->push_back(Text("attachments").is_not_null());
        else
            throw XStudioError("Invalid query term " + term + " " + value);
    } else if (term == "Twig Type") {
        if (negated)
            qry->push_back(Text("note_links.Version.sg_twig_type_code")
                               .is_not(resolve_query_value(term, JsonStore(value), lookup)
                                           .get<std::string>()));
        else
            qry->push_back(Text("note_links.Version.sg_twig_type_code")
                               .is(resolve_query_value(term, JsonStore(value), lookup)
                                       .get<std::string>()));
    } else if (term == "Twig Name") {
        qry->push_back(
            QueryEngine::add_text_value("note_links.Version.sg_twig_name", value, negated));
    } else if (term == "Client Note") {
        if (value == "False")
            qry->push_back(Checkbox("client_note").is(false));
        else if (value == "True")
            qry->push_back(Checkbox("client_note").is(true));
        else
            throw XStudioError("Invalid query term " + term + " " + value);

    } else if (term == "Pipeline Step") {
        if (negated) {
            if (value == "None")
                qry->push_back(Text("sg_pipeline_step").is_not_null());
            else
                qry->push_back(Text("sg_pipeline_step").is_not(value));
        } else {
            if (value == "None")
                qry->push_back(Text("sg_pipeline_step").is_null());
            else
                qry->push_back(Text("sg_pipeline_step").is(value));
        }
    } else if (term == "Older Version") {
        qry->push_back(
            Number("note_links.Version.sg_dneg_version").less_than(std::stoi(value)));
    } else if (term == "Newer Version") {
        qry->push_back(
            Number("note_links.Version.sg_dneg_version").greater_than(std::stoi(value)));
    } else {
        spdlog::warn("{} Unhandled Term {}", __PRETTY_FUNCTION__, term);
    }
}

void QueryEngine::add_term_to_filter(
    const std::string &entity,
    const JsonStore &term,
    const int project_id,
    const JsonStore &lookup,
    FilterBy *qry) {
    auto name    = term.value("term", "");
    auto val     = term.value("value", "");
    auto live    = to_value(term, "livelink", false);
    auto negated = to_value(term, "negated", false);

    // kill queries with invalid shot live link.
    if (val.empty() and live and name == "Shot") {
        auto rel = R"({"type": "Shot", "id":0})"_json;
        qry->push_back(RelationType("entity").is(JsonStore(rel)));
    }

    if (val.empty()) {
        throw XStudioError("Empty query value " + name);
    }

    if (entity == "Playlists")
        add_playlist_term_to_filter(name, val, negated, project_id, lookup, qry);
    else if (entity == "Notes")
        add_note_term_to_filter(name, val, negated, project_id, lookup, qry);
    else if (entity == "Versions")
        add_version_term_to_filter(name, val, negated, project_id, lookup, qry);
    else
        spdlog::warn("{} Unhandled Entity {}", __PRETTY_FUNCTION__, entity);
}


utility::JsonStore QueryEngine::resolve_attribute_value(
    const std::string &type,
    const utility::JsonStore &value,
    const int project_id,
    const utility::JsonStore &lookup) {
    auto _type      = type;
    auto attr_value = utility::JsonStore();

    if (_type == "Author" || _type == "Recipient")
        _type = "User";

    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    try {
        auto val = value.get<std::string>();
        if (lookup.count(_type)) {
            if (lookup.at(_type).count(val)) {
                attr_value = lookup.at(_type).at(val);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", _type, __PRETTY_FUNCTION__, err.what(), value.dump(2));
    }

    return attr_value;
}

// resolve value from look up
utility::JsonStore QueryEngine::resolve_query_value(
    const std::string &type,
    const utility::JsonStore &value,
    const int project_id,
    const utility::JsonStore &lookup) {
    auto _type        = type;
    auto mapped_value = utility::JsonStore();

    if (_type == "Author" || _type == "Recipient")
        _type = "User";

    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    try {
        auto val = value.get<std::string>();
        if (lookup.count(_type)) {
            if (lookup.at(_type).count(val)) {
                mapped_value = lookup.at(_type).at(val).at("id");
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", _type, __PRETTY_FUNCTION__, err.what(), value.dump(2));
    }

    if (mapped_value.is_null()) {
        if (type == "User" || type == "Author" || type == "Recipient") {
            // usually because user if not assigned to project.
            // force no match.
            mapped_value = JsonStore(R"(0)"_json);
            spdlog::warn(
                "{} {} {} {}",
                "QueryEngine",
                type,
                "not enabled for this Project",
                value.dump(2));
        } else
            throw XStudioError("Invalid term value " + value.dump());
    }

    return mapped_value;
}

std::string QueryEngine::cache_name_auto(const std::string &type, const int project_id) {
    auto _type = type;

    if (project_id != -1 and TermHasProjectKey.count(type))
        _type += "-" + std::to_string(project_id);

    return _type;
}

std::optional<utility::JsonStore> QueryEngine::get_cache(const std::string &key) const {
    if (cache_.count(key))
        return cache_.at(key);

    return {};
}

std::optional<utility::JsonStore> QueryEngine::get_lookup(const std::string &key) const {
    if (lookup_.count(key))
        return lookup_.at(key);

    return {};
}

void QueryEngine::set_cache(const std::string &key, const utility::JsonStore &data) {
    cache_[key] = data;
    if (cache_changed_callback_)
        cache_changed_callback_(key);
}

void QueryEngine::set_shot_sequence_lookup(
    const std::string &key, const utility::JsonStore &data) {
    set_shot_sequence_lookup(key, data, lookup_);
    if (lookup_changed_callback_)
        lookup_changed_callback_(key);
}

void QueryEngine::set_shot_sequence_lookup(
    const std::string &key, const utility::JsonStore &data, utility::JsonStore &lookup) {
    auto tmp = R"({})"_json;

    // build lookup
    try {
        auto seqmap = std::map<long, nlohmann::json>();

        for (const auto &i : data) {
            auto value       = R"({"id": null, "name": null, "parent_id": null})"_json;
            value.at("name") = i.at(json::json_pointer("/attributes/code"));
            value.at("id")   = i.at("id");
            value.at("parent_id") =
                i.at(json::json_pointer("/relationships/sg_parent/data/id"));

            seqmap[i.at("id")] = value;
        }

        for (const auto &i : data) {
            auto seqvalue = seqmap.at(i.at("id"));
            auto seqs     = R"([])"_json;

            while (seqvalue.at("parent_id") != seqvalue.at("id")) {
                seqs.push_back(seqvalue);
                seqvalue = seqmap.at(seqvalue.at("parent_id"));
            }
            seqs.push_back(seqvalue);

            // this is the direct parent of these shots..
            for (const auto &shot : i.at(json::json_pointer("/relationships/shots/data"))) {
                tmp[std::to_string(shot.at("id").get<long>())] = seqs;
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    lookup[key] = tmp;
}

void QueryEngine::set_lookup(const std::string &key, const utility::JsonStore &data) {
    set_lookup(key, data, lookup_);
    if (lookup_changed_callback_)
        lookup_changed_callback_(key);
}

void QueryEngine::set_lookup(
    const std::string &key, const utility::JsonStore &data, utility::JsonStore &lookup) {
    auto tmp = R"({})"_json;

    auto entries = R"([])"_json;
    try {
        auto value = R"({"id": null, "name": null})"_json;
        for (const auto &i : data) {
            if (i.count("name"))
                value.at("name") = i.at("name");
            else if (i.at("attributes").count("name"))
                value.at("name") = i.at("attributes").at("name");
            else if (i.at("attributes").count("code"))
                value.at("name") = i.at("attributes").at("code");
            else
                continue;

            value.at("id") = i.at("id");

            tmp[value.at("name")] = value;
            if (value.at("id").is_string())
                tmp[value.at("id").get<std::string>()] = value;
            else
                tmp[std::to_string(value.at("id").get<long>())] = value;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), key, data.dump(2));
    }

    lookup[key] = tmp;
}


JsonStore QueryEngine::data_from_field(const JsonStore &data) {
    auto result = R"([])"_json;

    std::map<std::string, std::string> entries;

    for (const auto &i : data.at("properties").at("valid_values").at("value")) {
        auto value = i.get<std::string>();
        auto key   = value;
        if (data.at("properties").count("display_values") and
            data.at("properties").at("display_values").at("value").count(value)) {
            key = data.at("properties")
                      .at("display_values")
                      .at("value")
                      .at(value)
                      .get<std::string>();
        }

        entries.insert(std::make_pair(key, value));
    }

    for (const auto &i : entries) {
        auto field                  = R"({"id": null, "attributes": {"name": null}})"_json;
        field["attributes"]["name"] = i.first;
        field["id"]                 = i.second;
        result.push_back(field);
    }

    return JsonStore(result);
}

std::vector<std::string> QueryEngine::get_sequence_name(
    const int project_id, const int shot_id, const utility::JsonStore &lookup) {
    auto result = std::vector<std::string>();
    auto key    = cache_name("ShotSequence", project_id);
    auto shot   = std::to_string(shot_id);

    if (lookup.count(key)) {
        if (lookup.at(key).count(shot)) {
            for (const auto &seq : lookup.at(key).at(shot))
                result.push_back(seq.at("name"));
        }
    }

    return result;
}

std::string QueryEngine::get_version_name(const utility::JsonStore &metadata) {
    auto name = std::string();

    try {
        std::vector<std::string> locations(
            {{"/metadata/shotgun/version/attributes/code"},
             {"/metadata/image_source_metadata/metadata/external/ivy/file/version/name"},
             {"/metadata/audio_source_metadata/metadata/external/ivy/file/version/name"}});
        for (const auto &i : locations) {
            if (metadata.contains(json::json_pointer(i))) {
                name = metadata.at(json::json_pointer(i)).get<std::string>();
                break;
            }
        }
    } catch (...) {
    }

    return name;
}

utility::JsonStore QueryEngine::get_livelink_value(
    const std::string &term,
    const utility::JsonStore &metadata,
    const utility::JsonStore &lookup) {

    auto result = JsonStore();

    try {
        if (term == "Preferred Audio") {
            if (metadata.count("metadata"))
                result =
                    metadata.at("metadata").value("audio_source", nlohmann::json("movie_dneg"));
            else
                result = json::array({"movie_dneg"});
        } else if (term == "Preferred Visual") {
            if (metadata.count("metadata"))
                result =
                    metadata.at("metadata").value("image_source", nlohmann::json("movie_dneg"));
            else
                result = json::array({"movie_dneg"});
        } else if (term == "Entity") {
            if (metadata.contains(json::json_pointer("/metadata/shotgun/version"))) {
                auto type = metadata.at(json::json_pointer(
                    "/metadata/shotgun/version/relationships/entity/data/type"));
                auto id   = metadata.at(json::json_pointer(
                    "/metadata/shotgun/version/relationships/entity/data/id"));
                result    = nlohmann::json(
                    type.get<std::string>() + "-" + std::to_string(id.get<int>()));
            } else {
                // try and derive from show/shot ?
                const auto project = get_project_name(metadata);
                const auto shot    = get_shot_name(metadata);
                if (not project.empty() and not shot.empty()) {
                    // Type is always Shot
                    // but we need the shot id.
                    auto shot_id = 0;
                    auto project_id =
                        resolve_query_value("Project", nlohmann::json(project), lookup)
                            .get<int>();

                    // force fail if shot not found
                    try {
                        shot_id = resolve_query_value(
                                      "Shot", nlohmann::json(shot), project_id, lookup)
                                      .get<int>();
                    } catch (...) {
                    }
                    result = nlohmann::json("Shot-" + std::to_string(shot_id));
                }
            }
        } else if (metadata.contains(json::json_pointer("/metadata/shotgun/version"))) {
            if (term == "Version Name") {
                result = nlohmann::json(get_version_name(metadata));
            } else if (term == "Older Version" or term == "Newer Version") {
                auto val = metadata
                               .at(json::json_pointer(
                                   "/metadata/shotgun/version/attributes/sg_dneg_version"))
                               .get<long>();
                result = nlohmann::json(std::to_string(val));
            } else if (term == "Author" or term == "Recipient") {
                result = metadata.at(json::json_pointer(
                    "/metadata/shotgun/version/relationships/user/data/name"));
            } else if (term == "Shot") {
                result = nlohmann::json(get_shot_name(metadata));
            } else if (term == "Twig Name") {
                result = nlohmann::json(
                    std::string("^") +
                    metadata
                        .at(json::json_pointer(
                            "/metadata/shotgun/version/attributes/sg_twig_name"))
                        .get<std::string>() +
                    std::string("$"));
            } else if (term == "Pipeline Step") {
                result = metadata.at(json::json_pointer(
                    "/metadata/shotgun/version/attributes/sg_pipeline_step"));
            } else if (term == "Project") {
                result = nlohmann::json(get_project_id(metadata, lookup));
            } else if (term == "Twig Type") {
                result = metadata.at(
                    json::json_pointer("/metadata/shotgun/version/attributes/sg_twig_type"));
            } else if (term == "Sequence") {
                auto type = metadata.at(json::json_pointer(
                    "/metadata/shotgun/version/relationships/entity/data/type"));
                if (type == "Sequence") {
                    result = metadata.at(json::json_pointer(
                        "/metadata/shotgun/version/relationships/entity/data/name"));
                } else {
                    auto project_id =
                        metadata
                            .at(json::json_pointer(
                                "/metadata/shotgun/version/relationships/project/data/id"))
                            .get<int>();
                    auto shot_id =
                        metadata
                            .at(json::json_pointer(
                                "/metadata/shotgun/version/relationships/entity/data/id"))
                            .get<int>();

                    auto seq_name = get_sequence_name(project_id, shot_id, lookup);
                    if (not seq_name.empty())
                        result = nlohmann::json(seq_name);
                }
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), term, metadata.dump(2));
    }


    return result;
}

void QueryEngine::regenerate_ids(nlohmann::json &data) {
    // iterate over all "id" keys reseting the uuids
    if (data.is_array()) {
        for (size_t i = 0; i < data.size(); i++)
            regenerate_ids(data.at(i));

    } else if (data.is_object()) {
        if (data.count("id"))
            data["id"] = Uuid::generate();

        // duplicates are not system presets.
        if (data.count("update"))
            data["update"] = nullptr;

        if (data.count("children"))
            regenerate_ids(data.at("children"));
    }
}
