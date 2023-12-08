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
        static inline int NullCodeValue    = {-888};
        static inline float NullPixelValue = {-888.0f};

        PixelInfo() = default;

        PixelInfo(
            const Imath::V2i &location_in_image, const std::string layer_name = std::string())
            : location_(location_in_image), layer_name_(std::move(layer_name)) {}

        PixelInfo(const PixelInfo &o) = default;

        ~PixelInfo() = default;

        void add_code_value_info(const std::string &channel_name, const int code_value) {
            code_values_data_.push_back(
                PixelChannelInfo{channel_name, NullPixelValue, code_value});
        }

        void add_raw_channel_info(const std::string &channel_name, const float pixel_value) {
            raw_data_.push_back(PixelChannelInfo{channel_name, pixel_value, NullCodeValue});
        }

        void add_linear_channel_info(const std::string &channel_name, const float pixel_value) {
            linear_data_.push_back(PixelChannelInfo{channel_name, pixel_value, NullCodeValue});
        }

        void add_display_rgb_info(const std::string &channel_name, const float pixel_value) {
            screen_rgb_data_.push_back(
                PixelChannelInfo{channel_name, pixel_value, NullCodeValue});
        }

        void set_raw_colourspace_name(const std::string &name) { raw_colourspace_name_ = name; }

        void set_display_colourspace_name(const std::string &name) {
            display_colourspace_name_ = name;
        }

        void set_linear_colourspace_name(const std::string &name) {
            linear_colourspace_name_ = name;
        }

        struct PixelChannelInfo {
            std::string channel_name;     // NOLINT
            float pixel_value = {-10.0f}; // NOLINT
            int code_value    = {-888};   // NOLINT
        };

        typedef std::vector<PixelChannelInfo> PixelChannelsData;

        [[nodiscard]] const std::string &raw_colourspace_name() const {
            return raw_colourspace_name_;
        }
        [[nodiscard]] const std::string &display_colourspace_name() const {
            return display_colourspace_name_;
        }
        [[nodiscard]] const std::string &linear_colourspace_name() const {
            return linear_colourspace_name_;
        }

        [[nodiscard]] const PixelChannelsData &raw_channels_info() const { return raw_data_; }
        [[nodiscard]] const PixelChannelsData &code_values_info() const {
            return code_values_data_;
        }
        [[nodiscard]] const PixelChannelsData &linear_channels_info() const {
            return linear_data_;
        }
        [[nodiscard]] const PixelChannelsData &rgb_display_info() const {
            return screen_rgb_data_;
        }
        [[nodiscard]] const Imath::V2i &location_in_image() const { return location_; }
        [[nodiscard]] const std::string &layer_name() const { return layer_name_; }

      private:
        PixelChannelsData screen_rgb_data_;
        PixelChannelsData linear_data_;
        PixelChannelsData raw_data_;
        PixelChannelsData code_values_data_;
        std::string raw_colourspace_name_     = "Raw";
        std::string display_colourspace_name_ = "Display";
        std::string linear_colourspace_name_  = "Linear";
        std::string layer_name_;
        Imath::V2i location_;
    };
} // namespace media_reader
} // namespace xstudio