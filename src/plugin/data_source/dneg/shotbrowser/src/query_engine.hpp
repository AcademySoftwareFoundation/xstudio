// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <nlohmann/json.hpp>
#include <set>

#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/json_store_sync.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "definitions.hpp"

using namespace xstudio;

const auto OperatorTermTemplate = R"({
    "id": null,
    "type": "term",
    "term": "Operator",
    "value": "And",
    "negated": false,
    "enabled": true,
    "children": []
})"_json;

// "dynamic": false,
const auto TermTemplate = R"({
	"id": null,
	"type": "term",
    "term": "",
    "value": "",
    "enabled": true,
    "livelink": null,
    "negated": null
})"_json;

const auto PresetTemplate = R"({
	"id": null,
	"type": "preset",
	"name": "PRESET",
    "hidden": false,
    "favourite": true,
    "update": null,
    "userdata": "",
    "children": []
})"_json;

const auto GroupTemplate = R"({
	"id": null,
	"type": "group",
	"name": "GROUP",
    "hidden": false,
    "favourite": true,
	"entity": "",
    "update": null,
    "userdata": "",
    "children": [
        {
            "id": null,
            "type": "preset",
            "name": "OVERRIDE",
            "update": null,
            "hidden": false,
            "favourite": false,
            "userdata": "",
            "children": []
        },
    	{
    		"id": null,
    		"type": "presets",
    		"children": []
    	}
    ]
})"_json;

const auto RootTemplate = R"({
    "id": null,
    "name": "root",
    "type": "root",
    "children": []
})"_json;

const auto ValidEntities = R"([
    "Playlists",
    "Notes",
    "Versions"
])"_json;

const auto ValidTerms = R"_({
    "Playlists" : [
        "Operator",
        "Author",
        "Department",
        "Disable Global",
        "Exclude Shot Status",
        "Filter",
        "Flag Media",
        "Has Contents",
        "Has Notes",
        "Id",
        "Lookback",
        "Order By",
        "Playlist Type",
        "Preferred Audio",
        "Preferred Visual",
        "Project",
        "Review Location",
        "Result Limit",
        "Shot Status",
        "Site",
        "Tag",
        "Unit"
    ],
    "Notes" : [
        "Operator",
        "Author",
        "Disable Global",
        "Exclude Shot Status",
        "Filter",
        "Flag Media",
        "Has Attachments",
        "Id",
        "Lookback",
        "Newer Version",
        "Note Type",
        "Older Version",
        "Order By",
        "Pipeline Step",
        "Playlist",
        "Preferred Audio",
        "Preferred Visual",
        "Project",
        "Recipient",
        "Result Limit",
        "Sequence",
        "Shot Status",
        "Shot",
        "Tag",
        "Twig Name",
        "Twig Type",
        "Version Name"
    ],
    "Versions" : [
        "Operator",
        "Author",
        "Client Filename",
        "Completion Location",
        "Disable Global",
        "dnTag",
        "Entity",
        "Exclude Shot Status",
        "Filter",
        "Flag Media",
        "Has Notes",
        "Id",
        "Is Hero",
        "Latest Version",
        "Lookback",
        "Newer Version",
        "Older Version",
        "On Disk",
        "Order By",
        "Pipeline Status",
        "Pipeline Step",
        "Playlist",
        "Preferred Audio",
        "Preferred Visual",
        "Production Status",
        "Project",
        "Reference Tag",
        "Reference Tags",
        "Result Limit",
        "Sent To",
        "Sent To Client",
        "Sent To Dailies",
        "Sequence",
        "Shot Status",
        "Shot",
        "Stage",
        "Tag",
        "Tag (Version)",
        "Twig Name",
        "Twig Type",
        "Unit"
    ]
})_"_json;

const auto OnDiskTermValues = R"([
        {"name": "chn"},
        {"name": "lon"},
        {"name": "mtl"},
        {"name": "mum"},
        {"name": "syd"},
        {"name": "van"}
    ])"_json;

const auto ResultLimitTermValues = R"([
        {"name": "1"},
        {"name": "2"},
        {"name": "4"},
        {"name": "8"},
        {"name": "10"},
        {"name": "20"},
        {"name": "40"},
        {"name": "100"},
        {"name": "200"},
        {"name": "500"},
        {"name": "1000"},
        {"name": "2500"},
        {"name": "4500"}
    ])"_json;

const auto LookbackTermValues = R"([
        {"name": "Today"},
        {"name": "1 Day"},
        {"name": "3 Days"},
        {"name": "7 Days"},
        {"name": "20 Days"},
        {"name": "30 Days"},
        {"name": "30-60 Days"},
        {"name": "60-90 Days"},
        {"name": "100-150 Days"},
        {"name": "Future Only"}
    ])"_json;

const auto OrderByTermValues = R"([
        {"name": "Date And Time ASC"},
        {"name": "Date And Time DESC"},
        {"name": "Created ASC"},
        {"name": "Created DESC"},
        {"name": "Updated ASC"},
        {"name": "Updated DESC"},
        {"name": "Client Submit ASC"},
        {"name": "Client Submit DESC"},
        {"name": "Version ASC"},
        {"name": "Version DESC"}
    ])"_json;

const auto BoolTermValues = R"([{"name": "True"},{"name": "False"}])"_json;
const auto SubmittedTermValues =
    R"([{"name": "Any"}, {"name": "Client"}, {"name": "Dailies"}, {"name": "Ignore"}])"_json;
const auto OperatorTermValues = R"([{"name": "And"},{"name": "Or"}])"_json;

const auto FlagTermValues = R"([
        {"name": "Red", "id":"#FFFF0000"},
        {"name": "Green", "id":"#FF00FF00"},
        {"name": "Blue", "id":"#FF0000FF"},
        {"name": "Yellow", "id":"#FFFFFF00"},
        {"name": "Orange", "id":"#FFFFA500"},
        {"name": "Purple", "id":"#FF800080"},
        {"name": "Black", "id":"#FF000000"},
        {"name": "White", "id":"#FFFFFFFF"}
    ])"_json;

const auto SourceTermValues = R"([
        { "name": "SG Movie" },
        { "name": "SG Frames" },
        { "name": "main_proxy0" },
        { "name": "main_proxy1" },
        { "name": "main_proxy2" },
        { "name": "main_proxy3" },
        { "name": "movie_client" },
        { "name": "movie_diagnostic" },
        { "name": "movie_dneg" },
        { "name": "movie_dneg_editorial" },
        { "name": "movie_scqt" },
        { "name": "movie_scqt_prores" },
        { "name": "orig_proxy0" },
        { "name": "orig_proxy1" },
        { "name": "orig_proxy2" },
        { "name": "orig_proxy3" },
        { "name": "orig_main_proxy0" },
        { "name": "orig_main_proxy1" },
        { "name": "orig_main_proxy2" },
        { "name": "orig_main_proxy3" },
        { "name": "review_proxy_1" },
        { "name": "review_proxy_2" }
    ])"_json;

const auto TermProperties = R"_({
    "Author": { "negated": null, "livelink": false },
    "Client Filename": { "negated": false, "livelink": null },
    "Completion Location": { "negated": false, "livelink": null },
    "Department": { "negated": false, "livelink": null },
    "Disable Global": { "negated": null, "livelink": null },
    "dnTag": { "negated": false, "livelink": null },
    "Entity": { "negated": null, "livelink": true },
    "Exclude Shot Status": { "negated": null, "livelink": null },
    "Filter": { "negated": false, "livelink": null },
    "Flag Media": { "negated": null, "livelink": null },
    "Has Attachments": { "negated": null, "livelink": null },
    "Has Contents": { "negated": null, "livelink": null },
    "Has Notes": { "negated": null, "livelink": null },
    "Id": { "negated": false, "livelink": null },
    "Is Hero": { "negated": null, "livelink": null },
    "Latest Version": { "negated": null, "livelink": null },
    "Lookback": { "negated": null, "livelink": null },
    "Newer Version": { "negated": null, "livelink": false },
    "Note Type": { "negated": false, "livelink": null },
    "Older Version": { "negated": null, "livelink": false },
    "On Disk": { "negated": false, "livelink": null },
    "Operator": { "negated": null, "livelink": null, "children": [] },
    "Order By": { "negated": null, "livelink": null },
    "Pipeline Status": { "negated": false, "livelink": null },
    "Pipeline Step": { "negated": false, "livelink": false },
    "Playlist Type": { "negated": false, "livelink": null },
    "Playlist": { "negated": null, "livelink": null },
    "Preferred Audio": { "negated": null, "livelink": false },
    "Preferred Visual": { "negated": null, "livelink": false },
    "Production Status": { "negated": false, "livelink": null },
    "Project": { "negated": null, "livelink": false },
    "Recipient": { "negated": null, "livelink": false },
    "Reference Tag": { "negated": false, "livelink": null },
    "Reference Tags": { "negated": false, "livelink": null },
    "Result Limit": { "negated": null, "livelink": null },
    "Review Location": { "negated": null, "livelink": null },
    "Sent To Client": { "negated": null, "livelink": null },
    "Sent To Dailies": { "negated": null, "livelink": null },
    "Sent To": { "negated": null, "livelink": null },
    "Sequence": { "negated": false, "livelink": false },
    "Shot Status": { "negated": false, "livelink": null },
    "Shot": { "negated": false, "livelink": false },
    "Site": { "negated": false, "livelink": null },
    "Stage": { "negated": null, "livelink": null },
    "Tag (Version)": { "negated": false, "livelink": null },
    "Tag": { "negated": false, "livelink": null },
    "Twig Name": { "negated": false, "livelink": false },
    "Twig Type": { "negated": false, "livelink": false },
    "Unit": { "negated": false, "livelink": null },
    "Version Name": { "negated": false, "livelink": false }
})_"_json;

const std::set<std::string> TermHasProjectKey = {
    "shot",
    "sequence",
    "playlist",
    "unit",
    "stage",
    "user",
    "group",
    "Group",
    "Shot",
    "ShotSequence",
    "ShotSequenceList",
    "Sequence",
    "Stage",
    "Unit",
    "User",
    "Author",
    "Recipient",
    "Playlist"};

const std::set<std::string> TermHasNoModel = {
    "Client Filename",
    "dnTag",
    "Entity",
    "Filter",
    "Id",
    "Newer Version",
    "Older Version",
    "Tag (Version)",
    "Tag",
    "Twig Name",
    "Version Name"};

typedef std::function<void(const std::string &key)> CacheChangedFunc;

class QueryEngine {
  public:
    QueryEngine();
    virtual ~QueryEngine() = default;

    static utility::JsonStore build_query(
        const int project_id,
        const std::string &entity,
        const utility::Uuid &group_id,
        const utility::JsonStore &group_terms,
        const utility::JsonStore &terms,
        const utility::JsonStore &custom_terms,
        const utility::JsonStore &context,
        const utility::JsonStore &metadata,
        const utility::JsonStore &env,
        const utility::JsonStore &lookup);

    static utility::JsonStore merge_query(
        const utility::JsonStore &base,
        const utility::JsonStore &override,
        const bool ignore_duplicates = true);

    static shotgun_client::Text add_text_value(
        const std::string &filter, const std::string &value, const bool negated = false);

    static void add_term_to_filter(
        const std::string &entity,
        const utility::JsonStore &term,
        const int project_id,
        const utility::JsonStore &lookup,
        shotgun_client::FilterBy *qry);

    static utility::JsonStore resolve_query_value(
        const std::string &type,
        const utility::JsonStore &value,
        const utility::JsonStore &lookup) {
        return resolve_query_value(type, value, -1, lookup);
    }

    static utility::JsonStore resolve_query_value(
        const std::string &type,
        const utility::JsonStore &value,
        const int project_id,
        const utility::JsonStore &lookup);

    static utility::JsonStore resolve_attribute_value(
        const std::string &type,
        const utility::JsonStore &value,
        const int project_id,
        const utility::JsonStore &lookup);

    static utility::JsonStore resolve_attribute_value(
        const std::string &type,
        const utility::JsonStore &value,
        const utility::JsonStore &lookup) {
        return resolve_attribute_value(type, value, -1, lookup);
    }


    static std::string cache_name(const std::string &type, const int project_id = -1) {
        auto _type = type;

        if (project_id != -1)
            _type += "-" + std::to_string(project_id);

        return _type;
    }

    static utility::JsonStore validate_presets(
        const utility::JsonStore &data,
        const bool is_user               = false,
        const utility::JsonStore &parent = utility::JsonStore(),
        const size_t index               = 0,
        const bool export_as_system      = false);


    static std::string cache_name_auto(const std::string &type, const int project_id);
    static utility::JsonStore data_from_field(const utility::JsonStore &data);

    static std::vector<std::string> get_sequence_name(
        const int project_id, const int shot_id, const utility::JsonStore &lookup);

    static utility::JsonStore get_livelink_value(
        const std::string &term,
        const utility::JsonStore &metadata,
        const utility::JsonStore &lookup);

    static std::string
    get_shot_name(const utility::JsonStore &metadata, const bool strict = false) {
        auto name = std::string();

        try {
            std::vector<std::string> locations(
                {{"/metadata/external/DNeg/shot"},
                 {"/metadata/external/ivy/file/version/scope/name"},
                 {"/metadata/shotgun/version/relationships/entity/data/name"},
                 {"/metadata/shotgun/shot/attributes/code"},
                 {"/metadata/image_source_metadata/metadata/external/DNeg/shot"},
                 {"/metadata/audio_source_metadata/metadata/external/DNeg/shot"},
                 {"/metadata/image_source_metadata/metadata/external/ivy/file/version/scope/"
                  "name"},
                 {"/metadata/audio_source_metadata/metadata/external/ivy/file/version/scope/"
                  "name"},
                 {"/metadata/image_source_metadata/colour_pipeline/ocio_context/SHOT"}});
            for (const auto &i : locations) {
                if (metadata.contains(json::json_pointer(i))) {
                    if (strict and i == locations.at(2) and
                        metadata.value(
                            json::json_pointer(
                                "/metadata/shotgun/version/relationships/entity/data/type"),
                            std::string()) != "Shot")
                        continue;
                    name = metadata.at(json::json_pointer(i)).get<std::string>();
                    break;
                }
            }
        } catch (...) {
        }

        return name;
    }

    static std::string get_version_name(const utility::JsonStore &metadata);

    static std::string get_project_name(const utility::JsonStore &metadata) {
        auto project_name = std::string();

        try {
            std::vector<std::string> project_name_locations(
                {{"/metadata/external/DNeg/show"},
                 {"/metadata/external/ivy/file/show"},
                 {"/metadata/shotgun/version/relationships/project/data/name"},
                 {"/metadata/shotgun/shot/relationships/project/data/name"},
                 {"/metadata/image_source_metadata/metadata/external/DNeg/show"},
                 {"/metadata/audio_source_metadata/metadata/external/DNeg/show"},
                 {"/metadata/image_source_metadata/metadata/external/ivy/file/show"},
                 {"/metadata/audio_source_metadata/metadata/external/ivy/file/show"},
                 {"/metadata/image_source_metadata/colour_pipeline/ocio_context/SHOW"}});
            for (const auto &i : project_name_locations) {
                if (metadata.contains(json::json_pointer(i))) {
                    project_name = metadata.at(json::json_pointer(i)).get<std::string>();
                    break;
                }
            }
        } catch (...) {
        }

        return project_name;
    }

    int get_project_id(const utility::JsonStore &metadata) {
        return get_project_id(metadata, cache_);
    }

    static int
    get_project_id(const utility::JsonStore &metadata, const utility::JsonStore &cache) {
        auto project_id = 0;

        try {
            std::vector<std::string> project_id_locations(
                {{"/metadata/shotgun/version/relationships/project/data/id"},
                 {"/metadata/shotgun/shot/relationships/project/data/id"}});
            for (const auto &i : project_id_locations) {
                if (metadata.contains(json::json_pointer(i))) {
                    project_id = metadata.at(json::json_pointer(i)).get<int>();
                    break;
                }
            }

            if (not project_id) {
                // use project cache to get id from name.
                auto name = get_project_name(metadata);
                if (not name.empty()) {
                    auto key = QueryEngine::cache_name("project");
                    if (cache.count(key)) {
                        for (const auto &i : cache.at(key)) {
                            if (i.at("attributes").at("name") == name) {
                                project_id = i.at("id").get<int>();
                                break;
                            }
                        }
                    }
                }
            }
        } catch (...) {
        }

        return project_id;
    }

    static void set_lookup(
        const std::string &key, const utility::JsonStore &data, utility::JsonStore &lookup);

    void set_lookup(const std::string &key, const utility::JsonStore &data);

    static void set_shot_sequence_lookup(
        const std::string &key, const utility::JsonStore &data, utility::JsonStore &lookup);

    void set_shot_sequence_lookup(const std::string &key, const utility::JsonStore &data);

    static void set_shot_sequence_list_cache(
        const std::string &key, const utility::JsonStore &data, utility::JsonStore &cache);

    void set_shot_sequence_list_cache(const std::string &key, const utility::JsonStore &data);

    std::optional<utility::JsonStore> get_cache(const std::string &key) const;
    std::optional<utility::JsonStore> get_lookup(const std::string &key) const;
    void set_cache(const std::string &key, const utility::JsonStore &data);

    utility::JsonStore &lookup() { return lookup_; }
    utility::JsonStore &cache() { return cache_; }
    utility::JsonStoreSync &user_presets() { return user_presets_; }
    utility::JsonStoreSync &system_presets() { return system_presets_; }

    void initialise_presets();
    void set_presets(const utility::JsonStore &user, const utility::JsonStore &system);
    void merge_presets(nlohmann::json &destination, const nlohmann::json &source);

    const utility::Uuid &user_uuid() const { return user_uuid_; }
    const utility::Uuid &system_uuid() const { return system_uuid_; }

    template <typename T>
    static T to_value(const nlohmann::json &jsn, const std::string &key, const T &fallback);

    static std::set<std::string>
    precache_needed(const int project_id, const utility::JsonStore &lookup);

    static utility::JsonStore apply_livelinks(
        const utility::JsonStore &terms,
        const utility::JsonStore &metadata,
        const utility::JsonStore &lookup);

    static void replace_with_env(
        nlohmann::json &value, const utility::JsonStore &env, const utility::JsonStore &lookup);

    static utility::JsonStore apply_env(
        const utility::JsonStore &terms,
        const utility::JsonStore &env,
        const utility::JsonStore &lookup);

    static utility::JsonStore get_default_env();

    static void regenerate_ids(nlohmann::json &data);

    static std::optional<nlohmann::json>
    find_by_id(const utility::Uuid &uuid, const utility::JsonStore &presets);

    void bindCacheChangedFunc(CacheChangedFunc ccf) {
        cache_changed_callback_ = [ccf](auto &&PH1) {
            return ccf(std::forward<decltype(PH1)>(PH1));
        };
    }

    void bindLookupChangedFunc(CacheChangedFunc ccf) {
        lookup_changed_callback_ = [ccf](auto &&PH1) {
            return ccf(std::forward<decltype(PH1)>(PH1));
        };
    }

  private:
    void merge_group(nlohmann::json &destination, const nlohmann::json &source);

    bool preset_diff(const nlohmann::json &a, const nlohmann::json &b);

    // handle expansion into OR/AND
    static utility::JsonStore preprocess_terms(
        const utility::JsonStore &terms,
        const std::string &entity,
        utility::JsonStore &query,
        const utility::JsonStore &lookup,
        const utility::JsonStore &metadata,
        const bool and_mode = true,
        const bool initial  = true);

    static shotgun_client::FilterBy terms_to_query(
        const utility::JsonStore &terms,
        const int project_id,
        const std::string &entity,
        const utility::JsonStore &lookup,
        const bool and_mode = true,
        const bool initial  = true);

    static void add_playlist_term_to_filter(
        const std::string &term,
        const std::string &value,
        const bool negated,
        const int project_id,
        const utility::JsonStore &lookup,
        shotgun_client::FilterBy *qry);

    static void add_version_term_to_filter(
        const std::string &term,
        const std::string &value,
        const bool negated,
        const int project_id,
        const utility::JsonStore &lookup,
        shotgun_client::FilterBy *qry);

    static void add_note_term_to_filter(
        const std::string &term,
        const std::string &value,
        const bool negated,
        const int project_id,
        const utility::JsonStore &lookup,
        shotgun_client::FilterBy *qry);

    utility::JsonStoreSync user_presets_;
    utility::JsonStoreSync system_presets_;

    utility::JsonStore cache_;
    utility::JsonStore lookup_;

    utility::Uuid user_uuid_   = utility::Uuid::generate();
    utility::Uuid system_uuid_ = utility::Uuid::generate();

    CacheChangedFunc cache_changed_callback_{nullptr};
    CacheChangedFunc lookup_changed_callback_{nullptr};
};
