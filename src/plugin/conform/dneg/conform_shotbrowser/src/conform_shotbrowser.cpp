// SPDX-License-Identifier: Apache-2.0

#include "xstudio/conform/conformer.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/timeline/track_actor.hpp"
#include "xstudio/utility/json_store_sync.hpp"

#include "../../../../data_source/dneg/shotbrowser/src/query_engine.hpp"

using namespace xstudio;
using namespace xstudio::conform;
using namespace xstudio::utility;
using namespace std::chrono_literals;

class ShotbrowserConform : public Conformer {
  public:
    ShotbrowserConform(const utility::JsonStore &prefs = utility::JsonStore())
        : Conformer(prefs) {}
    ~ShotbrowserConform() = default;


    void update_preferences(const utility::JsonStore &prefs) override {
        try {
            purge_sequence_on_import_ = global_store::preference_value<bool>(
                prefs, "/plugin/conformer/shotbrowser/purge_sequence_on_import");
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }


    std::vector<std::string> conform_tasks() override { return visible_tasks_; }

    bool update_tasks(const utility::JsonStoreSync &presets) {
        auto result = false;
        std::vector<std::string> tasks;
        std::vector<std::string> visible_tasks;
        std::map<std::string, utility::Uuid> task_uuids;

        // find entry with uuid  == b6e0ca0e-2d23-4b1d-a33a-761596183d5f
        try {
            auto task_group =
                presets.find_first("id", Uuid("b6e0ca0e-2d23-4b1d-a33a-761596183d5f"));
            if (task_group) {
                // collect preset names.
                for (const auto &i :
                     presets.at(*task_group).at("children").at(1).at("children")) {
                    if (i.value("hidden", false))
                        continue;

                    task_uuids[i.value("name", "")] = i.value("id", utility::Uuid());
                    tasks.emplace_back(i.value("name", ""));

                    if (not i.value("favourite", true))
                        continue;

                    visible_tasks.emplace_back(i.value("name", ""));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        if (tasks != tasks_ or visible_tasks_ != visible_tasks) {
            visible_tasks_ = visible_tasks;
            tasks_         = tasks;
            task_uuids_    = task_uuids;
            result         = true;
        }

        return result;
    }

    std::optional<utility::Uuid> get_task_id(const std::string &name) const {
        if (task_uuids_.count(name))
            return task_uuids_.at(name);

        return {};
    }

    std::optional<utility::Uuid> get_cut_query_id(const utility::JsonStoreSync &presets) const {
        try {
            auto task_group =
                presets.find_first("id", Uuid("86439af3-16be-4a2f-89f3-ee1e5810ae47"));
            if (task_group) {
                // collect preset names.
                for (const auto &i :
                     presets.at(*task_group).at("children").at(1).at("children")) {
                    if (i.value("hidden", false))
                        continue;

                    if (not i.value("favourite", true))
                        continue;

                    return i.value("id", utility::Uuid());
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return {};
    }

    [[nodiscard]] bool purge_sequence_on_import() const { return purge_sequence_on_import_; }

  private:
    std::vector<std::string> tasks_;
    std::vector<std::string> visible_tasks_;
    std::map<std::string, utility::Uuid> task_uuids_;
    bool purge_sequence_on_import_{true};
};

template <typename T> class ShotbrowserConformActor : public caf::event_based_actor {
  public:
    ShotbrowserConformActor(
        caf::actor_config &cfg, const utility::JsonStore &prefs = utility::JsonStore())
        : caf::event_based_actor(cfg), conform_(prefs) {
        spdlog::debug("Created ShotbrowserConformActor");
        utility::print_on_exit(this, "ShotbrowserConformActor");

        {
            auto prefs = global_store::GlobalStoreHelper(system());
            utility::JsonStore js;
            utility::join_broadcast(this, prefs.get_group(js));
            conform_.update_preferences(js);
        }

        // we need to subscribe to the shotbrowsers preset model


        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            // conform media into clips, doesn't require shot browser.. as we only key off
            // show/shot.
            [=](conform_atom, const ConformRequest &crequest) -> result<ConformReply> {
                auto rp = make_response_promise<ConformReply>();
                conform_request(rp, crequest);
                return rp;
            },

            [=](conform_atom,
                const std::string &conform_task,
                const ConformRequest &crequest) -> result<ConformReply> {
                auto rp = make_response_promise<ConformReply>();
                conform_task_request(rp, conform_task, crequest);
                return rp;
            },

            [=](conform_atom,
                const UuidActor &timeline,
                const bool only_create_conform_track) -> result<bool> {
                // get timeline detail.
                auto rp = make_response_promise<bool>();
                prepare_timeline(rp, timeline, only_create_conform_track);
                return rp;
            },

            [=](conform_tasks_atom) -> std::vector<std::string> {
                setup();
                conform_.update_tasks(presets_);
                return conform_.conform_tasks();
            },

            [=](conform_atom,
                const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &media)
                -> result<std::vector<std::optional<std::pair<std::string, caf::uri>>>> {
                auto rp = make_response_promise<
                    std::vector<std::optional<std::pair<std::string, caf::uri>>>>();
                conform_media(rp, media);
                return rp;
            },

            [=](conform_atom,
                const std::string &key,
                const std::pair<utility::UuidActor, utility::JsonStore> &needle,
                const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &haystack)
                -> result<utility::UuidActorVector> {
                auto rp = make_response_promise<utility::UuidActorVector>();
                find_matching(rp, key, needle, haystack);
                return rp;
            },

            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {
                if (uuid == user_preset_event_id_) {
                    presets_.process_event(event);
                    if (conform_.update_tasks(presets_))
                        anon_send(
                            system().registry().template get<caf::actor>(conform_registry),
                            conform_tasks_atom_v);
                }
            },

            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {
                delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
            },

            [=](json_store::update_atom, const utility::JsonStore &js) {
                try {
                    conform_.update_preferences(js);
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            });
    }


    ~ShotbrowserConformActor() override = default;
    caf::behavior make_behavior() override { return behavior_; }

  private:
    void conform_task_request(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        const ConformRequest &crequest) {
        try {

            if (not connected_) {
                setup();
                conform_.update_tasks(presets_);
            }

            // spdlog::warn("conform_request {} {}", conform_task,
            // conform_detail.dump(2)); spdlog::warn("conform_request {}",
            // crequest.playlist_json_.dump(2));

            if (crequest.items_.empty()) {
                rp.deliver(ConformReply(crequest));
            } else {
                auto query_id = conform_.get_task_id(conform_task);

                if (query_id) {
                    // build a query....

                    auto shotbrowser =
                        system().registry().template get<caf::actor>("SHOTBROWSER");

                    if (not shotbrowser)
                        throw std::runtime_error("Failed to find shotbrowser");


                    auto shotgrid_count = std::make_shared<size_t>(crequest.items_.size());
                    auto shotgrid_results =
                        std::make_shared<std::vector<UuidActorVector>>(crequest.items_.size());

                    // dispatch requests for shotgrid data.
                    for (size_t i = 0; i < crequest.items_.size(); i++) {
                        auto metadata =
                            crequest.metadata_.at(crequest.items_.at(i).item_.uuid());

                        // clip metadata has precedence
                        auto media_uuid = metadata.value("media_uuid", Uuid());
                        if (not media_uuid.is_null() and crequest.metadata_.count(media_uuid)) {
                            auto tmp               = crequest.metadata_.at(media_uuid);
                            auto sg_twig_type_code = nlohmann::json::json_pointer(
                                "/metadata/shotgun/version/attributes/sg_twig_type_code");
                            if (tmp.value(sg_twig_type_code, "") != "cut" and
                                tmp.value(sg_twig_type_code, "") != "edl") {
                                tmp.update(metadata, true);
                                metadata = tmp;
                                // spdlog::warn("{}", metadata.dump(2));
                            }
                            // metadata.update(crequest.metadata_.at(media_uuid));
                        }

                        auto project_id = QueryEngine::get_project_id(metadata, JsonStore());

                        // if (not project_id)
                        //     throw std::runtime_error("Failed to find project_id");
                        // here we go....

                        auto req          = JsonStore(GetExecutePreset);
                        req["project_id"] = project_id;
                        req["preset_id"]  = *query_id;
                        req["metadata"]   = metadata;
                        req["context"]    = R"({
                            "type": null,
                            "epoc": null,
                            "audio_source": [],
                            "visual_source": [],
                            "flag_text": "",
                            "flag_colour": "",
                            "truncated": false
                        })"_json;

                        // req["env"]          = qvariant_to_json(env);
                        // req["custom_terms"] = qvariant_to_json(custom_terms);

                        // req["context"]["epoc"] =
                        // utility::to_epoc_milliseconds(utility::clock::now());
                        // req["context"]["type"] = "result";

                        request(shotbrowser, infinite, data_source::get_data_atom_v, req)
                            .then(
                                [=](const JsonStore &result) mutable {
                                    request(
                                        shotbrowser,
                                        infinite,
                                        playlist::add_media_atom_v,
                                        result,
                                        crequest.container_.uuid(),
                                        crequest.container_.actor(),
                                        crequest.items_.at(i).before_)
                                        .then(
                                            [=](const UuidActorVector &new_media) mutable {
                                                (*shotgrid_results)[i] = new_media;
                                                (*shotgrid_count)--;
                                                if (not *shotgrid_count)
                                                    process_results(
                                                        rp, *shotgrid_results, crequest);
                                            },
                                            [=](caf::error &err) mutable {
                                                (*shotgrid_count)--;
                                                if (not *shotgrid_count)
                                                    process_results(
                                                        rp, *shotgrid_results, crequest);
                                            });
                                },
                                [=](caf::error &err) mutable {
                                    (*shotgrid_count)--;
                                    if (not *shotgrid_count)
                                        process_results(rp, *shotgrid_results, crequest);
                                });
                    }
                } else {
                    spdlog::warn(
                        "{} Failed to find query id {}", __PRETTY_FUNCTION__, conform_task);
                    rp.deliver(ConformReply(crequest));
                }
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            rp.deliver(make_error(xstudio_error::error, err.what()));
        }
    }

    void conform_request(
        caf::typed_response_promise<ConformReply> rp, const ConformRequest &crequest) {
        try {
            auto creply = ConformReply(crequest);
            auto clips  = crequest.template_tracks_.at(0).find_all_items(timeline::IT_CLIP);

            // build clip lookup.
            std::map<Uuid, std::string> clip_project_map;
            std::map<Uuid, std::string> clip_shot_map;

            // remap start frame if requested via metadata..
            for (auto &t : creply.request_.template_tracks_) {
                for (auto &c : t.find_all_items(timeline::IT_CLIP)) {

                    auto item_meta = c.get().prop();
                    const auto cut_start_ptr =
                        json::json_pointer("/metadata/external/DNeg/cut/start");
                    const auto override_cut_ptr =
                        json::json_pointer("/metadata/external/DNeg/cut/override");
                    const auto override_comp_ptr =
                        json::json_pointer("/metadata/external/DNeg/comp/override");

                    if (item_meta.contains(override_cut_ptr) and
                        item_meta.at(override_cut_ptr).get<bool>() and
                        item_meta.contains(cut_start_ptr) and
                        not item_meta.at(cut_start_ptr).is_null()) {
                        // get item trimmed start frame
                        auto trimmed_range = c.get().trimmed_range();

                        // spdlog::warn("conform_request {} {} {} {}",
                        // item_meta.at(cut_start_ptr).get<int>(),
                        // trimmed_range.rate().to_seconds(),
                        // trimmed_range.rate().to_fps(),trimmed_range.rate().count());

                        item_meta.at(override_cut_ptr)  = false;
                        item_meta.at(override_comp_ptr) = false;
                        trimmed_range.set_start(FrameRate(
                            trimmed_range.rate() * item_meta.at(cut_start_ptr).get<int>()));

                        c.get().set_prop(item_meta);
                        c.get().set_active_range(trimmed_range);
                    }
                }
            }


            for (const auto &c : clips) {
                auto clip_uuid  = c.get().uuid();
                auto media_uuid = c.get().prop().value("media_uuid", Uuid());

                // spdlog::warn("{}", c.get().prop().dump(2));

                auto clip_project =
                    QueryEngine::get_project_name(crequest.metadata_.at(clip_uuid));
                auto clip_shot = QueryEngine::get_shot_name(crequest.metadata_.at(clip_uuid));

                if (clip_project.empty() and not media_uuid.is_null())
                    clip_project =
                        QueryEngine::get_project_name(crequest.metadata_.at(media_uuid));
                if (clip_shot.empty() and not media_uuid.is_null())
                    clip_shot = QueryEngine::get_shot_name(crequest.metadata_.at(media_uuid));

                clip_project_map[clip_uuid] = clip_project;
                clip_shot_map[clip_uuid]    = clip_shot;

                if (clip_project.empty() or clip_shot.empty()) {
                    spdlog::warn(
                        "Clip metadata not found, {} project: '{}', 'shot':  {}",
                        c.get().name(),
                        clip_project,
                        clip_shot);
                }
                // else {
                //     spdlog::warn("CLIP {} {} {}", to_string(clip_uuid), clip_project,
                //     clip_shot);
                // }
            }

            std::set<utility::Uuid> matched_clips;
            const auto only_one_match =
                crequest.operations_.value("only_one_clip_match", false);

            auto clip_track_uuid = Uuid();

            // we're matching media to clips.
            for (const auto &i : crequest.items_) {
                // find match in clips..
                // get show shot..
                if (clip_track_uuid != i.clip_track_uuid_) {
                    matched_clips.clear();
                    clip_track_uuid = i.clip_track_uuid_;
                }

                if (clips.empty()) {
                    spdlog::warn("No clips found on selected conform track.");
                    creply.items_.push_back({});
                } else {
                    try {
                        // spdlog::warn("GET MEDIA SHOW/SHOT {} {}",
                        // to_string(std::get<0>(i).uuid()),
                        // crequest.metadata_.count(std::get<0>(i).uuid()));
                        const auto meta = crequest.metadata_.at(i.item_.uuid());
                        auto project    = QueryEngine::get_project_name(meta);
                        auto shot       = QueryEngine::get_shot_name(meta);

                        if (project.empty() or shot.empty()) {
                            creply.items_.push_back({});
                            spdlog::warn(
                                "Media is missing metadata, {} project: {}, shot: {}",
                                to_string(i.item_.uuid()),
                                project,
                                shot);
                        } else {
                            auto ritems = std::vector<ConformReplyItem>();

                            for (const auto &c : clips) {
                                auto clip_uuid = c.get().uuid();
                                if ((not only_one_match or
                                     not matched_clips.count(clip_uuid)) and
                                    clip_project_map.at(clip_uuid) == project and
                                    clip_shot_map.at(clip_uuid) == shot) {
                                    ritems.push_back(std::make_tuple(c.get().uuid_actor()));
                                    matched_clips.insert(clip_uuid);
                                    if (only_one_match)
                                        break;
                                }
                            }
                            if (ritems.empty()) {
                                spdlog::warn(
                                    "Media has no matching clip {} project: {}, shot:  "
                                    "{}",
                                    to_string(i.item_.uuid()),
                                    project,
                                    shot);
                                creply.items_.push_back({});
                            } else
                                creply.items_.push_back(ritems);
                        }

                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        creply.items_.push_back({});
                    }
                }
            }

            rp.deliver(creply);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            rp.deliver(make_error(xstudio_error::error, err.what()));
        }
    }

    void prepare_timeline(
        caf::typed_response_promise<bool> rp,
        const UuidActor &timeline,
        const bool only_create_conform_track) {

        scoped_actor sys{system()};
        try {
            auto found_project = std::string();
            auto timeline_item =
                request_receive<timeline::Item>(*sys, timeline.actor(), timeline::item_atom_v);


            auto timeline_path = timeline_item.prop().value("path", std::string());
            if (not timeline_path.empty()) {
                std::cmatch m;
                auto uri_path = caf::make_uri(timeline_path);
                const auto SHOW_REGEX =
                    std::regex(R"(^(?:/jobs|/hosts/[^/]+/user_data\d*)/([A-Z0-9]+)/.+$)");

                if (uri_path) {
                    auto posix_path = uri_to_posix_path(*uri_path);
                    if (std::regex_match(posix_path.c_str(), m, SHOW_REGEX)) {
                        found_project = m[1];
                    }
                }
            }
            // process timeline..
            // purge empty tracks.

            auto video_tracks = timeline_item.find_all_items(timeline::IT_VIDEO_TRACK);
            auto insert_index = static_cast<int>(video_tracks.size());
            auto vcount       = video_tracks.size();
            if (not only_create_conform_track) {
                for (auto &i : video_tracks) {
                    if (i.get().empty() and vcount > 1) {
                        auto pactor = find_parent_actor(timeline_item, i.get().uuid());
                        if (pactor) {
                            insert_index--;
                            vcount--;
                            request_receive<JsonStore>(
                                *sys,
                                pactor,
                                timeline::erase_item_atom_v,
                                i.get().uuid(),
                                true);
                        }
                    }
                }

                auto audio_tracks = timeline_item.find_all_items(timeline::IT_AUDIO_TRACK);
                auto acount       = audio_tracks.size();
                for (auto &i : audio_tracks) {
                    if (i.get().empty() and acount > 1) {
                        auto pactor = find_parent_actor(timeline_item, i.get().uuid());
                        if (pactor) {
                            acount--;
                            request_receive<JsonStore>(
                                *sys,
                                pactor,
                                timeline::erase_item_atom_v,
                                i.get().uuid(),
                                true);
                        }
                    }
                }
            }
            // create a new track with empty clips based off markers and scan track..
            // populate clips with metadata required to conform timeline
            auto vtrack = timeline::Item(timeline::IT_NONE);
            std::reverse(video_tracks.begin(), video_tracks.end());

            for (const auto &i : video_tracks) {
                if (not i.get().empty()) {
                    vtrack = i.get();
                    break;
                }
            }

            vtrack.set_name("Conform Track");
            vtrack.set_locked(true);
            vtrack.set_enabled(false);
            // populate vtrack name/metadata
            if (not only_create_conform_track)
                anon_send(vtrack.actor(), timeline::item_lock_atom_v, true);

            auto media_metadata = std::map<Uuid, JsonStore>();
            auto tframe         = timeline_item.trimmed_start();
            const auto trate    = timeline_item.rate();

            auto unknown_name = std::string("UNKNOWN");

            for (auto &i : vtrack.children()) {
                if (i.item_type() == timeline::IT_CLIP) {
                    auto is_valid   = false;
                    auto comp_start = R"(null)"_json;
                    auto comp_end   = R"(null)"_json;
                    auto cut_start  = R"(null)"_json;
                    auto cut_end    = R"(null)"_json;
                    auto project    = R"(null)"_json;
                    auto shot       = R"(null)"_json;

                    auto pm =
                        R"({"metadata": {"external": {"DNeg": {"shot": null, "show":null, "comp": {"start": null, "end":null, "override": true}, "cut": {"start": null, "end":null, "override":true}}}}})"_json;

                    // from check clip metadata (FEAT ANIM)
                    if (not is_valid and not found_project.empty()) {
                        auto cm = i.prop();
                        if (cm.contains("media_stalk_dnuuid")) {
                            project = found_project;

                            cut_start = i.trimmed_frame_start().frames();
                            cut_end   = i.trimmed_frame_start().frames() +
                                      i.trimmed_frame_duration().frames() - 1;

                            shot     = i.name();
                            is_valid = true;
                            // if (auto tmp = cm.value("shot_label", ""); not tmp.empty()) {
                            //     shot     = tmp;
                            //     is_valid = true;
                            // }
                        }
                    }

                    // from turnover auto markers
                    if (not is_valid) {
                        // marker should have same start time as clip..
                        // markers exist on stack..
                        for (const auto &m : timeline_item.children().front().markers()) {
                            if (m.start() == tframe) {
                                if (m.prop().contains("comp")) {
                                    if (auto tmp = m.prop().at("comp").value(
                                            "start", std::numeric_limits<int>::max());
                                        tmp != std::numeric_limits<int>::max())
                                        comp_start = tmp;
                                    if (auto tmp = m.prop().at("comp").value(
                                            "end", std::numeric_limits<int>::max());
                                        tmp != std::numeric_limits<int>::max())
                                        comp_end = tmp;
                                }

                                if (m.prop().contains("cut")) {
                                    if (auto tmp = m.prop().at("cut").value(
                                            "start", std::numeric_limits<int>::max());
                                        tmp != std::numeric_limits<int>::max())
                                        cut_start = tmp;
                                    if (auto tmp = m.prop().at("cut").value(
                                            "end", std::numeric_limits<int>::max());
                                        tmp != std::numeric_limits<int>::max())
                                        cut_end = tmp;
                                }

                                if (auto tmp = m.prop().value("show", found_project);
                                    not tmp.empty())
                                    project = tmp;

                                if (auto tmp = m.prop().value("shot", ""); not tmp.empty()) {
                                    shot = tmp;
                                    // got shot close enough...
                                    is_valid = true;
                                }
                            }

                            if (is_valid)
                                break;
                        }
                    }

                    // from premiere markers
                    if (not is_valid) {

                        // need a list of clips at this point in time backed down.
                        // do we need to check markers..
                        // marker should have same start time as clip..
                        // markers exist on stack..
                        const auto fcpp = json::json_pointer("/fcp_xml/comment");

                        for (const auto &m : timeline_item.children().front().markers()) {
                            if (m.start() == tframe) {
                                if (m.prop().contains(fcpp) and
                                    m.prop().at(fcpp).is_string() and not m.name().empty()) {
                                    const static auto cutcompre =
                                        std::regex("\\s*(\\d+)\\s*,\\s*(\\d+)\\s*-\\s*(\\d+)"
                                                   "\\s*,\\s*(\\d+)\\s*");
                                    auto comment = m.prop().at(fcpp).get<std::string>();
                                    std::cmatch match;
                                    if (std::regex_match(comment.c_str(), match, cutcompre)) {
                                        auto start_frame = std::stoi(match[2]);
                                        i.set_active_range(FrameRange(
                                            FrameRate(start_frame * trate.to_flicks()),
                                            i.trimmed_duration(),
                                            i.rate()));
                                        i.set_available_range(*i.active_range());

                                        comp_start = std::stoi(match[1]);
                                        cut_start  = std::stoi(match[2]);
                                        cut_end    = std::stoi(match[3]);
                                        comp_end   = std::stoi(match[4]);
                                        shot       = m.name();
                                        project    = found_project;
                                        is_valid   = true;
                                    } else {
                                        // spdlog::warn("no match {}", comment);
                                    }
                                }

                                if (is_valid)
                                    break;
                            }
                        }
                    }

                    // from media metadata
                    if (not is_valid) {
                        auto items = timeline_item.resolve_time_raw(tframe);

                        for (const auto &j : items) {
                            auto clip = j.first;

                            try {
                                project = QueryEngine::get_project_name(clip.prop());
                                shot    = QueryEngine::get_shot_name(clip.prop(), true);

                                if (project.get<std::string>().empty() or
                                    shot.get<std::string>().empty()) {
                                    // try media metadata..
                                    auto media_uuid = clip.prop().value("media_uuid", Uuid());

                                    if (not media_uuid.is_null() and
                                        not media_metadata.count(media_uuid)) {
                                        try {
                                            auto tries    = 10;
                                            auto metadata = JsonStore();
                                            while (true) {
                                                metadata = request_receive<JsonStore>(
                                                    *sys,
                                                    clip.actor(),
                                                    playlist::get_media_atom_v,
                                                    json_store::get_json_atom_v,
                                                    Uuid(),
                                                    "");
                                                tries--;
                                                if (not tries or not metadata.is_null())
                                                    break;
                                                std::this_thread::sleep_for(200ms);
                                                // spdlog::warn("retry");
                                            }

                                            if (metadata.is_null())
                                                spdlog::warn(
                                                    "No metadata {} {} {}",
                                                    clip.name(),
                                                    metadata.dump(2),
                                                    tries);

                                            media_metadata[media_uuid] = metadata;
                                        } catch (const std::exception &err) {
                                            spdlog::warn(
                                                "{} {} {}",
                                                __PRETTY_FUNCTION__,
                                                clip.name(),
                                                err.what());
                                            media_metadata[media_uuid] = R"({})"_json;
                                        }
                                    }

                                    if (project.get<std::string>().empty())
                                        project = QueryEngine::get_project_name(
                                            media_metadata.at(media_uuid));
                                    if (shot.get<std::string>().empty())
                                        shot = QueryEngine::get_shot_name(
                                            media_metadata.at(media_uuid), true);
                                }

                                if (not project.get<std::string>().empty())
                                    found_project = project;

                                if (not project.get<std::string>().empty() and
                                    not shot.get<std::string>().empty()) {
                                    cut_start = clip.trimmed_frame_start().frames();
                                    cut_end   = clip.trimmed_frame_start().frames() +
                                              clip.trimmed_frame_duration().frames() - 1;

                                    i.set_active_range(FrameRange(
                                        clip.trimmed_start(), i.trimmed_duration(), i.rate()));
                                    i.set_available_range(*i.active_range());
                                    is_valid = true;
                                    break;
                                }
                            } catch (const std::exception &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                            }
                        }
                    }

                    // validate shot name ?
                    const static std::regex valid_shot_re(R"(^[a-zA-Z0-9_]+$)");
                    if (not shot.is_null() and
                        not std::regex_match(shot.get<std::string>(), valid_shot_re))
                        is_valid = false;

                    // update clips with results
                    if (not is_valid) {
                        auto meta                                              = i.prop();
                        pm[json::json_pointer("/metadata/external/DNeg/shot")] = unknown_name;
                        pm[json::json_pointer("/metadata/external/DNeg/show")] =
                            found_project.empty() ? unknown_name : found_project;

                        meta.update(pm, true);

                        if (not only_create_conform_track) {
                            anon_send(i.actor(), timeline::item_prop_atom_v, meta);
                            anon_send(i.actor(), timeline::item_name_atom_v, unknown_name);
                            anon_send(i.actor(), timeline::item_flag_atom_v, "#FFFF0000");
                        }

                        i.set_prop(meta);
                        i.set_name(unknown_name);
                        i.set_flag("#FFFF0000");
                        i.set_enabled(false);
                    } else {
                        pm[json::json_pointer("/metadata/external/DNeg/show")] = project;
                        pm[json::json_pointer("/metadata/external/DNeg/shot")] = shot;
                        pm[json::json_pointer("/metadata/external/DNeg/comp/start")] =
                            comp_start;
                        pm[json::json_pointer("/metadata/external/DNeg/comp/end")]  = comp_end;
                        pm[json::json_pointer("/metadata/external/DNeg/cut/start")] = cut_start;
                        pm[json::json_pointer("/metadata/external/DNeg/cut/end")]   = cut_end;

                        if (found_project.empty())
                            found_project = project;

                        auto meta = i.prop();
                        meta.update(pm, true);

                        i.set_prop(meta);
                        i.set_name(shot);
                        i.set_flag("#FF00FF00");
                        i.set_enabled(true);

                        if (not only_create_conform_track) {
                            anon_send(i.actor(), timeline::item_prop_atom_v, meta);
                            anon_send(
                                i.actor(), timeline::item_name_atom_v, shot.get<std::string>());
                            anon_send(i.actor(), timeline::item_flag_atom_v, "#FF00FF00");
                        }
                    }
                }
                tframe += i.trimmed_duration();
            }

            if (only_create_conform_track) {
                // clean before adding
                vtrack.reset_uuid(true);
                vtrack.reset_actor(true);
                vtrack.reset_media_uuid();
                auto vua = UuidActor(vtrack.uuid(), spawn<timeline::TrackActor>(vtrack));

                request_receive<JsonStore>(
                    *sys,
                    timeline_item.children().front().actor(),
                    timeline::insert_item_atom_v,
                    insert_index,
                    UuidActorVector({vua}));
            }

            auto tprop                  = timeline_item.prop();
            tprop["conform_track_uuid"] = vtrack.uuid();
            request_receive<JsonStore>(
                *sys, timeline_item.actor(), timeline::item_prop_atom_v, tprop);

            // purge other video tracks
            if (not only_create_conform_track and conform_.purge_sequence_on_import() and
                vcount > 1) {

                request_receive<JsonStore>(
                    *sys,
                    timeline_item.children().front().actor(),
                    timeline::erase_item_atom_v,
                    0,
                    static_cast<int>(vcount - 1),
                    true);
            }

            // we shouldn't return until the main timeline item is fully sync'd with it's
            // children. how do we tell ?

            if (not only_create_conform_track) {
                // we get here when this function is called by the system
                // we might be chaining a conform after this..
                // so we need to be in sync..
                // request item
                auto done = false;
                while (not done) {
                    try {
                        auto conform_item = request_receive<timeline::Item>(
                            *sys, timeline.actor(), timeline::item_atom_v, vtrack.uuid());
                        done = true;
                        for (const auto &i : conform_item) {
                            // all clips in the base video track should be red or green..
                            if (i.item_type() == timeline::IT_CLIP and
                                not(i.flag() == "#FFFF0000" or i.flag() == "#FF00FF00")) {
                                done = false;
                                spdlog::warn("Waiting for timeline to sync. {}", timeline_path);
                                std::this_thread::sleep_for(1s);
                                break;
                            }
                        }
                    } catch (...) {
                        spdlog::warn("can't find conform track");
                        break;
                    }
                }
                spdlog::warn("Timeline is ready. {}", timeline_path);
            }

            rp.deliver(true);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            rp.deliver(false);
        }
    }

    void conform_media(
        caf::typed_response_promise<
            std::vector<std::optional<std::pair<std::string, caf::uri>>>> rp,
        const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &media) {

        // for(const auto &i: media) {
        //     spdlog::warn("{}", i.second.dump(2));
        // }

        try {

            if (not connected_) {
                setup();
                conform_.update_tasks(presets_);
            }

            auto query_id = conform_.get_cut_query_id(presets_);

            auto shotbrowser = system().registry().template get<caf::actor>("SHOTBROWSER");

            if (not shotbrowser)
                throw std::runtime_error("Failed to find shotbrowser");

            if (query_id) {

                auto shotgrid_count   = std::make_shared<size_t>(media.size());
                auto shotgrid_results = std::make_shared<
                    std::vector<std::optional<std::pair<std::string, caf::uri>>>>(media.size());

                auto count = 0;
                for (const auto &i : media) {
                    auto project_id = QueryEngine::get_project_id(i.second, JsonStore());

                    auto req          = JsonStore(GetExecutePreset);
                    req["project_id"] = project_id;
                    req["preset_id"]  = *query_id;
                    req["metadata"]   = i.second;
                    req["context"]    = R"({
                        "type": null,
                        "epoc": null,
                        "audio_source": [],
                        "visual_source": [],
                        "flag_text": "",
                        "flag_colour": "",
                        "truncated": false
                    })"_json;

                    request(shotbrowser, infinite, data_source::get_data_atom_v, req)
                        .then(
                            [=](const JsonStore &result) mutable {
                                auto jpath =
                                    nlohmann::json::json_pointer("/result/data/0/attributes");
                                auto spath = nlohmann::json::json_pointer(
                                    "/result/data/0/relationships/entity/data");
                                if (result.contains(jpath)) {
                                    auto otiopath = result.at(jpath).value(
                                        "sg_path_to_frames", std::string());
                                    auto last_dot = otiopath.find_last_of(".");
                                    otiopath      = otiopath.substr(0, last_dot);

                                    // need to test for optimised otio..
                                    if (not fs::exists(otiopath + "_optimised.otio"))
                                        otiopath += ".otio";
                                    else
                                        otiopath += "_optimised.otio";

                                    auto name = result.at(spath).value("name", std::string());

                                    try {
                                        auto uri = posix_path_to_uri(otiopath);
                                        (*shotgrid_results)[count] = std::make_pair(name, uri);
                                    } catch (...) {
                                        (*shotgrid_results)[count] = {};
                                    }
                                } else
                                    (*shotgrid_results)[count] = {};

                                // spdlog::warn("{}",result.dump(2));

                                (*shotgrid_count)--;
                                if (not *shotgrid_count)
                                    rp.deliver(*shotgrid_results);
                            },
                            [=](caf::error &err) mutable {
                                (*shotgrid_count)--;
                                if (not *shotgrid_count)
                                    rp.deliver(*shotgrid_results);
                            });
                    count++;
                }
            } else {
                spdlog::warn("{} Sequence preset not found", __PRETTY_FUNCTION__);
                rp.deliver(
                    std::vector<std::optional<std::pair<std::string, caf::uri>>>(media.size()));
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            rp.deliver(make_error(xstudio_error::error, err.what()));
        }
    }


    void setup() {
        if (not connected_) {
            sb_actor_ = system().registry().template get<caf::actor>("SHOTBROWSER");
            if (sb_actor_) {
                scoped_actor sys{system()};
                try {
                    auto uuids =
                        request_receive<UuidVector>(*sys, sb_actor_, json_store::sync_atom_v);
                    user_preset_event_id_ = uuids[0];

                    // get system presets
                    auto data = request_receive<JsonStore>(
                        *sys, sb_actor_, json_store::sync_atom_v, user_preset_event_id_);
                    presets_ = JsonStoreSync(data);


                    // join events.
                    if (preset_events_) {
                        try {
                            request_receive<bool>(
                                *sys, preset_events_, broadcast::leave_broadcast_atom_v, this);
                        } catch (const std::exception &) {
                        }
                        preset_events_ = caf::actor();
                    }
                    try {
                        preset_events_ = request_receive<caf::actor>(
                            *sys, sb_actor_, get_event_group_atom_v);
                        request_receive<bool>(
                            *sys, preset_events_, broadcast::join_broadcast_atom_v, this);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                    connected_ = true;
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            } else {
                // spdlog::warn("NOT CONNECTED");
            }
        }
    }


    void process_results(
        caf::typed_response_promise<ConformReply> rp,
        const std::vector<UuidActorVector> &results,
        const ConformRequest &crequest) {
        auto creply = ConformReply(crequest);

        // adjust the clip start frame if it doesn't match the clip metadata
        // to support using the client movie as the conform track, which will have useless
        // trimmed clip start frames. item_ should be a clone ?
        if (crequest.item_type_ == "Clip") {
            scoped_actor sys{system()};

            for (auto &i : creply.request_.items_) {
                // check item_metadata
                const auto &item_meta = creply.request_.metadata_.at(i.item_.uuid());
                const auto cut_start_ptr =
                    json::json_pointer("/metadata/external/DNeg/cut/start");
                const auto override_cut_ptr =
                    json::json_pointer("/metadata/external/DNeg/cut/override");
                const auto override_comp_ptr =
                    json::json_pointer("/metadata/external/DNeg/comp/override");

                if (item_meta.contains(override_cut_ptr) and
                    item_meta.at(override_cut_ptr).get<bool>() and
                    item_meta.contains(cut_start_ptr) and
                    not item_meta.at(cut_start_ptr).is_null()) {
                    // get item trimmed start frame
                    auto prop = request_receive<JsonStore>(
                        *sys, i.item_.actor(), timeline::item_prop_atom_v);
                    auto trimmed_range = request_receive<FrameRange>(
                        *sys, i.item_.actor(), timeline::trimmed_range_atom_v);

                    // spdlog::warn("conform_request {} {} {} {}",
                    // item_meta.at(cut_start_ptr).get<int>(),
                    // trimmed_range.rate().to_seconds(),
                    // trimmed_range.rate().to_fps(),trimmed_range.rate().count());

                    prop.at(override_cut_ptr)  = false;
                    prop.at(override_comp_ptr) = false;
                    trimmed_range.set_start(FrameRate(
                        trimmed_range.rate() * item_meta.at(cut_start_ptr).get<int>()));

                    anon_send(
                        i.item_.actor(),
                        timeline::trimmed_range_atom_v,
                        trimmed_range,
                        trimmed_range);
                    anon_send(i.item_.actor(), timeline::item_prop_atom_v, prop);
                }

                // spdlog::warn("{}", creply.request_.metadata_.at(i.item_.uuid()).dump(2));
            }
        }

        for (const auto &i : results) {
            auto ritems = std::vector<ConformReplyItem>();

            for (const auto &j : i)
                ritems.emplace_back(std::make_tuple(j));

            creply.items_.push_back(ritems);
        }

        creply.operations_["create_media"] = true;
        creply.operations_["insert_media"] = true;

        rp.deliver(creply);
    }

    void find_matching(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const std::string &key,
        const std::pair<utility::UuidActor, utility::JsonStore> &needle,
        const std::vector<std::pair<utility::UuidActor, utility::JsonStore>> &haystack) {
        // spdlog::warn("{}", needle.second.dump(2));

        auto result = utility::UuidActorVector();
        auto pointer =
            nlohmann::json::json_pointer("/metadata/shotgun/version/attributes/sg_ivy_dnuuid");

        for (const auto &i : haystack) {
            if (i.first.uuid() == needle.first.uuid())
                continue;

            try {
                if (i.second.value(pointer, Uuid()) == needle.second.value(pointer, Uuid()))
                    result.push_back(i.first);
            } catch (...) {
            }

            // spdlog::warn("{}", i.second.dump(2));
        }


        rp.deliver(result);
    }

  private:
    caf::behavior behavior_;
    T conform_;
    utility::Uuid user_preset_event_id_;
    utility::JsonStoreSync presets_;
    caf::actor sb_actor_;
    caf::actor preset_events_;
    bool connected_{false};
};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<ConformPlugin<ShotbrowserConformActor<ShotbrowserConform>>>(
                Uuid("ebeecb15-75c0-4aa2-9cc7-1b3ad2491c39"),
                "DNeg",
                "DNeg",
                "DNeg Shotbrowser Conformer",
                semver::version("1.0.0"))}));
}
}
