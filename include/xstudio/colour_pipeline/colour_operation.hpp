// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "colour_lut.hpp"
#include <memory>

namespace xstudio {

namespace colour_pipeline {

    class ColourOperationData {

      public:
        ColourOperationData()          = default;
        virtual ~ColourOperationData() = default;
    };

    class ColourOperation {

      public:
        ColourOperation() = default

            virtual ~ColourOperation() = default

            virtual std::shared_ptr<ColourOperationData> compute_data(
                const media::AVFrameID &mptr) const = 0;

        virtual std::vector<LUTDescriptor> required_luts() const = 0;

        virtual void upload_data(const std::shared_ptr<ColourOperationData>) const = 0;

        // std::vector < ColourOperationParams > source_parameters;
        // std::vector < ColourOperationParams > global_parameters;

      private:
    };

    typedef std::shared_ptr<ColourOperation> ColourOpPtr;

} // namespace colour_pipeline
} // namespace xstudio
