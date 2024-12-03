// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <memory>
#include <set>
#include <string>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/time_cache.hpp"
#include "xstudio/utility/chrono.hpp"

namespace xstudio {
namespace media_cache {
    class GlobalImageCacheActor : public caf::event_based_actor {
      public:
        GlobalImageCacheActor(caf::actor_config &cfg);

        ~GlobalImageCacheActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "GlobalImageCacheActor";
        void
        update_changes(const media::MediaKeyVector &store, const media::MediaKeyVector &erase);

        caf::behavior behavior_;
        utility::TimeCache<media::MediaKey, media_reader::ImageBufPtr> cache_;
        std::unordered_set<media::MediaKey> new_keys_;
        std::unordered_set<media::MediaKey> erased_keys_;
        bool update_pending_;
        std::chrono::minutes reset_idle_{0};
        utility::time_point last_activity_{utility::clock::now()};
    };

    class GlobalAudioCacheActor : public caf::event_based_actor {
      public:
        GlobalAudioCacheActor(caf::actor_config &cfg);

        ~GlobalAudioCacheActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        void on_exit() override;
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "GlobalAudioCacheActor";
        void
        update_changes(const media::MediaKeyVector &store, const media::MediaKeyVector &erase);

        caf::behavior behavior_;
        utility::TimeCache<media::MediaKey, media_reader::AudioBufPtr> cache_;
        std::unordered_set<media::MediaKey> new_keys_;
        std::unordered_set<media::MediaKey> erased_keys_;
        bool update_pending_;
        std::chrono::minutes reset_idle_{0};
        utility::time_point last_activity_{utility::clock::now()};
    };
} // namespace media_cache
} // namespace xstudio
