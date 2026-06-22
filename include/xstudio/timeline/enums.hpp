// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace timeline {
    typedef enum {
        IT_NONE        = 0x0L,
        IT_GAP         = 0x1L,
        IT_CLIP        = 0x2L,
        IT_AUDIO_TRACK = 0x3L,
        IT_VIDEO_TRACK = 0x4L,
        IT_STACK       = 0x5L,
        IT_TIMELINE    = 0x6L
    } ItemType;
}
} // namespace xstudio