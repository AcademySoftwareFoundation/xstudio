// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace plugin_manager {
    typedef enum {
        PF_CUSTOM            = 1 << 0,
        PF_MEDIA_READER      = 1 << 1,
        PF_MEDIA_HOOK        = 1 << 2,
        PF_MEDIA_METADATA    = 1 << 3,
        PF_COLOUR_MANAGEMENT = 1 << 4,
        PF_COLOUR_OPERATION  = 1 << 5,
        PF_DATA_SOURCE       = 1 << 6,
        PF_VIEWPORT_OVERLAY  = 1 << 7,
        PF_VIEWPORT_RENDERER = 1 << 8,
        PF_HEAD_UP_DISPLAY   = 1 << 9,
        PF_UTILITY           = 1 << 10,
        PF_CONFORM           = 1 << 11,
        PF_VIDEO_OUTPUT      = 1 << 12,
    } PluginFlags;

    typedef unsigned int PluginType;

} // namespace plugin_manager

namespace plugin {
    typedef enum {
        BottomLeft,
        BottomCenter,
        BottomRight,
        TopLeft,
        TopCenter,
        TopRight,
        FullScreen
    } HUDElementPosition;
} // namespace plugin
} // namespace xstudio