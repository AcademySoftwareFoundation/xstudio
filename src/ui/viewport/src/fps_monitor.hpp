// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <deque>

#include <caf/actor_system_config.hpp>
#include <caf/all.hpp>
#include <caf/io/all.hpp>
#include <caf/type_id.hpp>

#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/frame_rate.hpp"

using namespace caf;

namespace xstudio {
namespace ui {
    namespace fps_monitor {

        class FpsMonitor : public caf::event_based_actor {
          public:
            FpsMonitor(caf::actor_config &cfg);
            ~FpsMonitor() override = default;

          private:
            caf::behavior make_behavior() override { return behavior_; }

            utility::FrameRate caluculate_actual_fps_measure() const;
            void reset_actual_fps_measure();
            void framebuffer_swapped(const utility::time_point &tp, const int frame);
            void connect_to_playhead(caf::actor &playhead);

          protected:
            caf::behavior behavior_;
            caf::actor playhead_grp_;
            caf::actor fps_event_grp_;

            typedef std::deque<std::pair<utility::time_point, int>> TimePointFifo;
            TimePointFifo viewport_frame_update_timepoints_;
            utility::FrameRate target_playhead_rate_;
            bool playing_{0};
            bool frame_queued_for_display_{0};
            bool forward_{true};
            float velocity_multiplier_{1.0f};
            float velocity_{1.0f};
        };
    } // namespace fps_monitor
} // namespace ui
} // namespace xstudio
