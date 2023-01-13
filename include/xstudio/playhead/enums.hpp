// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace playhead {
    typedef enum {
        CM_STRING = 0,
        CM_AB,
        CM_VERTICAL,
        CM_HORIZONTAL,
        CM_GRID,
        CM_OFF
    } CompareMode;

    typedef enum { LM_PLAY_ONCE = 0, LM_LOOP, LM_PING_PONG } LoopMode;

    typedef enum {
        OM_FAIL = 0x0L,
        OM_NULL = 0x1L,
        OM_HOLD = 0x2L,
        OM_LOOP = 0x3L
    } OverflowMode;
} // namespace playhead
} // namespace xstudio