// SPDX-License-Identifier: Apache-2.0

#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::utility;
using namespace xstudio::media;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(StreamDetailSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    StreamDetail u1(
        FrameRateDuration(10, 1.0),
        "test_stream",
        MT_IMAGE,
        "{0}@{1}/{2}",
        Imath::V2f(1920, 1080),
        1.0f,
        1.0);
    StreamDetail u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(MediaDetailSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    MediaDetail u1("test_reader", {StreamDetail(FrameRateDuration(10, 1.0))});
    MediaDetail u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(MediaPointerSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    AVFrameID u1(
        posix_path_to_uri("cookham"),
        10,
        1,
        timebase::k_flicks_24fps,
        "stream",
        "{0}@{1}/{2}",
        "reader",
        caf::actor_addr(),
        JsonStore(
            nlohmann::json::parse(R"({ "happy": true, "pi": 3.141, "arr": [0, 2, 4] })")));
    AVFrameID u2;
    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}
