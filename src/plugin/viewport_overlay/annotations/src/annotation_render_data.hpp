// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/blind_data.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationRenderDataSet : public utility::BlindDataObject {
          public:
            AnnotationRenderDataSet(
                const xstudio::ui::canvas::Canvas &interaction_canvas,
                const utility::Uuid &current_edited_bookmark_uuid,
                const xstudio::ui::canvas::HandleState handle,
                const std::string &interaction_frame_key)
                : interaction_canvas_(interaction_canvas),
                  current_edited_bookmark_uuid_(current_edited_bookmark_uuid),
                  handle_(handle),
                  interaction_frame_key_(interaction_frame_key) {}

            // Canvas is thread safe
            const xstudio::ui::canvas::Canvas &interaction_canvas_;

            const utility::Uuid current_edited_bookmark_uuid_;
            const xstudio::ui::canvas::HandleState handle_;
            const std::string interaction_frame_key_;
        };

    } // namespace viewport
} // end namespace ui
} // end namespace xstudio