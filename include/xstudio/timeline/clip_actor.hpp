// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/timeline/clip.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/track.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {
    class ClipActor : public caf::event_based_actor {
      public:
        ClipActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        ClipActor(caf::actor_config &cfg, const utility::JsonStore &jsn, Item &item);
        ClipActor(
            caf::actor_config &cfg,
            const utility::UuidActor &media,
            const std::string &name   = "ClipActor",
            const utility::Uuid &uuid = utility::Uuid::generate());
        ~ClipActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ClipActor";
        void init();

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        Clip base_;
        caf::actor event_group_;
        caf::actor_addr media_;

        std::map<int, std::shared_ptr<const media::AVFrameID>> audio_ptr_cache_;
        std::map<int, std::shared_ptr<const media::AVFrameID>> image_ptr_cache_;
    };
} // namespace timeline
} // namespace xstudio
