// SPDX-License-Identifier: Apache-2.0
#include <sstream>

#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::colour_pipeline;

std::string LUTDescriptor::as_string() const {

    std::stringstream r;
    r << data_type_ << "_" << dimension_ << "_" << channels_ << "_" << interpolation_ << "_"
      << xsize_ << "_" << ysize_ << "_" << zsize_;
    return r.str();
}

size_t ColourPipelineData::size() const {
    size_t rt = 0;
    for (const auto &lut : luts_) {
        rt += lut->data_size();
    }
    return rt;
}
