// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <Imath/ImathVec.h>

namespace xstudio {
namespace media_reader {

    /**
     *  @brief PixelInfo class. A simple class to hold colour information about some pixel
     *  in the image.
     *
     */
    class PixelInfo {

      public:
        PixelInfo(const Imath::V2i &location_in_image) : location_(location_in_image) {}

        PixelInfo(const PixelInfo &o) = default;

        ~PixelInfo() = default;

        void add_pixel_channel_info(const std::string &channel_name, const float pixel_value) {
            data_.push_back(PixelChannelInfo{channel_name, pixel_value});
        }

        struct PixelChannelInfo {
            std::string channel_name;
            float pixel_value;
        };

        typedef std::vector<PixelChannelInfo> PixelChannelsData;

        [[nodiscard]] const PixelChannelsData &data() const { return data_; }
        [[nodiscard]] const Imath::V2i &location_in_image() const { return location_; }

      private:
        PixelChannelsData data_;
        Imath::V2i location_;
    };
} // namespace media_reader
} // namespace xstudio