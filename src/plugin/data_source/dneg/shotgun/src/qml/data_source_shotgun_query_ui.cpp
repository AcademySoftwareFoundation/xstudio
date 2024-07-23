// SPDX-License-Identifier: Apache-2.0
#include "data_source_shotgun_ui.hpp"
#include "shotgun_model_ui.hpp"

#include "../data_source_shotgun.hpp"
#include "../data_source_shotgun_definitions.hpp"
#include "../data_source_shotgun_query_engine.hpp"

// #include "xstudio/utility/string_helpers.hpp"
// #include "xstudio/ui/qml/json_tree_model_ui.hpp"
// #include "xstudio/global_store/global_store.hpp"
// #include "xstudio/atoms.hpp"
// #include "xstudio/ui/qml/module_ui.hpp"
// #include "xstudio/utility/chrono.hpp"

// #include <QProcess>
// #include <QQmlExtensionPlugin>
// #include <qdebug.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace xstudio::global_store;


void ShotgunDataSourceUI::updateQueryValueCache(
    const std::string &type, const utility::JsonStore &data, const int project_id) {
    std::map<std::string, JsonStore> cache;

    auto _type = type;
    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    // load map..
    if (not data.is_null()) {
        try {
            for (const auto &i : data) {
                if (i.count("name"))
                    cache[i.at("name").get<std::string>()] = i.at("id");
                else if (i.at("attributes").count("name"))
                    cache[i.at("attributes").at("name").get<std::string>()] = i.at("id");
                else if (i.at("attributes").count("code"))
                    cache[i.at("attributes").at("code").get<std::string>()] = i.at("id");
            }
        } catch (...) {
        }

        // add reverse map
        try {
            for (const auto &i : data) {
                if (i.count("name"))
                    cache[i.at("id").get<std::string>()] = i.at("name");
                else if (i.at("attributes").count("name"))
                    cache[i.at("id").get<std::string>()] = i.at("attributes").at("name");
                else if (i.at("attributes").count("code"))
                    cache[i.at("id").get<std::string>()] = i.at("attributes").at("code");
            }
        } catch (...) {
        }
    }

    query_value_cache_[_type] = cache;
}

utility::JsonStore ShotgunDataSourceUI::getQueryValue(
    const std::string &type, const utility::JsonStore &value, const int project_id) const {
    // look for map
    auto _type        = type;
    auto mapped_value = utility::JsonStore();

    if (_type == "Author" || _type == "Recipient")
        _type = "User";

    if (project_id != -1)
        _type += "-" + std::to_string(project_id);

    try {
        auto val = value.get<std::string>();
        if (query_value_cache_.count(_type)) {
            if (query_value_cache_.at(_type).count(val)) {
                mapped_value = query_value_cache_.at(_type).at(val);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", _type, __PRETTY_FUNCTION__, err.what(), value.dump(2));
    }

    if (mapped_value.is_null())
        throw XStudioError("Invalid term value " + value.dump());

    return mapped_value;
}


// merge global filters with Preset.
// Not sure if this should really happen here..
// DST = PRESET src == Global

QVariant ShotgunDataSourceUI::mergeQueries(
    const QVariant &dst, const QVariant &src, const bool ignore_duplicates) const {


    JsonStore dst_qry;
    JsonStore src_qry;

    try {
        if (std::string(dst.typeName()) == "QJSValue") {
            dst_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(dst.value<QJSValue>().toVariant())
                    .toJson(QJsonDocument::Compact)
                    .constData());
        } else {
            dst_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(dst).toJson(QJsonDocument::Compact).constData());
        }

        if (std::string(src.typeName()) == "QJSValue") {
            src_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(src.value<QJSValue>().toVariant())
                    .toJson(QJsonDocument::Compact)
                    .constData());
        } else {
            src_qry = nlohmann::json::parse(
                QJsonDocument::fromVariant(src).toJson(QJsonDocument::Compact).constData());
        }

        auto merged = QueryEngine::merge_query(
            dst_qry["queries"], src_qry.at("queries"), ignore_duplicates);
        dst_qry["queries"] = merged;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QVariantMapFromJson(dst_qry);
}

QFuture<QString> ShotgunDataSourceUI::executeQuery(
    const QString &context,
    const int project_id,
    const QVariant &query,
    const bool update_result_model) {
    // build and dispatch query, we then pass result via message back to ourself.

    // executeQueryNew(context, project_id, query, update_result_model);

    auto cxt = StdFromQString(context);
    JsonStore qry;

    try {
        qry = JsonStore(nlohmann::json::parse(
            QJsonDocument::fromVariant(query.value<QJSValue>().toVariant())
                .toJson(QJsonDocument::Compact)
                .constData()));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QtConcurrent::run([=]() {
        if (backend_ and not qry.is_null()) {
            scoped_actor sys{system()};

            auto request = JsonStore(GetQueryResult);

            request["context"] = R"({
                "type": null,
                "epoc": null,
                "audio_source": "",
                "visual_source": "",
                "flag_text": "",
                "flag_colour": "",
                "truncated": false
            })"_json;

            request["context"]["epoc"] = utility::to_epoc_milliseconds(utility::clock::now());

            if (cxt == "Playlists") {
                request["context"]["type"] = "playlist_result";
                request["entity"]          = "Playlists";
                request["fields"]          = PlaylistFields;
            } else if (cxt == "Versions") {
                request["context"]["type"] = "shot_result";
                request["entity"]          = "Versions";
                request["fields"]          = VersionFields;
            } else if (cxt == "Reference") {
                request["context"]["type"] = "reference_result";
                request["entity"]          = "Versions";
                request["fields"]          = VersionFields;
            } else if (cxt == "Versions Tree") {
                request["context"]["type"] = "shot_tree_result";
                request["entity"]          = "Versions";
                request["fields"]          = VersionFields;
            } else if (cxt == "Menu Setup") {
                request["context"]["type"] = "media_action_result";
                request["entity"]          = "Versions";
                request["fields"]          = VersionFields;
            } else if (cxt == "Notes") {
                request["context"]["type"] = "note_result";
                request["entity"]          = "Notes";
                request["fields"]          = NoteFields;
            } else if (cxt == "Notes Tree") {
                request["context"]["type"] = "note_tree_result";
                request["entity"]          = "Notes";
                request["fields"]          = NoteFields;
            }

            try {
                const auto &[filter, orderby, max_count, source_selection, flag_selection] =
                    buildQuery(cxt, project_id, qry);
                request["max_result"] = max_count;
                request["order"]      = orderby;
                request["query"]      = filter;


                request["context"]["visual_source"] = source_selection.first;
                request["context"]["audio_source"]  = source_selection.second;
                request["context"]["flag_text"]     = flag_selection.first;
                request["context"]["flag_colour"]   = flag_selection.second;

                auto data = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGUN_TIMEOUT, get_data_atom_v, request);

                if (data.at("result").at("data").is_array())
                    data["context"]["truncated"] =
                        (static_cast<int>(data.at("result").at("data").size()) == max_count);

                if (update_result_model)
                    anon_send(as_actor(), shotgun_info_atom_v, data);

                return QStringFromStd(data.dump());

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                // silence error..
                if (update_result_model)
                    anon_send(as_actor(), shotgun_info_atom_v, request);

                if (starts_with(std::string(err.what()), "LiveLink ")) {
                    return QStringFromStd(request.dump()); // R"({"data":[]})");
                }

                return QStringFromStd(err.what());
            }
        }
        return QString();
    });
}

QFuture<QString> ShotgunDataSourceUI::executeQueryNew(
    const QString &context,
    const int project_id,
    const QVariant &query,
    const bool update_result_model) {
    // build and dispatch query, we then pass result via message back to ourself.
    JsonStore qry;

    try {
        qry = JsonStore(nlohmann::json::parse(
            QJsonDocument::fromVariant(query.value<QJSValue>().toVariant())
                .toJson(QJsonDocument::Compact)
                .constData()));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return QtConcurrent::run([=]() {
        if (backend_ and not qry.is_null()) {
            scoped_actor sys{system()};

            std::string entity;
            auto query_context = R"({
                "type": null,
                "epoc": null,
                "audio_source": "",
                "visual_source": "",
                "flag_text": "",
                "flag_colour": "",
                "truncated": false
            })"_json;

            query_context["epoc"] = utility::to_epoc_milliseconds(utility::clock::now());

            if (context == "Playlists") {
                query_context["type"] = "playlist_result";
                entity                = "Playlists";
            } else if (context == "Versions") {
                query_context["type"] = "shot_result";
                entity                = "Versions";
            } else if (context == "Reference") {
                query_context["type"] = "reference_result";
                entity                = "Versions";
            } else if (context == "Versions Tree") {
                query_context["type"] = "shot_tree_result";
                entity                = "Versions";
            } else if (context == "Menu Setup") {
                query_context["type"] = "media_action_result";
                entity                = "Versions";
            } else if (context == "Notes") {
                query_context["type"] = "note_result";
                entity                = "Notes";
            } else if (context == "Notes Tree") {
                query_context["type"] = "note_tree_result";
                entity                = "Notes";
            }


            try {
                auto request = QueryEngine::build_query(
                    project_id, entity, R"([])"_json, qry, query_context, utility::JsonStore());

                try {

                    spdlog::warn("{}", request.dump(2));

                    // const auto &[filter, orderby, max_count, source_selection,
                    // flag_selection] =
                    //     buildQuery(cxt, project_id, qry);

                    // request["max_result"] = max_count;
                    // request["order"]      = orderby;
                    // request["query"]      = filter;

                    // request["context"]["visual_source"] = source_selection.first;
                    // request["context"]["audio_source"]  = source_selection.second;
                    // request["context"]["flag_text"]     = flag_selection.first;
                    // request["context"]["flag_colour"]   = flag_selection.second;

                    return QString();

                    auto data = request_receive_wait<JsonStore>(
                        *sys, backend_, SHOTGUN_TIMEOUT, get_data_atom_v, request);

                    if (data.at("result").at("data").is_array())
                        data["context"]["truncated"] =
                            (static_cast<int>(data.at("result").at("data").size()) ==
                             data.at("result").at("max_result"));

                    if (update_result_model)
                        anon_send(as_actor(), shotgun_info_atom_v, data);

                    return QStringFromStd(data.dump());

                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    // silence error..
                    if (update_result_model)
                        anon_send(as_actor(), shotgun_info_atom_v, request);

                    if (starts_with(std::string(err.what()), "LiveLink ")) {
                        return QStringFromStd(request.dump()); // R"({"data":[]})");
                    }

                    return QStringFromStd(err.what());
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }
        return QString();
    });
}


Text ShotgunDataSourceUI::addTextValue(
    const std::string &filter, const std::string &value, const bool negated) const {
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

void ShotgunDataSourceUI::addTerm(
    const int project_id, const std::string &context, FilterBy *qry, const JsonStore &term) {
    // qry->push_back(Text("versions").is_not_null());
    auto trm     = term.at("term").get<std::string>();
    auto val     = term.at("value").get<std::string>();
    auto live    = term.value("livelink", false);
    auto negated = term.value("negated", false);


    // kill queries with invalid shot live link.
    if (val.empty() and live and trm == "Shot") {
        auto rel = R"({"type": "Shot", "id":0})"_json;
        qry->push_back(RelationType("entity").is(JsonStore(rel)));
    }

    if (val.empty()) {
        throw XStudioError("Empty query value " + trm);
    }

    if (context == "Playlists") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("updated_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("updated_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("updated_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("updated_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("updated_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("updated_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("updated_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("updated_at").in_last(150, Period::DAY));
            } else if (val == "Future Only") {
                qry->push_back(DateTime("sg_date_and_time").in_next(30, Period::DAY));
            } else {
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Playlist Type") {
            if (negated)
                qry->push_back(Text("sg_type").is_not(val));
            else
                qry->push_back(Text("sg_type").is(val));
        } else if (trm == "Has Contents") {
            if (val == "False")
                qry->push_back(Text("versions").is_null());
            else if (val == "True")
                qry->push_back(Text("versions").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Site") {
            if (negated)
                qry->push_back(Text("sg_location").is_not(val));
            else
                qry->push_back(Text("sg_location").is(val));
        } else if (trm == "Review Location") {
            if (negated)
                qry->push_back(Text("sg_review_location_1").is_not(val));
            else
                qry->push_back(Text("sg_review_location_1").is(val));
        } else if (trm == "Department") {
            if (negated)
                qry->push_back(Number("sg_department_unit.Department.id")
                                   .is_not(getQueryValue(trm, JsonStore(val)).get<int>()));
            else
                qry->push_back(Number("sg_department_unit.Department.id")
                                   .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Has Notes") {
            if (val == "False")
                qry->push_back(Text("notes").is_null());
            else if (val == "True")
                qry->push_back(Text("notes").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Unit") {
            auto tmp  = R"({"type": "CustomEntity24", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            if (negated)
                qry->push_back(RelationType("sg_unit2").in({JsonStore(tmp)}));
            else
                qry->push_back(RelationType("sg_unit2").not_in({JsonStore(tmp)}));
        }

    } else if (context == "Notes" || context == "Notes Tree") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("created_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
            } else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("subject", val, negated));
        } else if (trm == "Note Type") {
            if (negated)
                qry->push_back(Text("sg_note_type").is_not(val));
            else
                qry->push_back(Text("sg_note_type").is(val));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Recipient") {
            auto tmp  = R"({"type": "HumanUser", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val)).get<int>();
            qry->push_back(RelationType("addressings_to").in({JsonStore(tmp)}));
        } else if (trm == "Shot") {
            auto tmp  = R"({"type": "Shot", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
        } else if (trm == "Sequence") {
            try {
                if (sequences_map_.count(project_id)) {
                    auto row = sequences_map_[project_id]->search(
                        QVariant::fromValue(QStringFromStd(val)), QStringFromStd("display"), 0);
                    if (row != -1) {
                        auto rel = std::vector<JsonStore>();
                        // auto sht   = R"({"type": "Shot", "id":0})"_json;
                        // auto shots = sequences_map_[project_id]
                        //                  ->modelData()
                        //                  .at(row)
                        //                  .at("relationships")
                        //                  .at("shots")
                        //                  .at("data");

                        // for (const auto &i : shots) {
                        //     sht["id"] = i.at("id").get<int>();
                        //     rel.emplace_back(sht);
                        // }
                        auto seq = R"({"type": "Sequence", "id":0})"_json;
                        seq["id"] =
                            sequences_map_[project_id]->modelData().at(row).at("id").get<int>();
                        rel.emplace_back(seq);

                        qry->push_back(RelationType("note_links").in(rel));
                    } else
                        throw XStudioError("Invalid query term " + trm + " " + val);
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Playlist") {
            auto tmp  = R"({"type": "Playlist", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("note_links").in({JsonStore(tmp)}));
        } else if (trm == "Version Name") {
            qry->push_back(addTextValue("note_links.Version.code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Twig Type") {
            if (negated)
                qry->push_back(
                    Text("note_links.Version.sg_twig_type_code")
                        .is_not(
                            getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
            else
                qry->push_back(
                    Text("note_links.Version.sg_twig_type_code")
                        .is(getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
        } else if (trm == "Twig Name") {
            qry->push_back(addTextValue("note_links.Version.sg_twig_name", val, negated));
        } else if (trm == "Client Note") {
            if (val == "False")
                qry->push_back(Checkbox("client_note").is(false));
            else if (val == "True")
                qry->push_back(Checkbox("client_note").is(true));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);

        } else if (trm == "Pipeline Step") {
            if (negated) {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_not_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is_not(val));
            } else {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is(val));
            }
        } else if (trm == "Older Version") {
            qry->push_back(
                Number("note_links.Version.sg_dneg_version").less_than(std::stoi(val)));
        } else if (trm == "Newer Version") {
            qry->push_back(
                Number("note_links.Version.sg_dneg_version").greater_than(std::stoi(val)));
        }

    } else if (
        context == "Versions" or context == "Reference" or context == "Versions Tree" or
        context == "Menu Setup") {
        if (trm == "Lookback") {
            if (val == "Today")
                qry->push_back(DateTime("created_at").in_calendar_day(0));
            else if (val == "1 Day")
                qry->push_back(DateTime("created_at").in_last(1, Period::DAY));
            else if (val == "3 Days")
                qry->push_back(DateTime("created_at").in_last(3, Period::DAY));
            else if (val == "7 Days")
                qry->push_back(DateTime("created_at").in_last(7, Period::DAY));
            else if (val == "20 Days")
                qry->push_back(DateTime("created_at").in_last(20, Period::DAY));
            else if (val == "30 Days")
                qry->push_back(DateTime("created_at").in_last(30, Period::DAY));
            else if (val == "30-60 Days") {
                qry->push_back(DateTime("created_at").not_in_last(30, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(60, Period::DAY));
            } else if (val == "60-90 Days") {
                qry->push_back(DateTime("created_at").not_in_last(60, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(90, Period::DAY));
            } else if (val == "100-150 Days") {
                qry->push_back(DateTime("created_at").not_in_last(100, Period::DAY));
                qry->push_back(DateTime("created_at").in_last(150, Period::DAY));
            } else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Playlist") {
            auto tmp  = R"({"type": "Playlist", "id":0})"_json;
            tmp["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("playlists").in({JsonStore(tmp)}));
        } else if (trm == "Author") {
            qry->push_back(Number("created_by.HumanUser.id")
                               .is(getQueryValue(trm, JsonStore(val)).get<int>()));
        } else if (trm == "Older Version") {
            qry->push_back(Number("sg_dneg_version").less_than(std::stoi(val)));
        } else if (trm == "Newer Version") {
            qry->push_back(Number("sg_dneg_version").greater_than(std::stoi(val)));
        } else if (trm == "Site") {
            if (negated)
                qry->push_back(Text("sg_location").is_not(val));
            else
                qry->push_back(Text("sg_location").is(val));
        } else if (trm == "On Disk") {
            std::string prop = std::string("sg_on_disk_") + val;
            if (negated)
                qry->push_back(Text(prop).is("None"));
            else
                qry->push_back(FilterBy().Or(Text(prop).is("Full"), Text(prop).is("Partial")));
        } else if (trm == "Pipeline Step") {
            if (negated) {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_not_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is_not(val));
            } else {
                if (val == "None")
                    qry->push_back(Text("sg_pipeline_step").is_null());
                else
                    qry->push_back(Text("sg_pipeline_step").is(val));
            }
        } else if (trm == "Pipeline Status") {
            if (negated)
                qry->push_back(
                    Text("sg_status_list")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("sg_status_list")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Production Status") {
            if (negated)
                qry->push_back(
                    Text("sg_production_status")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("sg_production_status")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Shot Status") {
            if (negated)
                qry->push_back(
                    Text("entity.Shot.sg_status_list")
                        .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
            else
                qry->push_back(Text("entity.Shot.sg_status_list")
                                   .is(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Exclude Shot Status") {
            qry->push_back(Text("entity.Shot.sg_status_list")
                               .is_not(getQueryValue(trm, JsonStore(val)).get<std::string>()));
        } else if (trm == "Latest Version") {
            if (val == "False")
                qry->push_back(Text("sg_latest").is_null());
            else if (val == "True")
                qry->push_back(Text("sg_latest").is("Yes"));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Is Hero") {
            if (val == "False")
                qry->push_back(Checkbox("sg_is_hero").is(false));
            else if (val == "True")
                qry->push_back(Checkbox("sg_is_hero").is(true));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Shot") {
            auto rel  = R"({"type": "Shot", "id":0})"_json;
            rel["id"] = getQueryValue(trm, JsonStore(val), project_id).get<int>();
            qry->push_back(RelationType("entity").is(JsonStore(rel)));
        } else if (trm == "Sequence") {
            try {
                if (sequences_map_.count(project_id)) {
                    auto row = sequences_map_[project_id]->search(
                        QVariant::fromValue(QStringFromStd(val)), QStringFromStd("display"), 0);
                    if (row != -1) {
                        auto rel = std::vector<JsonStore>();
                        // auto sht   = R"({"type": "Shot", "id":0})"_json;
                        // auto shots = sequences_map_[project_id]
                        //                  ->modelData()
                        //                  .at(row)
                        //                  .at("relationships")
                        //                  .at("shots")
                        //                  .at("data");

                        // for (const auto &i : shots) {
                        //     sht["id"] = i.at("id").get<int>();
                        //     rel.emplace_back(sht);
                        // }
                        auto seq = R"({"type": "Sequence", "id":0})"_json;
                        seq["id"] =
                            sequences_map_[project_id]->modelData().at(row).at("id").get<int>();
                        rel.emplace_back(seq);

                        qry->push_back(RelationType("entity").in(rel));
                    } else
                        throw XStudioError("Invalid query term " + trm + " " + val);
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                throw XStudioError("Invalid query term " + trm + " " + val);
            }
        } else if (trm == "Sent To Client") {
            if (val == "False")
                qry->push_back(DateTime("sg_date_submitted_to_client").is_null());
            else if (val == "True")
                qry->push_back(DateTime("sg_date_submitted_to_client").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);


        } else if (trm == "Sent To Dailies") {
            if (val == "False")
                qry->push_back(FilterBy().And(
                    DateTime("sg_submit_dailies").is_null(),
                    DateTime("sg_submit_dailies_chn").is_null(),
                    DateTime("sg_submit_dailies_mtl").is_null(),
                    DateTime("sg_submit_dailies_van").is_null(),
                    DateTime("sg_submit_dailies_mum").is_null()));
            else if (val == "True")
                qry->push_back(FilterBy().Or(
                    DateTime("sg_submit_dailies").is_not_null(),
                    DateTime("sg_submit_dailies_chn").is_not_null(),
                    DateTime("sg_submit_dailies_mtl").is_not_null(),
                    DateTime("sg_submit_dailies_van").is_not_null(),
                    DateTime("sg_submit_dailies_mum").is_not_null()));
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Has Notes") {
            if (val == "False")
                qry->push_back(Text("notes").is_null());
            else if (val == "True")
                qry->push_back(Text("notes").is_not_null());
            else
                throw XStudioError("Invalid query term " + trm + " " + val);
        } else if (trm == "Filter") {
            qry->push_back(addTextValue("code", val, negated));
        } else if (trm == "Tag") {
            qry->push_back(addTextValue("entity.Shot.tags.Tag.name", val, negated));
        } else if (trm == "Reference Tag" or trm == "Reference Tags") {

            if (val.find(',') != std::string::npos) {
                // split ...
                for (const auto &i : split(val, ',')) {
                    if (negated)
                        qry->push_back(
                            RelationType("tags").name_not_contains(i + ".REFERENCE"));
                    else
                        qry->push_back(RelationType("tags").name_is(i + ".REFERENCE"));
                }
            } else {
                if (negated)
                    qry->push_back(RelationType("tags").name_not_contains(val + ".REFERENCE"));
                else
                    qry->push_back(RelationType("tags").name_is(val + ".REFERENCE"));
            }
        } else if (trm == "Tag (Version)") {
            qry->push_back(addTextValue("tags.Tag.name", val, negated));
        } else if (trm == "Twig Name") {
            qry->push_back(addTextValue("sg_twig_name", val, negated));
        } else if (trm == "Twig Type") {
            if (negated)
                qry->push_back(
                    Text("sg_twig_type_code")
                        .is_not(
                            getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
            else
                qry->push_back(
                    Text("sg_twig_type_code")
                        .is(getQueryValue("TwigTypeCode", JsonStore(val)).get<std::string>()));
        } else if (trm == "Completion Location") {
            auto rel  = R"({"type": "CustomNonProjectEntity16", "id":0})"_json;
            rel["id"] = getQueryValue(trm, JsonStore(val)).get<int>();
            if (negated)
                qry->push_back(RelationType("entity.Shot.sg_primary_shot_location")
                                   .is_not(JsonStore(rel)));
            else
                qry->push_back(
                    RelationType("entity.Shot.sg_primary_shot_location").is(JsonStore(rel)));

        } else {
            spdlog::warn("{} Unhandled {} {}", __PRETTY_FUNCTION__, trm, val);
        }
    }
}


std::tuple<
    utility::JsonStore,
    std::vector<std::string>,
    int,
    std::pair<std::string, std::string>,
    std::pair<std::string, std::string>>
ShotgunDataSourceUI::buildQuery(
    const std::string &context, const int project_id, const utility::JsonStore &query) {

    int max_count = maximum_result_count_;
    std::vector<std::string> order_by;
    std::pair<std::string, std::string> source_selection;
    std::pair<std::string, std::string> flag_selection;

    FilterBy qry;
    try {

        std::multimap<std::string, JsonStore> qry_terms;

        // collect terms in map
        for (const auto &i : query.at("queries")) {
            if (i.at("enabled").get<bool>()) {
                // filter out order by and max count..
                if (i.at("term") == "Disable Global") {
                    // filtered out
                } else if (i.at("term") == "Result Limit") {
                    max_count = std::stoi(i.at("value").get<std::string>());
                } else if (i.at("term") == "Preferred Visual") {
                    source_selection.first = i.at("value").get<std::string>();
                } else if (i.at("term") == "Preferred Audio") {
                    source_selection.second = i.at("value").get<std::string>();
                } else if (i.at("term") == "Flag Media") {
                    flag_selection.first = i.at("value").get<std::string>();
                    if (flag_selection.first == "Red")
                        flag_selection.second = "#FFFF0000";
                    else if (flag_selection.first == "Green")
                        flag_selection.second = "#FF00FF00";
                    else if (flag_selection.first == "Blue")
                        flag_selection.second = "#FF0000FF";
                    else if (flag_selection.first == "Yellow")
                        flag_selection.second = "#FFFFFF00";
                    else if (flag_selection.first == "Orange")
                        flag_selection.second = "#FFFFA500";
                    else if (flag_selection.first == "Purple")
                        flag_selection.second = "#FF800080";
                    else if (flag_selection.first == "Black")
                        flag_selection.second = "#FF000000";
                    else if (flag_selection.first == "White")
                        flag_selection.second = "#FFFFFFFF";
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
                    if (context == "Playlists") {
                        if (val == "Date And Time")
                            field = "sg_date_and_time";
                        else if (val == "Created")
                            field = "created_at";
                        else if (val == "Updated")
                            field = "updated_at";
                    } else if (
                        context == "Versions" or context == "Versions Tree" or
                        context == "Reference" or context == "Menu Setup") {
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
                    } else if (context == "Notes" or context == "Notes Tree") {
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

            // set defaults if not specified
            if (source_selection.first.empty())
                source_selection.first = "SG Movie";
            if (source_selection.second.empty())
                source_selection.second = source_selection.first;
        }

        // add terms we always want.
        if (context == "Playlists") {
            qry.push_back(Number("project.Project.id").is(project_id));
        } else if (
            context == "Versions" or context == "Versions Tree" or context == "Menu Setup") {
            qry.push_back(Number("project.Project.id").is(project_id));
            qry.push_back(Text("sg_deleted").is_null());
            // qry.push_back(Entity("entity").type_is("Shot"));
            qry.push_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
        } else if (context == "Reference") {
            qry.push_back(Number("project.Project.id").is(project_id));
            qry.push_back(Text("sg_deleted").is_null());
            qry.push_back(FilterBy().Or(
                Text("sg_path_to_movie").is_not_null(),
                Text("sg_path_to_frames").is_not_null()));
            // qry.push_back(Entity("entity").type_is("Asset"));
        } else if (context == "Notes" or context == "Notes Tree") {
            qry.push_back(Number("project.Project.id").is(project_id));
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
                addTerm(project_id, context, dest, i.second);
            } catch (const std::exception &err) {
                // spdlog::warn("{}", err.what());
                //  bad term.. we ignore them..

                // if(i.second.value("livelink", false))
                //     throw XStudioError(std::string("LiveLink ") + err.what());

                // throw;
            }
        }
    } catch (const std::exception &err) {
        throw;
    }

    if (order_by.empty()) {
        if (context == "Playlists")
            order_by.emplace_back("-created_at");
        else if (context == "Versions" or context == "Versions Tree")
            order_by.emplace_back("-created_at");
        else if (context == "Reference")
            order_by.emplace_back("-created_at");
        else if (context == "Menu Setup")
            order_by.emplace_back("-created_at");
        else if (context == "Notes" or context == "Notes Tree")
            order_by.emplace_back("-created_at");
    }

    // spdlog::warn("{}", JsonStore(qry).dump(2));
    // spdlog::warn("{}", join_as_string(order_by,","));
    return std::make_tuple(
        JsonStore(qry), order_by, max_count, source_selection, flag_selection);
}
