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


bool iequals(const std::string &a, const std::string &b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) {
        return tolower(a) == tolower(b);
    });
}

bool is_exr(const caf::uri &uri) {
    static const std::regex exr_match(R"(.+\.(exr|EXR)$)");
    return std::regex_search(uri_to_posix_path(uri), exr_match);
}

std::string infer_dneg_leaf_name_from_movie_path(const std::string posix_path) {
    if (posix_path.find(".dneg.") != std::string::npos) {
        return std::string("DNEG Movie");
    } else if (posix_path.find(".review0.") != std::string::npos) {
        return std::string("Review Proxy 0");
    } else if (posix_path.find(".review1.") != std::string::npos) {
        return std::string("Review Proxy 1");
    } else if (posix_path.find(".review2.") != std::string::npos) {
        return std::string("Review Proxy 2");
    }
    throw std::runtime_error("Unrecognised DNEG pipeline movie path formatting.");
}

// recursively search 'searchfolder' for a folder called 'target_folder_name',
// stopping if depth is below zero or if a subfolder is in 'exclude_these_subfolders'
std::string recursively_find_matching_folder(
    const std::string searchfolder,
    const std::string target_folder_name,
    const int depth,
    const std::set<std::string> &exclude_these_subfolders) {
    if (depth < 0)
        return std::string();

    for (const auto &entry : fs::directory_iterator(searchfolder)) {
        if (!fs::is_directory(entry.status()))
            continue;
        if (exclude_these_subfolders.count(entry.path().filename().string()))
            continue;
        if (iequals(entry.path().filename().string(), target_folder_name)) {
            // case insensitive match because IVY does random stuff with case
            return entry.path().string();
        } else {
            std::string v = recursively_find_matching_folder(
                entry.path().string(), target_folder_name, depth - 1, exclude_these_subfolders);
            if (!v.empty())
                return v;
        }
    }
    return std::string();
}

// This isn't optimised for speed at all but the vector being sorted has at
// most about 5 entries
auto leaf_sort_func = [](const utility::MediaReference &a,
                         const utility::MediaReference &b) -> bool {
    static const std::regex exr_res_regex(R"(.+\/([0-9]+)x([0-9]+)\/[^\/]+\.(exr|EXR)$)");
    std::smatch m1, m2;
    std::string path1 = uri_to_posix_path(a.uri());
    std::string path2 = uri_to_posix_path(b.uri());

    bool a_is_exr = std::regex_search(path1, m1, exr_res_regex);
    bool b_is_exr = std::regex_search(path2, m2, exr_res_regex);

    if (a_is_exr && b_is_exr) {

        int x1 = atoi(m1[1].str().c_str());
        int y1 = atoi(m1[2].str().c_str());
        int x2 = atoi(m2[1].str().c_str());
        int y2 = atoi(m2[2].str().c_str());

        bool p1_is_orig = path1.find("_orig") != std::string::npos;
        bool p2_is_orig = path2.find("_orig") != std::string::npos;
        if (p1_is_orig && !p2_is_orig)
            return false;
        else if (!p1_is_orig && p2_is_orig)
            return true;
        return (x1 * y1) > (x2 * y2);

    } else if (!a_is_exr && b_is_exr) {

        // puts MOVs at the start of the list
        return true;

    } else if (a_is_exr && !b_is_exr) {

        // puts MOVs at the start of the list
        return false;

    } else {

        // puts DNEG movs at the start of the list

        bool p1_is_dnmov = path1.find(".dneg.") != std::string::npos;
        bool p2_is_dnmov = path2.find(".dneg.") != std::string::npos;
        if (p1_is_dnmov && !p2_is_dnmov)
            return true;
        else if (!p1_is_dnmov && p2_is_dnmov)
            return false;
        else {
            // alphabetical sort
            return path1 < path2;
        }
    }

    return false;
};
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

    std::optional<utility::MediaReference>
    modify_media_reference(const utility::MediaReference &mr, const utility::JsonStore &jsn) override {
        utility::MediaReference result = mr;
        auto changed = false;
        // sneaky ?

        // test paths
        // check for paths without /u/jobs/hosts/tools/apps
        {
            auto path = uri_to_posix_path(mr.uri());

            if (starts_with(path, "/user_data")) {
                // local path
                // add local host name + /hosts
                // validate it's a valid path.
                auto hostname                  = get_host_name();
                path                           = "/hosts/" + hostname + path;
                result.set_uri(posix_path_to_uri(path));
                changed = true;
            }
        }
        // we chomp the first frame if internal movie..
        // why do we come here multiple times ??
        if (not result.frame_list().start() and
            result.container()) {
            auto path = to_string(result.uri().path());

            if (ends_with(path, ".dneg.mov")) {
                // check metadata..
                int slate_frames = 1;

                //"comment": "\nsource frame range: 1001-1101\nsource image: aspect 1.85004516712 crop: l 0.0 r 0.0 b 0.0 t 0.0 slateFrames: 0",
                try {
                    auto comment = jsn.at("metadata").at("media").at("@").at("format").at("tags").at("comment").get<std::string>();
                    // regex..
                    static const std::regex slate_match(R"(.*slateFrames: (\d+).*)");

                    std::smatch m;
                    if(std::regex_search(comment, m, slate_match)){
                        slate_frames = std::atoi(m[1].str().c_str());
                    }

                } catch(...) {}

                while(slate_frames) {
                    auto fr = result.frame_list();
                    if (fr.pop_front()) {
                        result.set_frame_list(fr);
                        result.set_timecode(
                            result.timecode() + 1);
                        changed = true;
                    }
                    slate_frames --;
                }
            }
        }

        if(not changed)
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
        r["viewing_rules"] = true;

        auto dynamic_cdl       = utility::JsonStore();
        dynamic_cdl["primary"] = "GRD_primary";
        dynamic_cdl["neutral"] = "GRD_neutral";
        r["dynamic_cdl"]       = dynamic_cdl;

        return r;
    }

    // std::vector<std::pair<std::string,MediaReference>> gather_media_sources(
    //     const std::vector<utility::MediaReference> & media_refs,
    //     const std::vector<std::string> &media_source_names,
    //     std::string & preferred_source
    //     ) override {

    //     auto tt = utility::clock::now();

    //     std::stringstream ss;
    //     for (const auto &media_ref: media_refs) {
    //         spdlog::debug("DNEG Media Hook gathering sources for {}.",
    //         to_string(media_ref.uri()));
    //     }

    //     // N.B. This is nothing more than a rough demo/placeholder until we
    //     // can execute IVY queries from xstudio. We are doing a filesystem
    //     // search of the shot folder to find quicktimes and EXRs that also match
    //     // the 'existing_source_refs'.

    //     // for now we aren't going to trawl EXR folders, it's proving too slow
    //     const bool full_search = false;

    //     // make a map of the source names to refs so we can re-use if we need to
    //     std::map<caf::uri, std::string> orig_names;
    //     {
    //         auto p = media_refs.begin();
    //         auto q = media_source_names.begin();
    //         while (p != media_refs.end() && q != media_source_names.end()) {
    //             orig_names[(*p).uri()] = *q;
    //             p++;
    //             q++;
    //         }
    //     }

    //     static const std::regex movie_regex(R"((.+\/movie.+)\/([^\.]+)(.+)(mov|MOV|mp4)$)");
    //     static const std::regex
    //     exr_stalk_regex(R"((.+)\/([0-9]+x[0-9]+)\/(.+)\.(exr|EXR)$)");

    //     typedef std::vector<std::pair<caf::uri, FrameList>> UriAndFrameListVec;

    //     std::vector<utility::MediaReference> expanded_sources = media_refs;

    //     for (const auto &ref: media_refs) {

    //         std::string path = uri_to_posix_path(ref.uri());
    //         std::smatch m;

    //         if (std::regex_search(path, m, movie_regex)) {

    //             // search for other movie sources
    //             UriAndFrameListVec others = utility::scan_posix_path(m[1].str(), 1);
    //             for (const auto &o: others) {
    //                 if (uri_to_posix_path(o.first) != path) {
    //                     expanded_sources.emplace_back(
    //                         o.first,
    //                         o.second,
    //                         false,
    //                         ref.rate()
    //                         );
    //                 }
    //             }

    //             if (full_search) {
    //                 // now search for EXR sources by finding the matching stalk
    //                 // directory starting at the same level as the 'movie' folder
    //                 std::string stalkname = m[2].str();
    //                 std::string stemfolder = std::string(path,0,path.find("/movie"));
    //                 std::string stalk_folder = recursively_find_matching_folder(
    //                     stemfolder,
    //                     stalkname,
    //                     2,
    //                     std::set<std::string>({"movie", "source"}));

    //                 if (!stalk_folder.empty()) {
    //                     UriAndFrameListVec others = utility::scan_posix_path(stalk_folder,
    //                     2); for (const auto &o: others) {
    //                         if (uri_to_posix_path(o.first) != path &&
    //                             std::regex_search(uri_to_posix_path(o.first),
    //                             exr_stalk_regex) ) {

    //                             expanded_sources.emplace_back(
    //                                 o.first,
    //                                 o.second,
    //                                 false,
    //                                 ref.rate()
    //                             );
    //                         }
    //                     }
    //                 }
    //             }

    //         } else if (std::regex_search(path, m, exr_stalk_regex)) {

    //             std::string stalk_folder = m[1].str(); // i.e. render/scan folder etc
    //             // search the containing stalk folder for other resolutions (proxies)
    //             if (full_search) {
    //                 UriAndFrameListVec others = utility::scan_posix_path(stalk_folder, 2);
    //                 for (const auto &o: others) {
    //                     if (uri_to_posix_path(o.first) != path &&
    //                         std::regex_search(uri_to_posix_path(o.first), exr_stalk_regex))
    //                     {
    //                         expanded_sources.emplace_back(
    //                             o.first,
    //                             o.second,
    //                             false,
    //                             ref.rate()
    //                         );
    //                     }
    //                 }
    //             }

    //             // this captures /jobs/$SHOW/$SHOT in 1
    //             static const std::regex exr_stalk_regex2(
    //                 R"((.*\/jobs\/[_A-Z0-9]+\/[^\/]+)\/.+\/([0-9]+x[0-9]+)\/([^\.]+).+(exr|EXR)$)"
    //                 );

    //             if (std::regex_search(path, m, exr_stalk_regex2)) {

    //                 // assuming we have a movies folder under the shot folder
    //                 std::string movies_folder = m[1].str() + std::string("/movie");
    //                 std::string stalk_name = m[3].str();

    //                 try {
    //                     // look in the movie folder for subfolder matching the stalk
    //                     // (render) name
    //                     std::string movie_stalk_folder = recursively_find_matching_folder(
    //                         movies_folder,
    //                         stalk_name,
    //                         3,
    //                         std::set<std::string>());

    //                     if (!movie_stalk_folder.empty()) {
    //                         UriAndFrameListVec others =
    //                         utility::scan_posix_path(movie_stalk_folder, 2); for (const auto
    //                         &o: others) {
    //                             if (std::regex_search(uri_to_posix_path(o.first),
    //                             movie_regex)) {
    //                                 expanded_sources.emplace_back(
    //                                     o.first,
    //                                     o.second,
    //                                     false,
    //                                     ref.rate()
    //                                 );
    //                             }
    //                         }
    //                     }
    //                 } catch (std::exception & e) {
    //                      spdlog::warn("DNEG Media Hook {}.", e.what());
    //                 }

    //             }

    //         }

    //     }


    //     // now sort the sources (if any) by their resolutions, or movie names
    //     std::sort(
    //         expanded_sources.begin(),
    //         expanded_sources.end(),
    //         leaf_sort_func);

    //     auto is_orig_res = [](const caf::uri & uri) -> bool{
    //         return uri_to_posix_path(uri).find("_orig.") != std::string::npos;
    //     };

    //     if (!expanded_sources.empty()) {
    //         // remove duplicates, if any
    //         auto p = expanded_sources.begin();
    //         auto pp = expanded_sources.begin();
    //         pp++;
    //         while (pp != expanded_sources.end()) {
    //             if ((*pp).uri() == (*p).uri())  {
    //                 pp = expanded_sources.erase(pp);
    //             } else {
    //                 p++;
    //                 pp++;
    //             }
    //         }
    //     }

    //     // To name the EXR sources I'm using some really basic assumptions -
    //     // the highest res is the main_prox0 ... unless it contiains the string
    //     // '_orig.' which you will see on a SCAN, and this is
    //     int main_idx = 0;
    //     int orig_idx = 0;
    //     int unknown_mov_idx = 1;
    //     media_source_names.clear();

    //     for (const auto &source: expanded_sources) {
    //         std::stringstream ss;
    //         if (is_exr(source.uri())) {
    //             if (is_orig_res(source.uri())) {
    //                 ss << "EXR: OrigProxy " << orig_idx;
    //                 orig_idx++;
    //             } else {
    //                 if (!main_idx) ss << "EXR: Main";
    //                 else ss << "EXR: Proxy " << main_idx;
    //                 main_idx++;
    //             }
    //             media_source_names.push_back(ss.str());
    //         } else {
    //             try {
    //                 media_source_names.push_back(
    //                     infer_dneg_leaf_name_from_movie_path(
    //                         uri_to_posix_path(source.uri())
    //                         )
    //                     );
    //             } catch (...) {
    //                 if (orig_names.find(source.uri()) != orig_names.end()) {
    //                     media_source_names.push_back(orig_names[source.uri()]);
    //                 } else {
    //                     ss << "Other MOV " << unknown_mov_idx++;
    //                     media_source_names.push_back(ss.str());
    //                 }
    //             }
    //         }
    //     }

    //     // compare final list with original list map, and remove dups.
    //     // this whole function needs breaking up..
    //     {
    //         auto es = expanded_sources.begin();
    //         auto en = media_source_names.begin();

    //         for(;es!=expanded_sources.end(); ){
    //             auto esuri = es->uri();
    //             if(orig_names.count(esuri) and orig_names[esuri] == *en) {
    //                 // spdlog::debug("Remove dup {} {}", to_string(esuri),
    //                 orig_names[esuri]); es = expanded_sources.erase(es); en =
    //                 media_source_names.erase(en);
    //             } else {
    //                 es++;
    //                 en++;
    //             }
    //         }
    //     }

    //     spdlog::debug(
    //         "DNEG Media Hook gathering sources for {} took {} milliseconds.",
    //         ss.str(),
    //         std::chrono::duration_cast<std::chrono::milliseconds>(
    //             utility::clock::now()-tt
    //         ).count()
    //         );

    //     media_refs = expanded_sources;
    //     preferred_source = "Review Proxy 1";
    //     return true;
    // }
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
