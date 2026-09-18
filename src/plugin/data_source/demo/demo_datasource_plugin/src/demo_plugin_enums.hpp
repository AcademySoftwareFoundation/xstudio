// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <Qt>

namespace xstudio::demo_plugin {

enum DATA_MODEL_ROLE {
    JOB = Qt::UserRole,
    SEQUENCE,
    IS_ASSET,
    SHOT,
    VERSION_NAME,
    VERSION_TYPE,
    VERSION_STREAM,
    EXPANDED,
    VERSION,
    UUID,
    ARTIST,
    STATUS,
    MEDIA_PATH,
    FRAME_RANGE,
    COMP_RANGE
};

} // namespace xstudio::demo_plugin
