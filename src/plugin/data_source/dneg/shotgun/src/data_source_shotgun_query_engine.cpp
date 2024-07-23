// SPDX-License-Identifier: Apache-2.0

#include "data_source_shotgun_definitions.hpp"
#include "data_source_shotgun_query_engine.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio;
using namespace xstudio::shotgun_client;
using namespace xstudio::utility;

utility::JsonStore QueryEngine::build_query(
    const int project_id,
    const std::string &entity,
    const utility::JsonStore &group_terms,
    const utility::JsonStore &terms,
    const utility::JsonStore &context,
    const utility::JsonStore &lookup) {
    auto query = utility::JsonStore(GetQueryResult);
    FilterBy filter;

    query["entity"] = entity;

    query["context"] = context;
    // R"({
    //     "type": null,
    //     "epoc": null,
    //     "audio_source": "",
    //     "visual_source": "",
    //     "flag_text": "",
    //     "flag_colour": "",
    //     "truncated": false
    // })"_json;

    if (entity == "Versions")
        query["fields"] = VersionFields;
    else if (entity == "Notes")
        query["fields"] = NoteFields;
    else if (entity == "Playlists")
        query["fields"] = PlaylistFields;

    auto merged_preset = merge_query(terms, group_terms);

    FilterBy qry;

    try {

        std::multimap<std::string, JsonStore> qry_terms;
        std::vector<std::string> order_by;

        // collect terms in map
        for (const auto &i : merged_preset) {
            if (i.at("enabled").get<bool>()) {
                // filter out order by and max count..
                if (i.at("term") == "Disable Global") {
                    // filtered out
                } else if (i.at("term") == "Result Limit") {
                    query["max_result"] = std::stoi(i.at("value").get<std::string>());
                } else if (i.at("term") == "Preferred Visual") {
                    query["context"]["visual_source"] = i.at("value").get<std::string>();
                } else if (i.at("term") == "Preferred Audio") {
                    query["context"]["audio_source"] = i.at("value").get<std::string>();
                } else if (i.at("term") == "Flag Media") {
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
                } else if (i.at("term") == "Order By") {
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
                    qry_terms.insert(std::make_pair(
                        std::string(i.value("negated", false) ? "Not " : "") +
                            i.at("term").get<std::string>(),
                        i));
                }
            }
        }
        // set defaults if not specified
        if (query["context"]["visual_source"].empty())
            query["context"]["visual_source"] = "SG Movie";
        if (query["context"]["audio_source"].empty())
            query["context"]["audio_source"] = query["context"]["visual_source"];

        // set order by
        if (order_by.empty()) {
            order_by.emplace_back("-created_at");
        }
        query["order"] = order_by;

        // add terms we always want.
        qry.push_back(Number("project.Project.id").is(project_id));

        if (context == "Playlists") {
        } else if (entity == "Versions") {
            qry.push_back(Text("sg_deleted").is_null());
            qry.push_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
        } else if (entity == "Notes") {
        }

        // create OR group for multiples of same term.
        std::string key;
        FilterBy *dest = &qry;
        for (const auto &i : qry_terms) {
            if (key != i.first) {
                key = i.first;
                // multiple identical terms OR / AND them..
                if (qry_terms.count(key) > 1) {
                    if (starts_with(key, "Not ") or starts_with(key, "Exclude "))
                        qry.push_back(FilterBy(BoolOperator::AND));
                    else
                        qry.push_back(FilterBy(BoolOperator::OR));
                    dest = &std::get<FilterBy>(qry.back());
                } else {
                    dest = &qry;
                }
            }
            try {
                // addTerm(project_id, context, dest, i.second);
            } catch (const std::exception &err) {
                // spdlog::warn("{}", err.what());
                //  bad term.. we ignore them..

                // if(i.second.value("livelink", false))
                //     throw XStudioError(std::string("LiveLink ") + err.what());

                // throw;
            }
        }

        query["query"] = qry;

    } catch (const std::exception &err) {
        throw;
    }

    return query;
}

utility::JsonStore QueryEngine::merge_query(
    const utility::JsonStore &base,
    const utility::JsonStore &override,
    const bool ignore_duplicates) {
    auto result = base;

    // we need to preprocess for Disable Global flags..
    auto disable_globals = std::set<std::string>();

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

    return result;
}
