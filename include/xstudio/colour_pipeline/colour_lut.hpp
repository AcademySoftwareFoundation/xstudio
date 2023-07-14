// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <vector>
#include <string_view>

#include "xstudio/utility/uuid.hpp"

namespace xstudio {

namespace colour_pipeline {

    struct LUTDescriptor {

        enum DataType { UINT8, UINT16, FLOAT16, FLOAT32 } data_type_;

        enum Dimension { ONE_D = 1, TWO_D = 2, THREE_D = 4, RECT_TWO_D = 6 } dimension_;

        enum Channels { RED = 1, RGB = 2 } channels_;

        enum Interpolation { NEAREST, LINEAR } interpolation_;

        int xsize_;
        int ysize_;
        int zsize_;

        static LUTDescriptor Create1DLUT( // NOLINT
            int size,
            DataType dt      = FLOAT32,
            Channels ch      = RGB,
            Interpolation it = LINEAR) {
            return LUTDescriptor{dt, ONE_D, ch, it, size, 1, 1};
        }

        static LUTDescriptor Create2DLUT( // NOLINT
            int width,
            int height,
            DataType dt      = FLOAT32,
            Channels ch      = RGB,
            Interpolation it = LINEAR) {
            return LUTDescriptor{dt, TWO_D, ch, it, width, height, 1};
        }

        static LUTDescriptor Create2DRectLUT( // NOLINT
            int width,
            int height,
            DataType dt      = FLOAT32,
            Channels ch      = RGB,
            Interpolation it = LINEAR) {
            return LUTDescriptor{dt, RECT_TWO_D, ch, it, width, height, 1};
        }

        static LUTDescriptor Create3DLUT( // NOLINT
            int size,
            DataType dt      = FLOAT32,
            Channels ch      = RGB,
            Interpolation it = LINEAR) {
            return LUTDescriptor{dt, THREE_D, ch, it, size, size, size};
        }

        static LUTDescriptor Create3DLUT( // NOLINT
            int width,
            int height,
            int depth,
            DataType dt      = FLOAT32,
            Channels ch      = RGB,
            Interpolation it = LINEAR) {
            return LUTDescriptor{dt, THREE_D, ch, it, width, height, depth};
        }

        [[nodiscard]] std::string as_string() const;
    };

    class ColourLUT {

      public:
        ColourLUT(const LUTDescriptor &desc, const std::string name)
            : descriptor_(desc), texture_name_(std::move(name)) {
            const int chans = descriptor_.channels_ == LUTDescriptor::RGB ? 3 : 1;

            int sample_bytes;
            switch (desc.data_type_) {
            case LUTDescriptor::UINT16:
            case LUTDescriptor::FLOAT16:
                sample_bytes = chans * 2;
                break;
            case LUTDescriptor::FLOAT32:
                sample_bytes = chans * 4;
                break;
            default:
                sample_bytes = chans;
                break;
            };

            if (desc.dimension_ == LUTDescriptor::ONE_D)
                data_size_ = desc.xsize_ * sample_bytes;
            else if (desc.dimension_ & LUTDescriptor::TWO_D)
                data_size_ = desc.xsize_ * desc.ysize_ * sample_bytes;
            else if (desc.dimension_ == LUTDescriptor::THREE_D)
                data_size_ = desc.xsize_ * desc.ysize_ * desc.zsize_ * sample_bytes;

            using elem_type    = decltype(buffer_)::value_type;
            size_t vector_size = data_size_ / sizeof(elem_type);
            vector_size        = ((vector_size / sizeof(elem_type)) + 1) * sizeof(elem_type);
            buffer_.resize(vector_size);
        }
        ~ColourLUT() = default;

        [[nodiscard]] size_t data_size() const { return data_size_; }
        [[nodiscard]] const std::string &texture_name() const { return texture_name_; }
        [[nodiscard]] std::string texture_name_and_desc() const {
            return texture_name() + descriptor_.as_string();
        }
        [[nodiscard]] const LUTDescriptor &descriptor() const { return descriptor_; }
        [[nodiscard]] std::size_t cache_id() const { return cache_id_; }

        [[nodiscard]] const uint8_t *data() const { return (const uint8_t *)buffer_.data(); }
        [[nodiscard]] uint8_t *writeable_data() {
            // Unique hash generated on write
            cache_id_ = std::hash<std::string>{}(to_string(xstudio::utility::Uuid::generate()));
            return (uint8_t *)buffer_.data();
        }

        void update_content_hash() {
            // Content dependent hash for texture caching
            cache_id_ = std::hash<std::string_view>{}(
                std::string_view((const char *)buffer_.data(), data_size_));
        }

      private:
        const LUTDescriptor descriptor_;
        // using long long type for data store vector to achieve 8 byte mem alignment
        std::vector<long long> buffer_;
        std::size_t data_size_ = {0};
        std::size_t cache_id_;
        std::string texture_name_;
    };

    typedef std::shared_ptr<ColourLUT> ColourLUTPtr;
} // namespace colour_pipeline
} // namespace xstudio
