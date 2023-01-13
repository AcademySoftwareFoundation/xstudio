// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "caf/all.hpp"

namespace xstudio {
namespace media {
    enum class media_error : uint8_t { missing = 1, corrupt, unsupported, unreadable };

    inline std::string to_string(media_error x) {
        switch (x) {
        case media_error::missing:
            return "File missing";
        case media_error::corrupt:
            return "File Corrupt";
        case media_error::unsupported:
            return "File Unsupported";
        case media_error::unreadable:
            return "File unreadable";
        default:
            return "-unknown-error-";
        }
    }

    inline bool from_string(caf::string_view in, media_error &out) {
        if (in == "File missing") {
            out = media_error::missing;
            return true;
        } else if (in == "File Corrupt") {
            out = media_error::corrupt;
            return true;
        } else if (in == "File Unsupported") {
            out = media_error::unsupported;
            return true;
        } else if (in == "File unreadable") {
            out = media_error::unreadable;
            return true;
        } else {
            return false;
        }
    }

    inline bool from_integer(uint8_t in, media_error &out) {
        if (in == 1) {
            out = media_error::missing;
            return true;
        } else if (in == 2) {
            out = media_error::corrupt;
            return true;
        } else if (in == 3) {
            out = media_error::unsupported;
            return true;
        } else if (in == 4) {
            out = media_error::unreadable;
            return true;
        } else {
            return false;
        }
    }

    template <class Inspector> bool inspect(Inspector &f, media_error &x) {
        return caf::default_enum_inspect(f, x);
    }
} // namespace media
} // namespace xstudio