// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "caf/all.hpp"

namespace xstudio {
namespace http_client {
    enum class http_client_error : uint8_t { connection_error = 1, rest_error };

    inline std::string to_string(http_client_error x) {
        switch (x) {
        case http_client_error::connection_error:
            return "connection_error";
        case http_client_error::rest_error:
            return "rest_error";
        default:
            return "-unknown-error-";
        }
    }

    inline bool from_string(std::string_view in, http_client_error &out) {
        if (in == "connection_error") {
            out = http_client_error::connection_error;
            return true;
        } else if (in == "rest_error") {
            out = http_client_error::rest_error;
            return true;
        } else {
            return false;
        }
    }

    inline bool from_integer(uint8_t in, http_client_error &out) {
        if (in == 1) {
            out = http_client_error::connection_error;
            return true;
        } else if (in == 2) {
            out = http_client_error::rest_error;
            return true;
        } else {
            return false;
        }
    }

    template <class Inspector> bool inspect(Inspector &f, http_client_error &x) {
        return caf::default_enum_inspect(f, x);
    }
} // namespace http_client
} // namespace xstudio