// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace plugin_manager {
    typedef enum {
        PT_CUSTOM = 1,
        PT_MEDIA_READER,
        PT_MEDIA_HOOK,
        PT_MEDIA_METADATA,
        PT_COLOUR_MANAGEMENT,
        PT_DATA_SOURCE,
        PT_VIEWPORT_OVERLAY,
        PT_UTILITY,
    } PluginType;

}
} // namespace xstudio