// SPDX-License-Identifier: Apache-2.0

#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(M44fSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
    Imath::M44f u1(
        1.f, 2.f, 3.f, 4.f, 1.f, 2.f, 3.f, 4.f, 1.f, 2.f, 3.f, 4.f, 1.f, 2.f, 3.f, 4.f);
    Imath::M44f u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(V2iSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};

    Imath::V2i u1(1, 2);
    Imath::V2i u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(V2fSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};

    Imath::V2f u1(1.f, 2.f);
    Imath::V2f u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}

TEST(PointerEventSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};

    PointerEvent u1(
        EventType::ButtonRelease,
        Signature::Button::Middle,
        10,
        20,
        20,
        40,
        Signature::Modifier::AltModifier,
        {5, 10},
        {20, 30});
    PointerEvent u2;

    EXPECT_EQ(u1, u1) << "Creation from string should be equal";

    auto e = bs.apply(u1);
    EXPECT_TRUE(e) << "unable to serialize" << to_string(bs.get_error()) << std::endl;

    binary_deserializer bd{f.system, buf};
    e = bd.apply(u2);
    EXPECT_TRUE(e) << "unable to deserialize" << to_string(bd.get_error()) << std::endl;

    EXPECT_EQ(u1, u2) << "Creation from string should be equal";
}
