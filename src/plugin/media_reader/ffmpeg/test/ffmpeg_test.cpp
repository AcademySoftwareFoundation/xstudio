// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "ffmpeg_decoder.hpp"
#include "ffmpeg.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;
using namespace xstudio::media_reader::ffmpeg;

ACTOR_TEST_MINIMAL()

// TEST(FFMpegMediaReaderTest, Test) {
//     FFMpegMediaReader ffmmr;
//     caf::uri good = posix_path_to_uri(TEST_RESOURCE "/media/test.mov");

//     EXPECT_EQ(ffmmr.supported(good, get_signature(good)), MRC_FULLY) << "Should be
//     supported"; EXPECT_EQ(
//         ffmmr.supported(
//             posix_path_to_uri(TEST_RESOURCE "/media/test.0001."), get_signature(good)),
//         MRC_MAYBE)
//         << "Should be supported";

//     EXPECT_TRUE(ffmmr.image(media::AVFrameID(good, 0))) << "Should be supported";
// }


// TEST(FFMPEGLeaker, TEST) {
//    auto path = "/jobs/UAP/IO/london/incoming/2022_06/2022_06_17/client/"
//                "Graded_Media_for_DNEG_Showreel_Aspera_Package/DNE/EP6/106_acc_0120_GRADED.mov";

//    auto decoder = new FFMpegDecoder(path, 44100, VIDEO_STREAM, "stream 0");

//    ImageBufPtr rt;

//    for (auto i = 0; i < 261; i++) {
//        decoder->decode_video_frame(i, rt);
//        rt.reset();
//    }
//    for (auto i = 0; i < 261; i++) {
//        decoder->decode_video_frame(i, rt);
//        rt.reset();
//    }

//    delete decoder;
//}
