// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <caf/all.hpp>

#include "xstudio/event/event.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;

class BuildPlaylistMediaJob {

  public:
    BuildPlaylistMediaJob(
        caf::actor playlist_actor,
        const utility::Uuid &media_uuid,
        const std::string media_name,
        utility::JsonStore sg_data,
        utility::FrameRate media_rate,
        std::string preferred_visual_source,
        std::string preferred_audio_source,
        std::shared_ptr<event::Event> event,
        std::shared_ptr<utility::UuidList> ordererd_uuids,
        utility::Uuid before,
        std::string flag_colour,
        std::string flag_text,
        caf::typed_response_promise<utility::UuidActorVector> response_promise,
        std::shared_ptr<utility::UuidActorVector> result,
        std::shared_ptr<int> result_count)
        : playlist_actor_(std::move(playlist_actor)),
          media_uuid_(media_uuid),
          media_name_(media_name),
          sg_data_(sg_data),
          media_rate_(media_rate),
          preferred_visual_source_(std::move(preferred_visual_source)),
          preferred_audio_source_(std::move(preferred_audio_source)),
          event_msg_(std::move(event)),
          ordererd_uuids_(std::move(ordererd_uuids)),
          before_(std::move(before)),
          flag_colour_(std::move(flag_colour)),
          flag_text_(std::move(flag_text)),
          response_promise_(std::move(response_promise)),
          result_(std::move(result)),
          result_count_(result_count) {
        // increment a shared counter - the counter is shared between
        // all the indiviaual Media creation jobs in a single build playlist
        // task
        (*result_count)++;
    }

    BuildPlaylistMediaJob(const BuildPlaylistMediaJob &o) = default;
    BuildPlaylistMediaJob()                               = default;

    ~BuildPlaylistMediaJob() {
        // this gets destroyed when the job is done with.
        if (media_actor_) {
            result_->push_back(utility::UuidActor(media_uuid_, media_actor_));
        }
        // decrement the counter
        (*result_count_)--;

        if (!(*result_count_)) {
            // counter has dropped to zero, all jobs within a single build playlist
            // tas are done. Our 'result' member here is in the order that the
            // media items were created (asynchronously), rather than the order
            // of the final playlist ... so we need to reorder our 'result' to
            // match the ordering in the playlist
            utility::UuidActorVector reordered;
            reordered.reserve(result_->size());
            for (const auto &uuid : (*ordererd_uuids_)) {
                for (auto uai = result_->begin(); uai != result_->end(); uai++) {
                    if ((*uai).uuid() == uuid) {
                        reordered.push_back(*uai);
                        result_->erase(uai);
                        break;
                    }
                }
            }
            response_promise_.deliver(reordered);
        }
    }

    caf::actor playlist_actor_;
    utility::Uuid media_uuid_;
    std::string media_name_;
    utility::JsonStore sg_data_;
    utility::FrameRate media_rate_;
    std::string preferred_visual_source_;
    std::string preferred_audio_source_;
    std::shared_ptr<event::Event> event_msg_;
    std::shared_ptr<utility::UuidList> ordererd_uuids_;
    utility::Uuid before_;
    std::string flag_colour_;
    std::string flag_text_;
    caf::typed_response_promise<utility::UuidActorVector> response_promise_;
    std::shared_ptr<utility::UuidActorVector> result_;
    std::shared_ptr<int> result_count_;
    caf::actor media_actor_;
};

class ShotgunMediaWorker : public caf::event_based_actor {
  public:
    ShotgunMediaWorker(caf::actor_config &cfg, const caf::actor_addr source);
    ~ShotgunMediaWorker() override = default;

    const char *name() const override { return NAME.c_str(); }

  private:
    inline static const std::string NAME = "ShotgunMediaWorker";
    caf::behavior make_behavior() override { return behavior_; }

    void add_media_step_1(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const utility::JsonStore &jsn,
        const utility::FrameRate &media_rate);
    void add_media_step_2(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const utility::JsonStore &jsn,
        const utility::FrameRate &media_rate,
        const utility::UuidActor &movie_source);
    void add_media_step_3(
        caf::typed_response_promise<bool> rp,
        caf::actor media,
        const utility::JsonStore &jsn,
        const utility::UuidActorVector &srcs);

  private:
    caf::behavior behavior_;
    caf::actor_addr data_source_;
};