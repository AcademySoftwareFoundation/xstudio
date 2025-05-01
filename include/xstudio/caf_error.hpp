// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "caf/all.hpp"

namespace xstudio {
enum class xstudio_error : uint8_t { error = 1, authentication_error };

inline std::string to_string(xstudio_error x) {
    switch (x) {
    case xstudio_error::error:
        return "error";
    case xstudio_error::authentication_error:
        return "authentication_error";
    default:
        return "-unknown-error-";
    }
}

inline bool from_string(std::string_view in, xstudio_error &out) {
    if (in == "error") {
        out = xstudio_error::error;
        return true;
    } else if (in == "authentication_error") {
        out = xstudio_error::authentication_error;
        return true;
    } else {
        return false;
    }
}

inline bool from_integer(uint8_t in, xstudio_error &out) {
    if (in == 1) {
        out = xstudio_error::error;
        return true;
    } else if (in == 2) {
        out = xstudio_error::authentication_error;
        return true;
    } else {
        return false;
    }
}

template <class Inspector> bool inspect(Inspector &f, xstudio_error &x) {
    return caf::default_enum_inspect(f, x);
}
} // namespace xstudio

#include "xstudio/http_client/caf_http_client_error.hpp"
#include "xstudio/shotgun_client/caf_shotgun_client_error.hpp"
#include "xstudio/media/caf_media_error.hpp"
