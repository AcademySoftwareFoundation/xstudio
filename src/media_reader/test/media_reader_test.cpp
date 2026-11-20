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

// Stretching or squashing an AudioBuffer replaces its sample memory. size() has to
// follow: the buffer is recycled under that size when the AudioBuffer dies, and
// extend_size() / extend() grow from it, so a stale (larger) size let later writes
// run past the end of the smaller allocation.
TEST(AudioBuffer, ResampleUpdatesSize) {
    const size_t bytes_per_sample = 2 * sizeof(int16_t); // stereo int16

    AudioBuffer squashed;
    squashed.allocate(48000, 2, 2000, audio::SampleFormat::INT16);
    memset(squashed.buffer(), 0, squashed.size());
    squashed.set_display_timestamp_flicks(timebase::k_flicks_zero_seconds);
    // 2000 samples played back over 1/60 s at 48 kHz: 800 samples
    squashed.set_new_sample_rate(48000, timebase::k_flicks_one_second / 60);
    EXPECT_EQ(squashed.num_samples(), 800);
    EXPECT_EQ(squashed.size(), 800 * bytes_per_sample);

    AudioBuffer stretched;
    stretched.allocate(48000, 2, 800, audio::SampleFormat::INT16);
    memset(stretched.buffer(), 0, stretched.size());
    stretched.stretch_samples(2000);
    EXPECT_EQ(stretched.num_samples(), 2000);
    EXPECT_EQ(stretched.size(), 2000 * bytes_per_sample);

    // appending after a resample writes at num_samples(), so needs size() to match
    squashed.extend(stretched);
    EXPECT_EQ(squashed.num_samples(), 2800);
    EXPECT_EQ(squashed.size(), 2800 * bytes_per_sample);
}

TEST(AudioBuffer, ResampleEmpty) {
    // a zero length result used to loop ~2^32 times writing past the buffer
    AudioBuffer to_nothing;
    to_nothing.allocate(48000, 2, 100, audio::SampleFormat::INT16);
    memset(to_nothing.buffer(), 0, to_nothing.size());
    to_nothing.set_display_timestamp_flicks(timebase::k_flicks_zero_seconds);
    to_nothing.set_new_sample_rate(48000, timebase::k_flicks_zero_seconds);
    EXPECT_EQ(to_nothing.num_samples(), 0);
    EXPECT_EQ(to_nothing.size(), 0u);

    // an empty input used to be read from before its start
    AudioBuffer from_nothing;
    from_nothing.allocate(48000, 2, 0, audio::SampleFormat::INT16);
    from_nothing.stretch_samples(10);
    EXPECT_EQ(from_nothing.num_samples(), 10);
    ASSERT_EQ(from_nothing.size(), 10 * 2 * sizeof(int16_t));
    for (size_t i = 0; i < 20; ++i)
        EXPECT_EQ(reinterpret_cast<const int16_t *>(from_nothing.buffer())[i], 0);
}

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
