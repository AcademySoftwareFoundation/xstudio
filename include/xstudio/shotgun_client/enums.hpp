// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace xstudio {
namespace shotgun_client {
    enum AUTHENTICATION_METHOD { AM_UNDEFINED = 0, AM_SCRIPT, AM_LOGIN, AM_SESSION };

    inline std::string
    authentication_method_to_string(const AUTHENTICATION_METHOD authentication_method) {
        switch (authentication_method) {
        case AM_SCRIPT:
            return "client_credentials";
            break;
        case AM_LOGIN:
            return "password";
            break;
        case AM_SESSION:
            return "session_token";
            break;
        case AM_UNDEFINED:
        default:
            break;
        }
        return "";
    }

    inline AUTHENTICATION_METHOD
    authentication_method_from_string(const std::string &authentication_method) {
        if (authentication_method == authentication_method_to_string(AM_SCRIPT))
            return AM_SCRIPT;
        else if (authentication_method == authentication_method_to_string(AM_LOGIN))
            return AM_LOGIN;
        else if (authentication_method == authentication_method_to_string(AM_SESSION))
            return AM_SESSION;

        return AM_UNDEFINED;
    }

} // namespace shotgun_client
} // namespace xstudio