// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <memory>
#include <set>
#include <string>

#include "xstudio/utility/mru_cache.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

namespace xstudio {
namespace thumbnail {

    class ThumbnailCacheActor : public caf::event_based_actor {
      public:
        ThumbnailCacheActor(
            caf::actor_config &cfg,
            const size_t max_size  = std::numeric_limits<size_t>::max(),
            const size_t max_count = std::numeric_limits<size_t>::max());

        ~ThumbnailCacheActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ThumbnailCacheActor";
        void update_changes(const std::vector<size_t> &store, const std::vector<size_t> &erase);

        caf::behavior behavior_;
        utility::MRUCache<size_t, ThumbnailBuffer> cache_;
        std::set<size_t> new_keys_;
        std::set<size_t> erased_keys_;
        bool update_pending_;
    };

} // namespace thumbnail
} // namespace xstudio
