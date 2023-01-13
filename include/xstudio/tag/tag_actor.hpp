// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/tag/tag.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace tag {

    class TagActor : public caf::event_based_actor {
      public:
        TagActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        TagActor(caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());

        ~TagActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

      private:
        inline static const std::string NAME = "TagActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        TagBase base_;
        caf::actor event_group_;
    };

} // namespace tag
} // namespace xstudio
