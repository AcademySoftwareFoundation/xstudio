// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "grading_data.h"

namespace xstudio {
namespace ui {
namespace viewport {

    std::vector<GradingInfo> get_active_grades(
        const xstudio::media_reader::ImageBufPtr &image);

} // end namespace viewport
} // end namespace ui
} // end namespace xstudio