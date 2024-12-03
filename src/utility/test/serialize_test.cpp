// SPDX-License-Identifier: Apache-2.0

#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/types.hpp"


using namespace xstudio::utility;
using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(UUIDSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    Uuid u1 = Uuid::generate();
    Uuid u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(FrameRateSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    FrameRate u1(0.5f);
    FrameRate u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(FrameRateDurationSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    FrameRateDuration u1(10, 0.5f);
    FrameRateDuration u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(FrameGroupSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    FrameGroup u1(10, 20, 2);
    FrameGroup u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(FrameListSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    FrameList u1({FrameGroup(10, 20, 2)});
    FrameList u2;

    EXPECT_NE(u1, u2) << "Creation from string should not be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(MediaReferenceSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    MediaReference u1(posix_path_to_uri("/hosts/cookham/usersf"), "1-20");
    MediaReference u2;

    // EXPECT_NE(u1, u2);
    EXPECT_EQ(u1, u1);

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(PlaylistTreeSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    PlaylistTree u1("Test Tree");
    u1.insert(PlaylistItem("test2"));
    PlaylistTree u2;

    // EXPECT_NE(u1, u2);
    EXPECT_EQ(u1, u1);

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(UriSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    caf::uri u1 = posix_path_to_uri("/hosts/cookham/usersf");
    caf::uri u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(UriSerializerTest2, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    caf::uri u1 = posix_path_to_uri("//hosts/cookham/usersf");
    caf::uri u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(JsonStoreSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    JsonStore u1({"test", "/hosts/cookham/usersf"});
    JsonStore u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(TimecodeSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    Timecode u1("01:02:04:05");
    Timecode u2;

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}
