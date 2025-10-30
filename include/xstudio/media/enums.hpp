// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace media {

    typedef enum { MT_IMAGE = 0x1L, MT_AUDIO = 0x2L } MediaType;
    typedef enum {
        MS_ONLINE      = 0x0L,
        MS_MISSING     = 0x01L,
        MS_CORRUPT     = 0x02L,
        MS_UNSUPPORTED = 0x03L,
        MS_UNREADABLE  = 0x04L,
        MS_UNKNOWN     = 0x05L
    } MediaStatus;

    typedef enum {
        PS_DONT_HOLD_FRAME = 0,
        PS_HOLD_FRAME,
        PS_COLLAPSE_TO_ON_DISK_FRAMES
    } PartialSeqBehaviour;

    typedef enum { FS_ON_DISK = 0, FS_NOT_ON_DISK, FS_HELD_FRAME, FS_UNKNOWN } FrameStatus;

} // namespace media
} // namespace xstudio