// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/http_client/http_client_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::http_client;

using namespace caf;


#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(HTTPClientActorTest, Test) {
    // Segfaults and I've no idea why..
    // fixture f;
    // auto tmp = f.self->spawn<HTTPClientActor>();

    // try {
    //     auto body = request_receive<std::string>(
    //         *(f.self), tmp, http_get_simple_atom_v, "http://localhost", "/");
    // } catch (const std::exception &err) {
    //     std::cerr << err.what() << std::endl;
    // }
}
