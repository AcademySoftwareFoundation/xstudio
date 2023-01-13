// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/http_client/http_client.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::http_client;

TEST(HttpClientTest, Test) {
    httplib::Client cli("http://localhost");
    cli.set_follow_location(true);
    cli.set_connection_timeout(5, 0);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    auto res = cli.Get("", httplib::Params(), httplib::Headers());
}
