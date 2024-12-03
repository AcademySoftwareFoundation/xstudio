// SPDX-License-Identifier: Apache-2.0
#pragma once

#undef NO_ERROR
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/blind_data.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <malloc.h>
#include <new>

#define UNSET_DTS -1e6

namespace xstudio {
namespace media_reader {

    enum class byte : unsigned char {};

    class ImageBufferRecyclerCache;

    class Buffer {

      public:
        Buffer(const utility::JsonStore &params = utility::JsonStore()) : params_(params) {}
        explicit Buffer(const std::string error_message)
            : error_message_(std::move(error_message)), error_state_(HAS_ERROR) {}
        virtual ~Buffer();

        virtual byte *allocate(const size_t size);

        /*
        Preserve existing data and extend buffer
        */
        void resize(const size_t size);

        /*virtual void assign(byte *buffer, const size_t size) {
            buffer_ = std::unique_ptr<byte>(buffer);
            size_   = size;
        }*/

        void set_params(const utility::JsonStore &params) { params_ = params; }

        [[nodiscard]] const utility::JsonStore &params() const { return params_; }
        utility::JsonStore &params() { return params_; }
        [[nodiscard]] size_t size() const { return size_; }
        [[nodiscard]] byte *buffer() {
            return buffer_ ? (byte *)(buffer_->data_.get()) : nullptr;
        }
        [[nodiscard]] const byte *buffer() const {
            return buffer_ ? (const byte *)buffer_->data_.get() : nullptr;
        }
        [[nodiscard]] BufferErrorState error_state() const { return error_state_; }
        [[nodiscard]] const std::string &error_message() const { return error_message_; }
        [[nodiscard]] double display_timestamp_seconds() const { return dts_; }
        [[nodiscard]] bool display_timestamp_seconds_is_set() const {
            return dts_ != UNSET_DTS;
        }

        void set_display_timestamp_seconds(const double dts) { dts_ = dts; }
        void set_error(const std::string &err) {
            error_message_ = err;
            error_state_   = HAS_ERROR;
        }

        struct BufferData {
            struct BufferDeleter {
                void operator()(byte *ptr) const {
                    operator delete[](ptr, std::align_val_t(1024));
                }
            };

            BufferData(byte *d) : data_(d, BufferDeleter()) {}
            BufferData(size_t sz) : data_(nullptr, BufferDeleter()) {
                byte *ptr = static_cast<byte *>(operator new[](sz, std::align_val_t(1024)));
                data_.reset(ptr);
            }

            std::unique_ptr<byte, BufferDeleter> data_;
        };
        typedef std::shared_ptr<BufferData> BufferDataPtr;

        static std::shared_ptr<ImageBufferRecyclerCache> s_buf_cache;

      private:
        BufferDataPtr buffer_; // use long long to get 16 byte alignment
        size_t size_{0};
        utility::JsonStore params_;
        std::string error_message_;
        BufferErrorState error_state_{NO_ERROR};
        double dts_ = {UNSET_DTS};
    };

    /* Lower level dumb cache that hangs onto BufferDataPtrs after deletion
    of parent Buffer class for re-use.*/
    class ImageBufferRecyclerCache {
      public:
        void store_unwanted_buffer(Buffer::BufferDataPtr &buf, const size_t size);
        Buffer::BufferDataPtr fetch_recycled_buffer(const size_t required_size);
        size_t max_size_ = {512 * 1024 * 1024}; // 0.5GB - probably doesn't need to be that big,
                                                // and need to set this with a pref
        size_t total_size_ = {0};
        typedef std::vector<Buffer::BufferDataPtr> Buffers;
        std::map<size_t, Buffers> recycle_buffer_bin_;
        std::map<utility::time_point, size_t> size_by_time_;
        std::mutex mutex_;
    };


} // namespace media_reader
} // namespace xstudio
