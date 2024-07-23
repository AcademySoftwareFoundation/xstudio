// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/playhead/playhead.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace playhead {

    class PlayheadGlobalEventsActor : public caf::event_based_actor {
      public:
        PlayheadGlobalEventsActor(caf::actor_config &cfg);
        ~PlayheadGlobalEventsActor() override = default;

        const char *name() const override { return NAME.c_str(); }

        inline static caf::message_handler default_event_handler() {
            return {
                [=](utility::event_atom, ui::viewport::viewport_playhead_atom, caf::actor) {},
                [=](utility::event_atom, show_atom, const media_reader::ImageBufPtr &) {}};
        }

      private:
        inline static const std::string NAME = "PlayheadGlobalEventsActor";

        void init();

        caf::behavior make_behavior() override { return behavior_; }

      protected:
        caf::behavior behavior_;
        caf::actor event_group_;
        caf::actor on_screen_playhead_;
        std::map<std::string, caf::actor_addr> viewports_;
    };
} // namespace playhead
} // namespace xstudio
