// SPDX-License-Identifier: Apache-2.0

#include "xstudio/media_reader/image_buffer.hpp"
#include "xstudio/utility/helpers.hpp"

#include "grading.h"
#include "grading_mask_render_data.h"
#include "grading_mask_gl_renderer.h"
#include "grading_colour_op.hpp"
#include "grading_common.h"

using namespace xstudio;
using namespace xstudio::ui::viewport;


std::vector<GradingInfo>
xstudio::ui::viewport::get_active_grades(const xstudio::media_reader::ImageBufPtr &image) {

    std::vector<GradingInfo> active_grades;

    // Make sure the bookmarks are applied in a consistent order.
    // The order matters when applying multiple CDLs.
    auto bookmarks = image.bookmarks();
    std::sort(bookmarks.begin(), bookmarks.end(), [](auto &a, auto &b) {
        if (a->detail_.created_ && b->detail_.created_) {
            return a->detail_.created_.value() < b->detail_.created_.value();
        } else {
            // Make a guess
            return true;
        }
    });

    for (auto &bookmark : bookmarks) {
        auto bookmark_data = dynamic_cast<GradingData *>(bookmark->annotation_.get());
        if (bookmark_data) {
            auto json_data = bookmark->detail_.user_data_.value_or(utility::JsonStore());
            bool isActive  = json_data.get_or("grade_active", true);
            active_grades.push_back({bookmark_data, isActive});
        }
    }

    // Currently edited grade take precedence over image's bookmark, this improves
    // interactivity when drawing a mask. Simply update the vector in-place or append
    // if the grade was not saved in a bookmark yet.
    utility::BlindDataObjectPtr blind_data =
        image.plugin_blind_data(utility::Uuid(colour_pipeline::GradingTool::PLUGIN_UUID));
    if (blind_data) {
        const GradingMaskRenderData *render_data =
            dynamic_cast<const GradingMaskRenderData *>(blind_data.get());

        if (render_data) {
            auto blind_uuid = render_data->interaction_grading_data_.bookmark_uuid_;

            if (!active_grades.empty()) {
                for (auto &[grade_data, _] : active_grades) {
                    if (grade_data->bookmark_uuid_ == blind_uuid) {
                        grade_data = &(render_data->interaction_grading_data_);
                        break;
                    }
                }
            } else {
                active_grades.push_back({&(render_data->interaction_grading_data_), true});
            }
        }
    }

    return active_grades;
}