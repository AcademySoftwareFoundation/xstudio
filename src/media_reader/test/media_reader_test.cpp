// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;

ACTOR_TEST_MINIMAL()

// TEST(ImageBuffer, Test) {
// 	ImageBuffer mb;
// 	mb.allocate(10);
// 	EXPECT_EQ(mb.size(), unsigned(10));
// 	media_reader::byte *b = mb.allocate(20);
// 	EXPECT_EQ(mb.size(), unsigned(20));
// 	EXPECT_EQ(mb.buffer(), b);
// }


TEST(MediaReader, Test) {
    // MediaReader mr("test");
    // caf::uri path = posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm");
    // EXPECT_EQ(mr.name(), "test");
    // EXPECT_EQ(mr.supported(path, std::array<uint8_t, 16>()), MRC_NO);

    // EXPECT_FALSE(mr.image(media::AVFrameID(path)));
}

TEST(MediaReaderManager, Test) {
    // MediaReaderManager mrm;

    // mrm.add_path(xstudio_root("/plugin"));
    // mrm.load_plugins();

    // EXPECT_EQ(mrm.names().size(), unsigned(4));
    // caf::uri path    = posix_path_to_uri(TEST_RESOURCE "/media/test.0001.ppm");
    // caf::uri badpath = posix_path_to_uri(TEST_RESOURCE "/media/test.ppm");

    // EXPECT_TRUE(mrm.image(media::AVFrameID(path, 0)));
    // EXPECT_THROW(auto i = mrm.image(media::AVFrameID(badpath, 0)), std::runtime_error);
}
