// SPDX-License-Identifier: Apache-2.0
#pragma once

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

    class Buffer {

      public:
        Buffer(const utility::JsonStore &params = utility::JsonStore()) : params_(params) {}
        explicit Buffer(const std::string error_message)
            : error_message_(std::move(error_message)), error_state_(HAS_ERROR) {}
        virtual ~Buffer() = default;

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
        [[nodiscard]] byte *buffer() { return buffer_.get(); }
        [[nodiscard]] const byte *buffer() const { return (const byte *)(buffer_.get()); }
        [[nodiscard]] BufferErrorState error_state() const { return error_state_; }
        [[nodiscard]] const std::string &error_message() const { return error_message_; }

        [[nodiscard]] double display_timestamp_seconds() const { return dts_; }
        bool display_timestamp_seconds_is_set() const { return dts_ != UNSET_DTS; }
        void set_display_timestamp_seconds(const double dts) { dts_ = dts; }
        void set_error(const std::string &err) {
            error_message_ = err;
            error_state_   = HAS_ERROR;
        }

      private:
        std::unique_ptr<byte> buffer_; // use long long to get 16 byte alignment
        size_t size_{0};
        utility::JsonStore params_;
        std::string error_message_;
        BufferErrorState error_state_{NO_ERROR};
        double dts_ = {UNSET_DTS};
    };
} // namespace media_reader
} // namespace xstudio