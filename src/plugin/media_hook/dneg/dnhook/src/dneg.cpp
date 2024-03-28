// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <regex>
#include <algorithm>
#include <set>

#include <OpenColorIO/OpenColorIO.h> //NOLINT

#include "xstudio/media_hook/media_hook.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"

namespace fs = std::filesystem;

using namespace xstudio;
using namespace xstudio::media_hook;
using namespace xstudio::utility;

std::optional<std::string> find_stalk_uuid_from_path(const std::string &path) {
    // .stalk_ec2df42f-d4c5-45e3-bb19-e6626e213f2b
    static const std::regex stalk_regex(
        R"(^.stalk_([0-9a-f]{8,8}-[0-9a-f]{4,4}-[0-9a-f]{4,4}-[0-9a-f]{4,4}-[0-9a-f]{12,12})$)");
    // scan directory for .stalk file..
    try {
        for (const auto &entry : fs::directory_iterator(path)) {
            auto name = entry.path().filename().string();
            if (starts_with(name, ".stalk_")) {
                std::smatch match;
                if (std::regex_search(name, match, stalk_regex)) {
                    return match[1].str();
                }
            }
        }
    } catch (...) {
    }
    return {};
}

std::optional<std::string> find_stalk_uuid(const std::string &path) {
    // /jobs/SGE/094_bge_1005/SCAN/S_094_bge_1005_bg01_s01_00/505x266/S_094_bge_1005_bg01_s01_00.1049.exr
    static const std::regex resolution_regex(
        R"(^\/?((\/hosts\/[a-z]+fs[0-9]+\/user_data[1-9]{0,1}|\/jobs)\/[^\/]+\/[^\/]+\/.+?)\/+\d+x\d+[^\/]*\/+[^\/]+$)");
    static const std::regex prefix_regex(
        R"(^\/?((\/hosts\/[a-z]+fs[0-9]+\/user_data[1-9]{0,1}|\/jobs)\/[^\/]+\/[^\/]+)\/(.+?)\/([^\/]+)$)");
    static const std::regex filename_regex(R"(^([^_]+)_([^.]+).*$)");

    std::smatch match;
    if (std::regex_search(path, match, resolution_regex)) {
        auto stalk = find_stalk_uuid_from_path(match[1].str());
        if (stalk)
            return *stalk;
    }

    // /jobs/SGE/094_bge_1005
    // movie/cgr/CG_094_bge_1005_fx_pulseHandcuff_NOHO_07_L010_BEAUTY_v002
    // CG_094_bge_1005_fx_pulseHandcuff_NOHO_07_L010_BEAUTY_v002.dneg.mov
    if (std::regex_search(path, match, prefix_regex)) {
        std::smatch nmatch;
        auto filename = match[4].str();
        if (std::regex_search(filename, nmatch, filename_regex)) {
            auto prefix = nmatch[1].str();
            if (prefix == "S")
                prefix = "SCAN";
            else if (prefix == "O")
                prefix = "OUT";
            else if (prefix == "E")
                prefix = "ELEMENT";

            auto stalk = find_stalk_uuid_from_path(
                match[1].str() + "/" + prefix + "/" + nmatch[1].str() + "_" + nmatch[2].str());
            if (stalk)
                return *stalk;
        }
    }

    return {};
}


class DNegMediaHook : public MediaHook {
  public:
    DNegMediaHook() : MediaHook("DNeg") {

        auto_trim_slate_ = add_boolean_attribute("Auto Trim Slate", "Auto Trim Slate", true);

        auto_trim_slate_->set_preference_path("/plugin/dneg_media_hook/auto_trim_slate");
        auto_trim_slate_->expose_in_ui_attrs_group("media_hook_settings");
        auto_trim_slate_->set_tool_tip(
            "Some DNEG pipeline movies have metadata indicating if there is a slate frame. "
            "Enable this option to use the metadata to automatically trim the slate.");

        slate_trim_behaviour_ = add_string_choice_attribute(
            "Default Trim Slate Behaviour",
            "Trim Slate",
            "Trim First Frame",
            {"Trim First Frame", "Don't Trim First Frame"});

        slate_trim_behaviour_->set_tool_tip(
            "If Auto Trim Slate is disabled or if the movie slate metadata is not found you "
            "can force the removal of the first movie frame with this option.");

        slate_trim_behaviour_->set_preference_path(
            "/plugin/dneg_media_hook/default_trim_slate_behaviour");
        slate_trim_behaviour_->expose_in_ui_attrs_group("media_hook_settings");
    }
    ~DNegMediaHook() override = default;

    module::StringChoiceAttribute *slate_trim_behaviour_;
    module::BooleanAttribute *auto_trim_slate_;

    std::optional<utility::MediaReference> modify_media_reference(
        const utility::MediaReference &mr, const utility::JsonStore &jsn) override {
        utility::MediaReference result = mr;
        auto changed                   = false;
        // sneaky ?

        // test paths
        // check for paths without /u/jobs/hosts/tools/apps
        {
            auto path = uri_to_posix_path(mr.uri());

            if (starts_with(path, "/user_data")) {
                // local path
                // add local host name + /hosts
                // validate it's a valid path.
                auto hostname = get_host_name();
                path          = "/hosts/" + hostname + path;
                result.set_uri(posix_path_to_uri(path));
                changed = true;
            }
        }
        // we chomp the first frame if internal movie..
        // why do we come here multiple times ??
        if (not result.frame_list().start() and result.container()) {
            auto path = to_string(result.uri().path());

            if (ends_with(path, ".dneg.mov") or ends_with(path, ".dneg.webm")) {
                // check metadata..
                int slate_frames = -1;


                //"comment": "\nsource frame range: 1001-1101\nsource image:
                // aspect 1.85004516712 crop: l 0.0 r 0.0 b 0.0 t 0.0 slateFrames: 0",
                if (auto_trim_slate_->value()) {
                    try {
                        auto comment = jsn.at("metadata")
                                           .at("media")
                                           .at("@")
                                           .at("format")
                                           .at("tags")
                                           .at("comment")
                                           .get<std::string>();

                        // regex..
                        static const std::regex slate_match(R"(.*slateFrames: (\d+).*)");

                        std::smatch m;
                        if (std::regex_search(comment, m, slate_match)) {
                            slate_frames = std::atoi(m[1].str().c_str());
                        }

                    } catch (...) {
                    }
                }

                if (slate_frames == -1) {
                    // auto trim slate is disabled OR there was no metadata
                    // to do the auto trimming
                    if (slate_trim_behaviour_->value() == "Trim First Frame") {
                        slate_frames = 1;
                    } else {
                        slate_frames = 0;
                    }
                }

                while (slate_frames) {
                    auto fr = result.frame_list();
                    if (fr.pop_front()) {
                        result.set_frame_list(fr);
                        result.set_timecode(result.timecode() + 1);
                        changed = true;
                    }
                    slate_frames--;
                }
            }
        }

        if (not changed)
            return {};

        return result;
    }

    /*
    Use media path to infer values relating to the ocio colour pipeline,
    in particular we need to know the show, shot and colourspace of the
    media. Also, given one or more source paths find other matching source
    paths to fill out a set of 'leafs' from one stalk.

    Currently, exr is linear, .review*.mov is log, and dneg.mov is in
    film emulation Rec709 space.
    */

    utility::JsonStore modify_metadata(
        const utility::MediaReference &mr, const utility::JsonStore &metadata) override {
        utility::JsonStore result = metadata;

        const caf::uri &uri =
            mr.container() or mr.uris().empty() ? mr.uri() : mr.uris()[0].first;

        const std::string path = to_string(uri);
        auto ppath             = uri_to_posix_path(uri);


        // utility::JsonStore j(R"(
        //     {
        //         "metadata": {
        //             "external": {
        //                 "DNeg": {
        //                     "Ivy": {
        //                     }
        //                 }
        //             }
        //         }
        //     }
        // )"_json);

        // auto decode_p = decoding_params(path, metadata);
        auto colour_p                     = colour_params(path, metadata);
        result["colour_pipeline"]         = colour_p;
        result["colour_pipeline"]["path"] = path;


        static const std::regex show_shot_regex(
            R"([\/]+(hosts\/\w+fs\w+\/user_data[1-9]{0,1}|jobs)\/([^\/]+)\/([^\/]+))");
        static const std::regex show_shot_alternative_regex(R"(.+-([^-]+)-([^-]+).dneg.webm$)");

        std::smatch match;

        if (std::regex_search(ppath, match, show_shot_regex)) {
            result["metadata"]["external"]["DNeg"]["show"] = match[2];
            result["metadata"]["external"]["DNeg"]["shot"] = match[3];
            // if we've got this much we might be able to get the ivy_stalk_uuid..
            // movies are sometimes stored in a different path outside the stalk dir..
            auto stalk = find_stalk_uuid(ppath);
            if (stalk) {
                result["metadata"]["external"]["DNeg"]["Ivy"]["dnuuid"] = *stalk;
            }
        } else if (std::regex_search(ppath, match, show_shot_alternative_regex)) {
            result["metadata"]["external"]["DNeg"]["show"] = match[1];
            result["metadata"]["external"]["DNeg"]["shot"] = match[2];
            // if we've got this much we might be able to get the ivy_stalk_uuid..
            // movies are sometimes stored in a different path outside the stalk dir..
            auto stalk = find_stalk_uuid(ppath);
            if (stalk) {
                result["metadata"]["external"]["DNeg"]["Ivy"]["dnuuid"] = *stalk;
            }
        }

        if (metadata.contains(nlohmann::json::json_pointer("/metadata/timeline/dneg"))) {
            //     "dnuuid": "b32f9c30-9c18-4e93-ac11-98ae1c685273",
            //     "job": "NECRUS",
            //     "shot": "00TS_0020"
            const auto &dneg =
                metadata.at(nlohmann::json::json_pointer("/metadata/timeline/dneg"));
            if (dneg.count("job"))
                result["metadata"]["external"]["DNeg"]["show"] = dneg.at("job");
            if (dneg.count("shot"))
                result["metadata"]["external"]["DNeg"]["shot"] = dneg.at("shot");
            if (dneg.count("dnuuid"))
                result["metadata"]["external"]["DNeg"]["Ivy"]["dnuuid"] = dneg.at("dnuuid");
        }

        // spdlog::warn("MediaHook Metadata Result {}", result["colour_pipeline"].dump(2));
        // spdlog::warn("MediaHook Metadata Result {}", result.dump(2));

        return result;
    }

    utility::JsonStore
    colour_params(const std::string &path, const utility::JsonStore &metadata) {
        utility::JsonStore r(R"({})"_json);

        // OpenColorIO Config and Context variables
        // Unrecognized media will not be colour-managed
        auto context = R"({})"_json;

        std::smatch match;

        static const std::regex show_shot_regex(
            R"([\/]+(hosts\/\w+fs\w+\/user_data[1-9]{0,1}|jobs)\/([^\/]+)\/([^\/]+))");

        static const std::regex show_shot_alternative_regex(
            R"(.+-([^-]+)-([^-]+)\.dneg\.webm$)");

        if (std::regex_search(path, match, show_shot_regex)) {
            context["SHOW"] = match[2];
            context["SHOT"] = match[3];
        } else if (std::regex_search(path, match, show_shot_alternative_regex)) {
            context["SHOW"] = match[1];
            context["SHOT"] = match[2];
        }

        if (context.count("SHOW")) {
            r["ocio_context"] = context;

            // Detect OCIO config path
            const std::string default_config =
                fmt::format("/tools/{}/data/colsci/config.ocio", context["SHOW"]);
            const std::string ocio_config =
                get_showvar_or(context["SHOW"], "OCIO", default_config);
            r["ocio_config"] = ocio_config;

            // Detect the pipeline version of config
            const std::string pipeline_version =
                get_showvar_or(context["SHOW"], "DN_COLOR_PIPELINE_VERSION", "1");
            r["pipeline_version"] = pipeline_version;

            bool is_cms1_config = pipeline_version == "2";

            // Detect override to active displays and views
            const std::string active_displays =
                get_showvar_or(context["SHOW"], "DN_REVIEW_XSTUDIO_OCIO_ACTIVE_DISPLAYS", "");
            if (!active_displays.empty()) {
                r["active_displays"] = active_displays;
            }

            std::string active_views =
                get_showvar_or(context["SHOW"], "DN_REVIEW_XSTUDIO_OCIO_ACTIVE_VIEWS", "");
            if (!active_views.empty()) {
                r["active_views"] = active_views;
            }
            const auto views = utility::split(active_views, ':');
            const bool has_untonemapped_view = std::find(
                views.begin(), views.end(), "Un-tone-mapped") != views.end();


            // Input media category detection
            static const std::regex review_regex(".+\\.review[0-9]\\.mov$");
            static const std::regex internal_regex(".+\\.dneg.mov$");

            const std::string ext = utility::to_lower(fs::path(path).extension());
            static const std::set<std::string> linear_ext{".exr", ".sxr", ".mxr", ".movieproc"};
            static const std::set<std::string> log_ext{".cin", ".dpx"};
            static const std::set<std::string> stills_ext{
                ".png", ".tiff", ".tif", ".jpeg", ".jpg", ".gif"};

            std::string input_category = "unknown";

            if (std::regex_match(path, review_regex)) {
                input_category = "review_proxy";
            } else if (std::regex_match(path, internal_regex)) {
                input_category = "internal_movie";
            } else if (path.find("/edit_ref/") != std::string::npos) {
                input_category = "edit_ref";
            } else if (linear_ext.find(ext) != linear_ext.end()) {
                input_category = "linear_media";
            } else if (log_ext.find(ext) != log_ext.end()) {
                input_category = "log_media";
            } else if (stills_ext.find(ext) != stills_ext.end()) {
                input_category = "still_media";
            } else {
                input_category = "movie_media";
            }

            r["input_category"] = input_category;

            // Input colour space detection

            // Extract OCIO metadata from internal and review proxy movies.
            std::string media_colorspace;
            std::string media_display;
            std::string media_view;

            if (std::regex_match(path, review_regex) ||
                std::regex_match(path, internal_regex)) {
                try {
                    const utility::JsonStore &tags =
                        metadata.at("metadata").at("media").at("@").at("format").at("tags");

                    media_colorspace = tags.get_or("dneg/ocio_colorspace", std::string(""));
                    media_display    = tags.get_or("dneg/ocio_display", std::string(""));
                    media_view       = tags.get_or("dneg/ocio_view", std::string(""));
                } catch (...) {
                }
            }

            // Note that we prefer using input_colorspace when possible,
            // this maps better to the UI source colour space menu.

            // Except for specific cases, we convert the source to scene_linear
            r["working_space"] = "scene_linear";

            // If we have OCIO metadata, use it to derive the input space
            if (!media_colorspace.empty()) {
                r["input_colorspace"] = media_colorspace;
            } else if (!media_display.empty() && !media_view.empty()) {
                r["input_colorspace"] = media_view + "_" + media_display;
            } else if (input_category == "review_proxy") {
                r["input_colorspace"] = "dneg_proxy_log:log";
                // LBP review proxy before CMS1 migration (no metadata)
                // http://jira/browse/CLR-2006
                if (context["SHOW"] == "LBP") {
                    r["input_colorspace"] = "log_ARRIWideGamut_ARRILogC3";
                }
            } else if (input_category == "internal_movie") {
                // LBP internal movie before CMS1 migration (no metadata)
                // http://jira/browse/CLR-2006
                if (context["SHOW"] == "LBP") {
                    r["input_colorspace"] = "Client_Rec709";
                } else if (is_cms1_config) {
                    r["input_colorspace"] = "DNEG_Rec709";
                } else {
                    r["input_display"] = "Rec709";
                    r["input_view"]    = "Film";
                }
            } else if (input_category == "edit_ref") {
                if (is_cms1_config or has_untonemapped_view) {
                    r["input_colorspace"] = "disp_Rec709-G24";
                    r["working_space"]    = "display_linear";
                    r["automatic_view"]   = "Un-tone-mapped";
                } else {
                    r["input_display"]  = "Rec709";
                    r["input_view"]     = "Film";
                    r["automatic_view"] = "Film";
                }
            } else if (input_category == "linear_media") {
                r["input_colorspace"] = "scene_linear:linear";
            } else if (input_category == "log_media") {
                r["input_colorspace"] = "compositing_log:log";
            } else if (input_category == "still_media") {
                if (is_cms1_config) {
                    r["input_colorspace"] = "DNEG_sRGB";
                    r["automatic_view"]   = "DNEG";
                } else {
                    r["input_display"]  = "sRGB";
                    r["input_view"]     = "Film";
                    r["automatic_view"] = "Film";
                }
            } else if (input_category == "movie_media") {
                if (is_cms1_config or has_untonemapped_view) {
                    r["input_colorspace"] = "disp_Rec709-G24";
                    r["working_space"]    = "display_linear";
                    r["automatic_view"]   = "Un-tone-mapped";
                } else {
                    r["input_display"]  = "Rec709";
                    r["input_view"]     = "Film";
                    r["automatic_view"] = "Film";
                }
            }

            // Detect automatic view assignment in case not found yet
            if (!r.count("automatic_view")) {
                if (path.find("/ASSET/") != std::string::npos) {
                    r["automatic_view"] = "DNEG";
                } else if (
                    path.find("/out/") != std::string::npos ||
                    path.find("/ELEMENT/") != std::string::npos) {
                    r["automatic_view"] = is_cms1_config ? "Client graded" : "Film primary";
                } else {
                    r["automatic_view"] = is_cms1_config ? "Client" : "Film";
                }
            }

            // Detect grading CDLs slots to upgrade as GradingPrimary
            auto dynamic_cdl       = utility::JsonStore();
            dynamic_cdl["primary"] = is_cms1_config ? "$GRD_PRIMARY" : "GRD_primary";
            dynamic_cdl["neutral"] = is_cms1_config ? "$GRD_NEUTRAL" : "GRD_neutral";
            r["dynamic_cdl"]       = dynamic_cdl;

            // Enable DNEG display detection rules
            r["viewing_rules"] = true;

        } else {
            r["ocio_config"] = "__raw__";
            r["working_space"] = "raw";
        }

        return r;
    }

    std::string get_showvar_or(
        const std::string &show, const std::string &variable, const std::string &default_val) {

        const auto &variables = read_general_dat_cached(show);
        if (variables.count(variable)) {
            return variables.at(variable);
        }

        return default_val;
    }

    std::map<std::string, std::string> read_general_dat_cached(const std::string &show) {

        auto p = show_variables_store_.find(show);
        if (p == show_variables_store_.end()) {
            show_variables_store_[show] = read_general_dat(show);
        }

        return show_variables_store_[show];
    }

    std::map<std::string, std::string> read_general_dat(const std::string &show) {

        std::map<std::string, std::string> variables;

        try {
            std::ifstream ifs(fmt::format("/tools/{}/data/general.dat", show));
            if (!ifs.is_open())
                return {};

            const std::string lines(std::istreambuf_iterator<char>{ifs}, {});
            for (const auto &line : utility::split(lines, '\n')) {
                if (line.empty() or utility::starts_with(line, "#"))
                    continue;

                const auto pos = line.find(":");
                if (pos != std::string::npos) {
                    const auto key   = utility::trim(line.substr(0, pos));
                    const auto value = utility::trim(line.substr(pos + 1));
                    variables[key]   = value;
                }
            }
        } catch (const std::exception &e) {
            // pass
        }

        return variables;
    }

    std::map<std::string, std::map<std::string, std::string>> show_variables_store_;
};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaHookPlugin<MediaHookActor<DNegMediaHook>>>(
                Uuid("e4e1d569-2338-4e6e-b127-5a9688df161a"),
                "DNeg",
                "DNeg",
                "DNeg Media Hook",
                semver::version("1.0.0"))}));
}
}
