// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace bookmark {

    class BookmarkActor : public caf::event_based_actor {
      public:
        BookmarkActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        BookmarkActor(
            caf::actor_config &cfg,
            const utility::Uuid &uuid = utility::Uuid::generate(),
            const Bookmark &base      = Bookmark());

        ~BookmarkActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "BookmarkActor";
        void init();
        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

        void build_annotation_via_plugin(const utility::JsonStore &anno_data);

        void set_owner(caf::actor owner, const bool dead = false);


      private:
        Bookmark base_;
        caf::actor_addr owner_;
        caf::actor json_store_;
    };

} // namespace bookmark
} // namespace xstudio
