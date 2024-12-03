// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <memory>
#include <set>
#include <string>
#include <functional>
#include <fmt/format.h>
#include <filesystem>


#include "xstudio/media/media.hpp"
#include "xstudio/thumbnail/enums.hpp"
#include "xstudio/utility/logging.hpp"

namespace xstudio {
namespace thumbnail {
    namespace fs = std::filesystem;


    class ThumbnailKey : private std::string {

      public:
        ThumbnailKey()                      = default;
        ThumbnailKey(const ThumbnailKey &o) = default;
        ThumbnailKey(const std::string &o) : std::string(o) {}
        ThumbnailKey(const std::string &o, const size_t size)
            : std::string(fmt::format("{}/{}", o, size)) {}
        ThumbnailKey(
            const media::AVFrameID &mptr, const size_t hash = 0, const size_t size = 256)
            : std::string(fmt::format("{}/{}/{}", mptr.key(), std::to_string(hash), size)) {}

        using std::string::empty;
        using std::string::substr;

        bool operator==(const ThumbnailKey &o) const {
            return static_cast<const std::string &>(o) ==
                   static_cast<const std::string &>(*this);
        }

        bool operator!=(const ThumbnailKey &o) const {
            return static_cast<const std::string &>(o) !=
                   static_cast<const std::string &>(*this);
        }

        bool operator<(const ThumbnailKey &o) const {
            return static_cast<const std::string &>(o) <
                   static_cast<const std::string &>(*this);
        }

        friend std::string to_string(const ThumbnailKey &value);
        friend fmt::string_view to_string_view(const ThumbnailKey &value);

        template <class Inspector> friend bool inspect(Inspector &f, ThumbnailKey &x) {
            return f.object(x).fields(f.field("data", static_cast<std::string &>(x)));
        }
        [[nodiscard]] std::string hash_str() const {
            return fmt::format(
                fmt::runtime(fmt_format()),
                std::hash<std::string>{}(static_cast<const std::string &>(*this)));
        }
        [[nodiscard]] size_t hash() const {
            return std::hash<std::string>{}(static_cast<const std::string &>(*this));
        }

      private:
        [[nodiscard]] const std::string fmt_format() const {
            return "{:0" + std::to_string(sizeof(size_t) * 2) + "x}";
        }
    };

    inline std::string to_hash_string(const size_t hash) {
        return fmt::format(
            fmt::runtime("{:0" + std::to_string(sizeof(size_t) * 2) + "x}"), hash);
    }
    inline std::string to_string(const ThumbnailKey &v) {
        return static_cast<const std::string>(v);
    }

    inline fmt::string_view to_string_view(const ThumbnailKey &s) {
        return {s.data(), s.length()};
    }

    typedef std::vector<ThumbnailKey> ThumbnailKeyVector;

    class ThumbnailBuffer {
      public:
        ThumbnailBuffer(
            const size_t width            = 0,
            const size_t height           = 0,
            const THUMBNAIL_FORMAT format = TF_RGB24)
            : width_(width), height_(height), format_(format) {
            switch (format_) {
            default:
            case TF_RGB24:
                break;
            case TF_RGBF96:
                channel_size_ = sizeof(float);
                break;
            }

            buffer_.resize(size());
        }
        virtual ~ThumbnailBuffer() = default;

        [[nodiscard]] size_t size() const {
            return width_ * height_ * channels_ * channel_size_;
        }
        [[nodiscard]] bool empty() const { return buffer_.empty(); }

        [[nodiscard]] std::vector<std::byte> &data() { return buffer_; }

        void set_dimensions(const size_t width, const size_t height) {
            width_  = width;
            height_ = height;
            buffer_.resize(size());
        }

        void bilin_resize(const size_t new_width, const size_t new_height);

        void convert_to(const THUMBNAIL_FORMAT format);

        void flip();

        [[nodiscard]] utility::JsonStore &metadata() { return metadata_; }

        [[nodiscard]] size_t width() const { return width_; }
        [[nodiscard]] size_t height() const { return height_; }
        [[nodiscard]] THUMBNAIL_FORMAT format() const { return format_; }

        [[nodiscard]] size_t channels() const { return channels_; }
        [[nodiscard]] size_t pixel_stride() const { return channel_size_ * channels_; }
        [[nodiscard]] size_t row_stride() const { return pixel_stride() * width_; }

        template <class Inspector> friend bool inspect(Inspector &f, ThumbnailBuffer &x) {
            return f.object(x).fields(
                f.field("w", x.width_),
                f.field("h", x.height_),
                f.field("f", x.format_),
                f.field("c", x.channels_),
                f.field("s", x.channel_size_),
                f.field("d", x.buffer_));
        }

      private:
        size_t width_{0};
        size_t height_{0};
        size_t channels_{3};
        size_t channel_size_{1};
        THUMBNAIL_FORMAT format_{TF_RGB24};
        utility::JsonStore metadata_;

        std::vector<std::byte> buffer_;
    };

    typedef std::shared_ptr<ThumbnailBuffer> ThumbnailBufferPtr;

    struct DiskCacheStat {
        DiskCacheStat() = default;
        DiskCacheStat(const size_t size, const size_t count) : size_(size), count_(count) {}

        template <class Inspector> friend bool inspect(Inspector &f, DiskCacheStat &x) {
            return f.object(x).fields(
                f.field("size", x.size_),
                f.field("count", x.count_),
                f.field("cache", x.cache_));
        }

        void populate(const std::string &path);
        void add_thumbnail(
            const ThumbnailKey &key, const size_t size, const fs::file_time_type &mtime) {
            add_thumbnail(key.hash(), size, mtime);
        }

        void
        add_thumbnail(const size_t key, const size_t size, const fs::file_time_type &mtime);
        std::vector<size_t> evict(const size_t max_size, const size_t max_count);

        size_t size_{0};
        size_t count_{0};
        std::map<size_t, std::pair<size_t, fs::file_time_type>> cache_;
    };

} // namespace thumbnail
} // namespace xstudio

namespace caf {
template <>
struct inspector_access<std::shared_ptr<xstudio::thumbnail::ThumbnailBuffer>>
    : optional_inspector_access<std::shared_ptr<xstudio::thumbnail::ThumbnailBuffer>> {
    // nop
};
} // namespace caf
