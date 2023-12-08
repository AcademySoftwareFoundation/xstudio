// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include "xstudio/ui/font.hpp"
#include <Imath/ImathVec.h>

namespace xstudio {
namespace ui {
    namespace viewport {

        class Caption {

          public:
            enum HoverState {
                NotHovered,
                HoveredInCaptionArea,
                HoveredOnMoveHandle,
                HoveredOnResizeHandle,
                HoveredOnDeleteHandle
            };

            Caption(
                const Imath::V2f position,
                const float wrap_width,
                const float font_size,
                const utility::ColourTriplet colour,
                const float opacity,
                const Justification justification,
                const std::string font_name);
            Caption()                 = default;
            Caption(const Caption &o) = default;

            Caption &operator=(const Caption &o) = default;

            void modify_text(const std::string &t, std::string::const_iterator &cursor);

            std::string text_;
            Imath::V2f position_;
            Imath::Box2f bounding_box_;
            float wrap_width_;
            float font_size_;
            std::string font_name_;
            utility::ColourTriplet colour_ = {utility::ColourTriplet(1.0f, 1.0f, 1.0f)};
            float opacity_;
            Justification justification_;

          private:
            friend class AnnotationSerialiser;
        };

    } // end namespace viewport
} // end namespace ui
} // end namespace xstudio
