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

    typedef enum {
        AM_STRING = 0,
        AM_ONE,
        AM_ALL,
        AM_TEN
    } AssemblyMode;

    typedef enum { AAM_ALIGN_OFF = 0, AAM_ALIGN_FRAMES, AAM_ALIGN_TRIM } AutoAlignMode;

    typedef enum { LM_PLAY_ONCE = 0, LM_LOOP, LM_PING_PONG } LoopMode;

    typedef enum {
        OM_FAIL = 0x0L,
        OM_NULL = 0x1L,
        OM_HOLD = 0x2L,
        OM_LOOP = 0x3L
    } OverflowMode;

    typedef enum {
        SM_NO_UPDATE        = 0x0L, // NO OP
        SM_CLEAR            = 0x1L,
        SM_SELECT           = 0x2L,
        SM_CLEAR_AND_SELECT = 0x3L,
        SM_DESELECT         = 0x4L,
        SM_TOGGLE           = 0x8L
    } SelectionMode;

} // namespace playhead
} // namespace xstudio
