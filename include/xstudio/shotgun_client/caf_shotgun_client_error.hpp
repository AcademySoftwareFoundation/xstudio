// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "caf/all.hpp"

namespace xstudio {
namespace shotgun_client {
    enum class shotgun_client_error : uint8_t {
        connection_error = 1,
        authentication_error,
        response_error,
        args_error
    };

    inline std::string to_string(shotgun_client_error x) {
        switch (x) {
        case shotgun_client_error::connection_error:
            return "connection_error";
        case shotgun_client_error::authentication_error:
            return "authentication_error";
        case shotgun_client_error::response_error:
            return "response_error";
        case shotgun_client_error::args_error:
            return "args_error";
        default:
            return "-unknown-error-";
        }
    }

    inline bool from_string(caf::string_view in, shotgun_client_error &out) {
        if (in == "connection_error") {
            out = shotgun_client_error::connection_error;
            return true;
        } else if (in == "authentication_error") {
            out = shotgun_client_error::authentication_error;
            return true;
        } else if (in == "response_error") {
            out = shotgun_client_error::response_error;
            return true;
        } else if (in == "args_error") {
            out = shotgun_client_error::args_error;
            return true;
        } else {
            return false;
        }
    }

    inline bool from_integer(uint8_t in, shotgun_client_error &out) {
        if (in == 1) {
            out = shotgun_client_error::connection_error;
            return true;
        } else if (in == 2) {
            out = shotgun_client_error::authentication_error;
            return true;
        } else if (in == 3) {
            out = shotgun_client_error::response_error;
            return true;
        } else if (in == 4) {
            out = shotgun_client_error::args_error;
            return true;
        } else {
            return false;
        }
    }

    template <class Inspector> bool inspect(Inspector &f, shotgun_client_error &x) {
        return caf::default_enum_inspect(f, x);
    }
} // namespace shotgun_client
} // namespace xstudio