// SPDX-License-Identifier: Apache-2.0

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/logging.hpp"

#include <gtest/gtest.h>

using namespace xstudio::utility;
ACTOR_TEST_MINIMAL()

TEST(MediaReferenceTest, Test) {
    int frame = 0;
    caf::uri path1(posix_path_to_uri("/tmp/test"));
    MediaReference mr1(path1);
    mr1.set_timecode(Timecode("02:00:00:01"));
    EXPECT_TRUE(mr1.container()) << "Shouldn't have frames";
    EXPECT_EQ(mr1.uri(), path1) << "Should match path1";

    MediaReference mr2(mr1.serialise());
    EXPECT_TRUE(mr2.container()) << "Shouldn't have frames";
    EXPECT_EQ(mr2.uri(), path1) << "Should match path1";
    EXPECT_EQ(mr2.timecode().frames(), static_cast<size_t>(1)) << "Should match 1";
    EXPECT_EQ(mr1, mr2) << "Should match";

    caf::uri path2(posix_path_to_uri("/tmp/test/test.{:04d}.exr"));
    MediaReference mr3(path2, std::string("1-24"));
    EXPECT_FALSE(mr3.container()) << "Should have frames";
    EXPECT_EQ(mr3.uri(), path2) << "Should match path2";
    EXPECT_EQ(mr3.uri_from_frame(1), posix_path_to_uri("/tmp/test/test.0001.exr"))
        << "Should match frame 1";

    MediaReference mr4(mr3.serialise());
    EXPECT_FALSE(mr4.container()) << "Should have frames";
    EXPECT_EQ(mr4.uri(), path2) << "Should match path2";
    EXPECT_EQ(mr4.uri_from_frame(1), posix_path_to_uri("/tmp/test/test.0001.exr"))
        << "Should match frame 1";
    EXPECT_EQ(mr4.frame_count(), 24);
    EXPECT_EQ(mr4.seconds(), 1.0);

    EXPECT_EQ(mr4.uri(0, frame), posix_path_to_uri("/tmp/test/test.0001.exr"));

    MediaReference mr5(path2, std::string("2-10x2"));
    EXPECT_EQ(mr5.uri(0, frame), posix_path_to_uri("/tmp/test/test.0002.exr"));
    EXPECT_EQ(mr5.uri(4, frame), posix_path_to_uri("/tmp/test/test.0010.exr"));
}

TEST(MediaReferenceURITest, Test) {
    MediaReference mr1(posix_path_to_uri("/tmp/test/test.{:04d}.exr"), std::string("1-24"));

    EXPECT_EQ(uri_to_posix_path(mr1.uri()), "/tmp/test/test.{:04d}.exr");
    EXPECT_EQ(
        uri_to_posix_path(mr1.uri(MediaReference::FramePadFormat::FPF_NUKE)),
        "/tmp/test/test.%04d.exr");
    EXPECT_EQ(
        uri_to_posix_path(mr1.uri(MediaReference::FramePadFormat::FPF_SHAKE)),
        "/tmp/test/test.####.exr");

    // MediaReference mr2(posix_path_to_uri("/tmp/test/test.{:03d}.exr"), std::string("1-24"));

    // EXPECT_EQ(mr2.uri(), posix_path_to_uri("/tmp/test/test.{:03d}.exr"));
    // EXPECT_EQ(uri_to_posix_path(mr2.uri(MediaReference::FramePadFormat::FPF_NUKE)),
    // "/tmp/test/test.%03d.exr"); EXPECT_EQ(mr2.uri(MediaReference::FramePadFormat::FPF_SHAKE),
    // posix_path_to_uri("/tmp/test/test.###.exr"));
}