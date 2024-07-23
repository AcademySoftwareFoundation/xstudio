// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>

namespace xstudio {
namespace ui {
    namespace canvas {

        enum class HandleHoverState {
            NotHovered,
            HoveredInCaptionArea,
            HoveredOnMoveHandle,
            HoveredOnResizeHandle,
            HoveredOnDeleteHandle
        };

        struct HandleState {
            HandleHoverState hover_state{HandleHoverState::NotHovered};
            Imath::Box2f under_mouse_caption_bdb;
            Imath::Box2f current_caption_bdb;
            Imath::V2f handle_size{50.0f, 50.0f};
            std::array<Imath::V2f, 2> cursor_position = {
                Imath::V2f(0.0f, 0.0f), Imath::V2f(0.0f, 0.0f)};
            bool cursor_blink_state{false};

            bool operator==(const HandleState &o) const {
                return (
                    hover_state == o.hover_state &&
                    under_mouse_caption_bdb == o.under_mouse_caption_bdb &&
                    current_caption_bdb == o.current_caption_bdb &&
                    handle_size == o.handle_size && cursor_position == o.cursor_position &&
                    cursor_blink_state == o.cursor_blink_state);
            }

            bool operator!=(const HandleState &o) const { return !(*this == o); }
        };

    } // end namespace canvas
} // end namespace ui
} // end namespace xstudio
