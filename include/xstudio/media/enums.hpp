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
        MS_UNREADABLE  = 0x04L
    } MediaStatus;

} // namespace media
} // namespace xstudio