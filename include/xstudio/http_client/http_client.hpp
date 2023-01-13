// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#ifndef CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

namespace xstudio {
namespace http_client {
    class HTTPClient {
      public:
        HTTPClient()          = default;
        virtual ~HTTPClient() = default;
    };
} // namespace http_client
} // namespace xstudio
