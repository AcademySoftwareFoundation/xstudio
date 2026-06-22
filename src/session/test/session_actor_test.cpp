// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::playlist;
using namespace xstudio::session;
using namespace xstudio;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(SessionActorTest, Test) {
    fixture f;
    auto tmp = f.self->spawn<SessionActor>("Test");

    f.self->request(tmp, std::chrono::seconds(10), name_atom_v)
        .receive(
            [&](const std::string &name) { EXPECT_EQ(name, "Test"); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->send(tmp, add_playlist_atom_v, std::string("Playlist_test"));

    JsonStore serial;
    f.self->request(tmp, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &jsn) { serial = jsn; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->send_exit(tmp, caf::exit_reason::user_shutdown);

    std::this_thread::sleep_for(500ms);

    auto tmp2 = f.self->spawn<SessionActor>(serial);

    auto playlist_container_uuid = request_receive_wait<UuidUuidActor>(
        *(f.self),
        tmp2,
        std::chrono::milliseconds(1000),
        add_playlist_atom_v,
        std::string("Playlist_test"));

    f.self->request(tmp2, std::chrono::seconds(10), name_atom_v)
        .receive(
            [&](const std::string &name) { EXPECT_EQ(name, "Test"); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self
        ->request(
            tmp2,
            std::chrono::seconds(10),
            remove_container_atom_v,
            playlist_container_uuid.first)
        .receive(
            [&](const bool &result) { EXPECT_TRUE(result); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->send_exit(tmp2, caf::exit_reason::user_shutdown);
}

TEST(SessionSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};

    std::vector<UuidActor> dt1{};
    std::vector<UuidActor> dt2{};

    auto e = bs.apply(dt1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(dt2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(dt1, dt2) << "Creation from string should be equal";
}
