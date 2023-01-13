// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "ffprobe_lib.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"

using namespace xstudio;
using namespace xstudio::ffprobe;
using namespace xstudio::utility;

ACTOR_TEST_MINIMAL()

static const std::string metadata(
    R"foo({
  "format": {
    "bit_rate": 183486,
    "duration": 4.1666669999999995,
    "filename": "/u/al/dev/xstudio/test_resource/media/test.mov",
    "format_long_name": "QuickTime / MOV",
    "format_name": "mov,mp4,m4a,3gp,3g2,mj2",
    "nb_programs": 0,
    "nb_streams": 1,
    "probe_score": 100,
    "size": 95566,
    "start_time": 0.0,
    "tags": {
      "compatible_brands": "qt  ",
      "major_brand": "qt  ",
      "minor_version": "512",
      "uk.co.thefoundry.Application": "Nuke",
      "uk.co.thefoundry.ApplicationVersion": "10.5v7",
      "uk.co.thefoundry.Colorspace": "Gamma2.2",
      "uk.co.thefoundry.Writer": "mov64",
      "uk.co.thefoundry.YCbCrMatrix": "Rec 601"
    }
  },
  "streams": [
    {
      "avg_frame_rate": "24/1",
      "bit_rate": 181248,
      "bits_per_raw_sample": 10,
      "chroma_location": null,
      "codec_long_name": "Apple ProRes (iCodec Pro)",
      "codec_name": "prores",
      "codec_tag": "0x6f637061",
      "codec_tag_string": "apco",
      "codec_time_base": "1/24",
      "codec_type": "video",
      "coded_height": 64,
      "coded_width": 64,
      "color_primaries": null,
      "color_range": "tv",
      "color_space": "smpte170m",
      "color_transfer": null,
      "display_aspect_ratio": "1:1",
      "disposition": {
        "attached_pic": 0,
        "clean_effects": 0,
        "comment": 0,
        "default": 1,
        "dub": 0,
        "forced": 0,
        "hearing_impaired": 0,
        "karaoke": 0,
        "lyrics": 0,
        "original": 0,
        "timed_thumbnails": 0,
        "visual_impaired": 0
      },
      "duration": 4.166666666666667,
      "duration_ts": 10000,
      "field_order": "progressive",
      "has_b_frames": 0,
      "height": 64,
      "id": null,
      "index": 0,
      "level": -99,
      "max_bit_rate": null,
      "nb_frames": 100,
      "pix_fmt": "yuv422p10le",
      "profile": "Proxy",
      "r_frame_rate": "24/1",
      "refs": 1,
      "sample_aspect_ratio": "1:1",
      "start_pts": 0,
      "start_time": 0.0,
      "tags": {
        "encoder": "Apple ProRes 422 Proxy",
        "handler_name": "VideoHandler",
        "language": "eng"
      },
      "time_base": "1/2400",
      "timecode": null,
      "width": 64
    }
  ]
})foo");


TEST(FFProbeTest, Test) {
    FFProbe probe;

    EXPECT_EQ(
        probe.probe_file(posix_path_to_uri(TEST_RESOURCE "/media/test.mov")).dump(2), metadata);
}


// TEST(FFProbeExecTest, Test) {
//     int exit_code;
//     start_logger();

//     auto result = utility::exec(
//         std::vector<std::string>({
//             "ffprobe", "-v", "quiet", "-print_format", "json", "-show_format",
//             "-show_streams", "-find_stream_info", TEST_RESOURCE "/media/test.mov"}),
//         exit_code);

//     EXPECT_EQ(nlohmann::json::parse(result).dump(2), metadata);
// }
