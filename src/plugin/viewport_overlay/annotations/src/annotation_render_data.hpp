#pragma once

#include "xstudio/utility/blind_data.hpp"

namespace xstudio {
namespace ui {
    namespace viewport {

        class AnnotationRenderDataSet : public utility::BlindDataObject {
          public:
            AnnotationRenderDataSet(
                const bool show_annotations,
                const xstudio::ui::canvas::HandleState &h,
                const utility::Uuid &current_edited_bookmark_uuid,
                const media::MediaKey &frame_being_annotated,
                bool laser_mode)
                : show_annotations_(show_annotations),
                  handle_(h),
                  current_edited_bookmark_uuid_(current_edited_bookmark_uuid),
                  interaction_frame_key_(frame_being_annotated),
                  laser_mode_(laser_mode) {}

            const bool show_annotations_;
            const utility::Uuid current_edited_bookmark_uuid_;
            const xstudio::ui::canvas::HandleState handle_;
            const media::MediaKey interaction_frame_key_;
            const bool laser_mode_;
        };

    } // namespace viewport
} // end namespace ui
} // end namespace xstudio