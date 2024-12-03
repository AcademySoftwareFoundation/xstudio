// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace bookmark {

    class BookmarksActor : public caf::event_based_actor {
      public:
        BookmarksActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        BookmarksActor(
            caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());

        ~BookmarksActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        static caf::message_handler default_event_handler();

        void on_exit() override;

      private:
        inline static const std::string NAME = "BookmarksActor";
        void init();
        caf::message_handler message_handler();

        caf::behavior make_behavior() override {
            return message_handler().or_else(base_.container_message_handler(this));
        }

        void csv_export(
            caf::typed_response_promise<std::pair<std::string, std::vector<std::byte>>> rp);

      private:
        Bookmarks base_;
        std::string default_category_;

        std::map<utility::Uuid, caf::actor> bookmarks_;
    };

} // namespace bookmark
} // namespace xstudio
