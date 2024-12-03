// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <limits>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/tree.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace media {
    class MediaActor : public caf::event_based_actor {
      public:
        // MediaActor(
        //     caf::actor_config &cfg,
        //     const std::string &name            = "Unnamed",
        //     const utility::Uuid &uuid          = utility::Uuid(),
        //     caf::actor submedia                = caf::actor(),
        //     const utility::Uuid &submedia_uuid = utility::Uuid());

        MediaActor(
            caf::actor_config &cfg,
            const std::string &name              = "Unnamed",
            const utility::Uuid &uuid            = utility::Uuid(),
            const utility::UuidActorVector &srcs = utility::UuidActorVector());

        MediaActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn, const bool async = false);
        ~MediaActor() override = default;

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }
        const char *name() const override { return NAME.c_str(); }
        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "MediaActor";
        void init();

        void deserialise(const utility::JsonStore &jsn);

        void add_or_rename_media_source(
            const utility::MediaReference &ref,
            const std::string &name,
            const std::vector<utility::MediaReference> &existing_source_refs,
            const bool set_as_current_source);

        void clone_bookmarks_to(
            caf::typed_response_promise<utility::UuidUuidActor> rp,
            const utility::UuidUuidActor &uua,
            caf::actor src_bookmark,
            caf::actor dst_bookmark);

        void switch_current_source_to_named_source(
            caf::typed_response_promise<bool> rp,
            const std::string &source_name,
            const media::MediaType mt,
            const bool auto_select_source_if_failed = false);

        void auto_set_current_source(
            const media::MediaType media_type, caf::typed_response_promise<bool> rp);

        void update_human_readable_details(caf::typed_response_promise<utility::JsonStore> rp);

        void build_media_list_info(caf::typed_response_promise<utility::JsonStore> rp);

        void display_info_item(
            const utility::JsonStore item_query_info,
            caf::typed_response_promise<utility::JsonStore> rp);

        void duplicate(
            caf::typed_response_promise<utility::UuidUuidActor> rp,
            caf::actor src_bookmarks,
            caf::actor dst_bookmarks);

        Media base_;
        caf::actor json_store_;
        std::map<utility::Uuid, caf::actor> media_sources_;
        utility::UuidList bookmark_uuids_;
        bool pending_change_{false};
        utility::JsonTree media_list_columns_config_;
        utility::JsonStore human_readable_info_;
        utility::JsonStore media_list_columns_info_;
    };

    class MediaSourceActor : public caf::event_based_actor {
      public:
        MediaSourceActor(
            caf::actor_config &cfg,
            const std::string &name,
            const caf::uri &_uri,
            const utility::FrameList &frame_list,
            const utility::FrameRate &rate = utility::FrameRate(timebase::k_flicks_24fps),
            const utility::Uuid &uuid      = utility::Uuid());
        MediaSourceActor(
            caf::actor_config &cfg,
            const std::string &name,
            const std::string &reader,
            const utility::MediaReference &media_reference,
            const utility::Uuid &uuid = utility::Uuid());
        MediaSourceActor(
            caf::actor_config &cfg,
            const std::string &name        = "Unnamed",
            const caf::uri &_uri           = caf::uri(),
            const utility::FrameRate &rate = utility::FrameRate(timebase::k_flicks_24fps),
            const utility::Uuid &uuid      = utility::Uuid());
        MediaSourceActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        ~MediaSourceActor() override = default;

        static caf::message_handler default_event_handler();

        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

        const char *name() const override { return NAME.c_str(); }

      private:
        void update_media_status();

        void update_media_detail();

        void
        acquire_detail(const utility::FrameRate &rate, caf::typed_response_promise<bool> rp);
        void deliver_frames_media_keys(
            caf::typed_response_promise<media::MediaKeyVector> rp,
            const MediaType media_type,
            const std::vector<int> logical_frames);

        void get_media_pointers_for_frames(
            const MediaType media_type,
            const LogicalFrameRanges &ranges,
            caf::typed_response_promise<media::AVFrameIDs> rp,
            const utility::Uuid clip_uuid);

        void update_stream_media_reference(
            StreamDetail &stream_detail,
            const utility::Uuid &stream_uuid,
            const utility::FrameRate &rate,
            const utility::Timecode &timecode);

        void send_stream_metadata_to_stream_actors(const utility::JsonStore &meta);

        void duplicate(caf::typed_response_promise<utility::UuidUuidActor> rp);

        inline static const std::string NAME = "MediaSourceActor";
        void init();
        MediaSource base_;
        caf::actor json_store_;
        std::map<utility::Uuid, caf::actor> media_streams_;
        caf::actor_addr parent_;
        utility::Uuid parent_uuid_;
        std::vector<caf::typed_response_promise<bool>> pending_stream_detail_requests_;
    };

    class MediaStreamActor : public caf::event_based_actor {
      public:
        MediaStreamActor(
            caf::actor_config &cfg,
            const StreamDetail &detail,
            const utility::Uuid &uuid = utility::Uuid());
        MediaStreamActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        ~MediaStreamActor() override = default;
        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "MediaStreamActor";
        void init();
        MediaStream base_;
        caf::actor json_store_;
    };
} // namespace media
} // namespace xstudio
