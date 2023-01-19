// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <regex>
#include <algorithm>
#include <set>

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
    DNegMediaHook() : MediaHook("DNeg") {}
    ~DNegMediaHook() override = default;

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

            if (ends_with(path, ".dneg.mov")) {
                // check metadata..
                int slate_frames = 1;

                //"comment": "\nsource frame range: 1001-1101\nsource image:
                // aspect 1.85004516712 crop: l 0.0 r 0.0 b 0.0 t 0.0 slateFrames: 0",
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

        // auto decode_p = decoding_params(path, metadata);
        auto colour_p                     = colour_params(path, metadata);
        result["colour_pipeline"]         = colour_p;
        result["colour_pipeline"]["path"] = path;

        static const std::regex show_shot_regex(
            R"([\/]+(hosts\/*fs*\/user_data[1-9]{0,1}|jobs)\/([^\/]+)\/([^\/]+))");

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

        auto ppath = uri_to_posix_path(uri);
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
        }

        // spdlog::warn("MediaHook Metadata Result {}", result["colour_pipeline"].dump(2));

        return result;
    }

    utility::JsonStore
    colour_params(const std::string &path, const utility::JsonStore &metadata) {
        utility::JsonStore r;

        // OpenColorIO Config and Context variables
        // Unrecognized media will not be colour-managed
        auto context = utility::JsonStore();

        std::smatch match;
        static const std::regex show_shot_regex(
            R"(\/(\/hosts\/*fs*\/user_data|jobs)\/([^\/]+)\/([^\/]+))");
        if (std::regex_search(path, match, show_shot_regex)) {
            context["SHOW"]   = match[2];
            context["SHOT"]   = match[3];
            r["ocio_context"] = context;
            // TODO: ColSci
            // Need to read the general.dat to get the actual OCIO environment variable
            r["ocio_config"] =
                fmt::format("/tools/{}/data/colsci/config.ocio", std::string(match[2]));

            // Input colour space detection
            static const std::regex review_regex(".+\\.review[0-9]\\.mov$");
            static const std::regex internal_regex(".+\\.dneg.mov$");

            const std::string ext = utility::to_lower(fs::path(path).extension());
            static const std::set<std::string> linear_ext{".exr", ".sxr", ".mxr", ".movieproc"};
            static const std::set<std::string> log_ext{".cin", ".dpx"};
            static const std::set<std::string> stills_ext{
                ".png", ".tiff", ".tif", ".jpeg", ".jpg", ".gif"};

            if (std::regex_match(path, review_regex)) {
                r["input_colorspace"] = "dneg_proxy_log:log";
            } else if (std::regex_match(path, internal_regex)) {
                r["input_display"] = "Rec709";
                r["input_view"]    = "Film";
            } else if (linear_ext.find(ext) != linear_ext.end()) {
                r["input_colorspace"] = "linear";
            } else if (log_ext.find(ext) != log_ext.end()) {
                r["input_colorspace"] = "log";
            } else if (stills_ext.find(ext) != stills_ext.end()) {
                r["input_display"] = "sRGB";
                r["input_view"]    = "Film";
            } else {
                r["input_display"] = "Rec709";
                r["input_view"]    = "Film";
            }
        } else {
            r["ocio_config"] = "__raw__";
        }

        // DNEG specific colour plugin behaviour
        // TODO: ColSci
        // Disable when not building DNEG version
        r["viewing_rules"] = true;

        auto dynamic_cdl       = utility::JsonStore();
        dynamic_cdl["primary"] = "GRD_primary";
        dynamic_cdl["neutral"] = "GRD_neutral";
        r["dynamic_cdl"]       = dynamic_cdl;

        return r;
    }
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
