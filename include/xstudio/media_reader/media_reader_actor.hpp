// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/uuid.hpp"

// media actor will be a collection of actors..
// it'll delegate to the correct endpoint..

// this needs to be plugin enbabled
// we also need a way of deciding which plugin should be used..
// make plugins register support
// extension / 16bytes ? buffer match / free function, NATIVE flag to notified prefered plugin
// for collision? use above to decide which plugin to use.. The worker actor should wrap the
// underlying class.

namespace xstudio {
namespace media_reader {

    class GlobalMediaReaderActor : public caf::event_based_actor {
      public:
        GlobalMediaReaderActor(
            caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());
        ~GlobalMediaReaderActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }
        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "GlobalMediaReaderActor";

        void prune_readers();
        bool prune_reader(const std::string &key);

        std::optional<caf::actor>
        check_cached_reader(const std::string &key, const bool preserve = true);
        caf::actor add_reader(
            const caf::actor &reader, const std::string &key, const bool preserve = true);
        std::string reader_key(const caf::uri &_uri, const caf::actor_addr &_key) const;
        caf::actor get_reader(
            const caf::uri &_uri, const caf::actor_addr &_key, const std::string &hint = "");
        void do_precache();

        void keep_cache_hot(
            const media::MediaKey &new_entry,
            const utility::time_point &tp,
            const utility::Uuid &playhead_uuid);

        void read_and_cache_image(
            caf::actor reader,
            const FrameRequest fr,
            const utility::time_point cache_out_of_date_threshold,
            const bool is_background_cache);

        void read_and_cache_audio(
            caf::actor reader,
            const FrameRequest fr,
            const utility::time_point cache_out_of_date_threshold,
            const bool is_background_cache);

        void continue_precacheing();

        void mark_playhead_waiting_for_precache_result(const utility::Uuid &playhead_uuid);

        void mark_playhead_received_precache_result(const utility::Uuid &playhead_uuid);

        void send_error_to_source(const caf::actor_addr &addr, const caf::error &err);

        void process_get_media_detail_queue();

      private:
        caf::actor pool_;
        caf::actor image_cache_;
        caf::actor audio_cache_;
        caf::behavior behavior_;
        utility::Uuid uuid_;
        std::map<std::string, caf::actor> readers_;
        std::map<std::string, utility::time_point> reader_access_;
        std::map<utility::Uuid, std::vector<std::pair<media::MediaKey, utility::time_point>>>
            background_cached_frames_;
        std::map<utility::Uuid, utility::time_point> background_cached_ref_timepoint_;

        std::map<utility::Uuid, int> playheads_with_precache_requests_in_flight_;

        std::vector<caf::actor> plugins_;
        std::map<std::string, utility::Uuid> plugins_map_;

        size_t max_source_count_;
        size_t max_source_age_;

        FrameRequestQueue playback_precache_request_queue_;
        FrameRequestQueue background_precache_request_queue_;
    };

} // namespace media_reader
} // namespace xstudio
