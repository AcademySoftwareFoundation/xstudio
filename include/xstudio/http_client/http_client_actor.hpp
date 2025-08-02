// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/http_client/http_client.hpp"
#include "xstudio/utility/uuid.hpp"


namespace xstudio {
namespace http_client {
    class HTTPClientActor : public caf::event_based_actor {
      public:
        HTTPClientActor(
            caf::actor_config &cfg,
            time_t connection_timeout = CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND,
            time_t read_timeout       = CPPHTTPLIB_READ_TIMEOUT_SECOND,
            time_t write_timeout      = CPPHTTPLIB_WRITE_TIMEOUT_SECOND);
        ~HTTPClientActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "HTTPClientActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        time_t connection_timeout_;
        time_t read_timeout_;
        time_t write_timeout_;
    };

    class HTTPWorker : public caf::event_based_actor {
      public:
        HTTPWorker(
            caf::actor_config &cfg,
            time_t connection_timeout = CPPHTTPLIB_CONNECTION_TIMEOUT_SECOND,
            time_t read_timeout       = CPPHTTPLIB_READ_TIMEOUT_SECOND,
            time_t write_timeout      = CPPHTTPLIB_WRITE_TIMEOUT_SECOND);
        ~HTTPWorker() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "HTTPWorker";
        caf::behavior make_behavior() override { return behavior_; }
        std::string get_error_string(const httplib::Error err);
        bool is_safe_url(const std::string &scheme_host_port);

      private:
        caf::behavior behavior_;
    };
} // namespace http_client
} // namespace xstudio
