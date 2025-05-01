// SPDX-License-Identifier: Apache-2.0
#include "shotbrowser_engine_ui.hpp"
#include "model_ui.hpp"

#include "definitions.hpp"
#include "query_engine.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace xstudio::global_store;
using namespace xstudio::data_source;


void ShotBrowserEngine::updateQueryValueCache(
    const std::string &type, const utility::JsonStore &data, const int project_id) {

    if (type == "Sequence") {
        query_engine_.set_shot_sequence_lookup(
            QueryEngine::cache_name("ShotSequence", project_id), data);
    }

    query_engine_.set_lookup(QueryEngine::cache_name(type, project_id), data);
}

// merge global filters with Preset.
// Not sure if this should really happen here..
// DST = PRESET src == Global

QVariant ShotBrowserEngine::mergeQueries(
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

QFuture<QVariant> ShotBrowserEngine::executeQueryJSON(
    const QStringList &preset_paths,
    const QVariantMap &env,
    const QVariantList &custom_terms,
    const QVariantMap &customContext) {


    // spdlog::warn("ShotBrowserEngine::executeQuery{}", live_link_metadata_.dump(2));
    // get project from metadata.
    try {
        auto project_id =
            query_engine_.get_project_id(live_link_metadata_, query_engine_.cache());

        // if (not project_id)
        //     throw std::runtime_error("Project metadata not found.");

        return executeProjectQueryJSON(
            preset_paths, project_id, env, custom_terms, customContext);
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), live_link_metadata_.dump(2));
        return QtConcurrent::run([=]() { return QVariant(); });
    }
}

QFuture<QVariant> ShotBrowserEngine::executeProjectQueryJSON(
    const QStringList &preset_paths,
    const int project_id,
    const QVariantMap &env,
    const QVariantList &custom_terms,
    const QVariantMap &customContext) {

    // spdlog::warn("{} project_id - {}", __PRETTY_FUNCTION__, project_id);
    cacheProject(project_id);

    return QtConcurrent::run([=]() {
        if (backend_) {
            scoped_actor sys{system()};

            std::vector<std::string> paths;

            for (const auto &i : preset_paths)
                paths.emplace_back(StdFromQString(i));

            auto request            = JsonStore(GetExecutePreset);
            request["project_id"]   = project_id;
            request["preset_paths"] = paths;
            request["metadata"]     = live_link_metadata_;
            request["context"]      = R"({
                "type": null,
                "epoc": null,
                "audio_source": [],
                "visual_source": [],
                "flag_text": "",
                "flag_colour": "",
                "truncated": false,
                "custom": null
            })"_json;

            try {
                request["env"]               = qvariant_to_json(env);
                request["custom_terms"]      = qvariant_to_json(custom_terms);
                request["context"]["custom"] = qvariant_to_json(customContext);

                if (not request["context"]["custom"].contains("project_name")) {
                    request["context"]["custom"]["project_name"] =
                        query_engine_.get_project_name(live_link_metadata_);
                }

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }

            request["context"]["epoc"] = utility::to_epoc_milliseconds(utility::clock::now());
            request["context"]["type"] = "result";

            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, request.dump(2));
            try {

                auto data = request_receive_wait<JsonStore>(
                    *sys, backend_, SHOTGRID_TIMEOUT, get_data_atom_v, request);

                // spdlog::warn("{} {}", __PRETTY_FUNCTION__, data.dump(2));

                if (data.at("result").count("errors")) {
                    spdlog::warn(
                        "{} {}", __PRETTY_FUNCTION__, data.at("result").at("errors").dump(2));
                    return QVariant(mapFromValue(R"([])"_json));
                }

                if (data.at("result").at("data").is_array())
                    data["context"]["truncated"] =
                        (static_cast<int>(data.at("result").at("data").size()) ==
                         data.at("max_result"));


                return QVariant(mapFromValue(data));

            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                // silence error..

                if (starts_with(std::string(err.what()), "LiveLink ")) {
                    return QVariant(mapFromValue(request));
                }

                return QVariant(mapFromValue(R"([])"_json));
            }
        }
        return QVariant(mapFromValue(R"([])"_json));
    });
}
