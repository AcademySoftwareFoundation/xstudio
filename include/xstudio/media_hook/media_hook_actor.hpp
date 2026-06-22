// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media_hook/media_hook.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace media_hook {
    class MediaHookWorkerActor : public caf::event_based_actor {
      public:
        MediaHookWorkerActor(caf::actor_config &cfg);
        ~MediaHookWorkerActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "MediaHookWorkerActor";
        caf::behavior behavior_;
        std::vector<caf::actor> hooks_;

        void do_clip_hook(
            caf::typed_response_promise<bool> rp,
            const caf::actor &clip_actor,
            const utility::JsonStore &clip_meta,
            const utility::JsonStore &media_meta);
    };

    class GlobalMediaHookActor : public caf::event_based_actor {
      public:
        GlobalMediaHookActor(caf::actor_config &cfg);

        ~GlobalMediaHookActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

      private:
        inline static const std::string NAME = "GlobalMediaHookActor";
        caf::behavior behavior_;
        utility::Uuid uuid_;
    };
} // namespace media_hook
} // namespace xstudio
