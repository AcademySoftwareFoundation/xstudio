// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <memory>
#include <set>
#include <string>
#include <deque>

#include "xstudio/thumbnail/thumbnail.hpp"


namespace xstudio::thumbnail {
class ThumbnailManagerActor : public caf::event_based_actor {
  public:
    ThumbnailManagerActor(caf::actor_config &cfg);

    ~ThumbnailManagerActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    void on_exit() override;
    const char *name() const override { return NAME.c_str(); }

  private:
    void request_buffer(
        caf::typed_response_promise<ThumbnailBufferPtr> rp,
        const media::AVFrameID &mptr,
        const size_t thumb_size,
        const size_t hash,
        const bool cache_to_disk,
        const utility::Uuid &job_uuid);

    void request_buffer(
        caf::typed_response_promise<ThumbnailBufferPtr> rp,
        const std::string &key,
        const size_t thumb_size);

    void process_queue();

    inline static const std::string NAME = "ThumbnailManagerActor";
    caf::behavior behavior_;

    caf::actor mem_cache_;
    caf::actor dsk_cache_;

    std::deque<std::tuple<
        caf::typed_response_promise<ThumbnailBufferPtr>,
        media::AVFrameID,
        size_t,
        size_t,
        bool,
        utility::Uuid>>
        request_queue_;
};

} // namespace xstudio::thumbnail
