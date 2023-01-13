// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::media;

ACTOR_TEST_MINIMAL()

TEST(MediaTest, Test) {
    Media mr1;
    EXPECT_TRUE(mr1.empty());

    EXPECT_EQ(mr1.current(), Uuid());

    mr1.add_media_source(Uuid());
    EXPECT_TRUE(mr1.empty());

    Uuid new_uuid = Uuid::generate();
    mr1.add_media_source(new_uuid);
    EXPECT_FALSE(mr1.empty());

    EXPECT_EQ(mr1.current(), new_uuid);

    mr1.remove_media_source(new_uuid);
    EXPECT_TRUE(mr1.empty());
    EXPECT_EQ(mr1.current(), Uuid());

    EXPECT_FALSE(mr1.set_current(Uuid()));
    EXPECT_FALSE(mr1.set_current(new_uuid));

    mr1.add_media_source(new_uuid);
    EXPECT_TRUE(mr1.set_current(new_uuid));

    Media mr2("test", new_uuid);
    EXPECT_EQ(mr2.current(), new_uuid);
    EXPECT_FALSE(mr2.empty());

    Media mr3(mr2.serialise());
    EXPECT_EQ(mr3.current(), new_uuid);
    EXPECT_FALSE(mr3.empty());
}

TEST(MediaSourceTest, Test) {
    caf::uri path1(posix_path_to_uri("/tmp/test"));
    MediaSource mr1("test", path1);
    EXPECT_TRUE(mr1.media_reference().container()) << "Shouldn't have frames";
    EXPECT_EQ(mr1.media_reference().uri(), path1) << "Should match path1";

    MediaSource mr2(mr1.serialise());
    EXPECT_TRUE(mr2.media_reference().container()) << "Shouldn't have frames";
    EXPECT_EQ(mr2.media_reference().uri(), path1) << "Should match path1";

    caf::uri path2(posix_path_to_uri("/tmp/test/test.{:04d}.exr"));
    MediaSource mr3("test", path2, FrameList(1, 10));
    EXPECT_FALSE(mr3.media_reference().container()) << "Should have frames";
    EXPECT_EQ(mr3.media_reference().uri(), path2) << "Should match path2";
    EXPECT_EQ(
        mr3.media_reference().uri_from_frame(1), posix_path_to_uri("/tmp/test/test.0001.exr"))
        << "Should match frame 1";
    std::vector<std::pair<caf::uri, int>> frames = mr3.media_reference().uris();

    EXPECT_EQ(std::get<0>(frames[0]), posix_path_to_uri("/tmp/test/test.0001.exr"));
    EXPECT_EQ(std::get<0>(frames[9]), posix_path_to_uri("/tmp/test/test.0010.exr"));


    MediaSource mr4(mr3.serialise());
    EXPECT_FALSE(mr4.media_reference().container()) << "Should have frames";
    EXPECT_EQ(mr4.media_reference().uri(), path2) << "Should match path2";
    EXPECT_EQ(
        mr4.media_reference().uri_from_frame(1), posix_path_to_uri("/tmp/test/test.0001.exr"))
        << "Should match frame 1";
}

TEST(MediaStreamTest, Test) {}
