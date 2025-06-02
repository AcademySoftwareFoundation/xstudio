// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace session {

    typedef enum {
        EF_UNDEFINED = 0,
        EF_CSV,
        EF_CSV_WITH_ANNOTATIONS,
        EF_CSV_WITH_IMAGES,
        EF_LAST
    } ExportFormat;
}
} // namespace xstudio