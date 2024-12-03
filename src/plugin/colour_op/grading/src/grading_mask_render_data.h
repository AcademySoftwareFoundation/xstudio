#pragma once

#include "xstudio/utility/blind_data.hpp"

#include "grading_data.h"

namespace xstudio {
namespace ui {
namespace viewport {

class GradingMaskRenderData : public utility::BlindDataObject {
  public:
    // This is the grade the user is currently interacting with.
    GradingData interaction_grading_data_;
};

} // end namespace viewport
} // end namespace ui
} // namespace xstudio
