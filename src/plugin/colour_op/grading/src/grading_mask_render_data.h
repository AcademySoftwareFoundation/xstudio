#pragma once

#include "xstudio/utility/blind_data.hpp"

#include "grading_data.h"

namespace xstudio {
namespace ui {
namespace viewport {

class GradingMaskRenderData : public utility::BlindDataObject {
  public:

    // As far as I understand it, we only need a single GradingData to 
    // worry about here. This contains the full data set for the grade
    // that the user is interacting with.
    GradingData interaction_grading_data_;

    // Leaving this old code here in case it needs re-instating.
    
    /*void add_grading_data(const GradingData &data) {
        data_vec_.push_back(data);
    }

    size_t layer_count() const {
        size_t ret = 0;
        for (auto& layer : data_vec_) {
            ret += layer.size();
        }
        return ret;
    }

    size_t size() const { return data_vec_.size(); }

    std::vector<GradingData>::const_iterator begin() const {
        return data_vec_.cbegin();
    }
    std::vector<GradingData>::const_iterator end() const {
        return data_vec_.cend();
    }*/

  private:
      //std::vector<GradingData> data_vec_;
};

} // end namespace viewport
} // end namespace ui
} // namespace xstudio
