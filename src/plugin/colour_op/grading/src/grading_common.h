// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "grading_data.h"

namespace xstudio {
namespace ui {
namespace viewport {

    bookmark::BookmarkAndAnnotations get_active_grade_bookmarks(const xstudio::media_reader::ImageBufPtr &image);

    std::vector<GradingInfo> get_active_grades(const xstudio::media_reader::ImageBufPtr &image);

} // end namespace viewport
} // end namespace ui
} // end namespace xstudio