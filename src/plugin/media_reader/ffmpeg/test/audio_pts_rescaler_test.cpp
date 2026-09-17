// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavutil/mathematics.h>
}

#include "audio_pts_rescaler.hpp"

using xstudio::media_reader::ffmpeg::AudioPtsRescaler;
using Frame = AudioPtsRescaler::Frame;

// Timestamps and frame sizes recorded from streams the audio integrity harness
// generates, read with ffprobe, with the start as AV_CODEC_FLAG2_SKIP_MANUAL
// delivers it: a packet the decoder would trim or drop comes back whole at the
// packet's own pts. Frames were recorded in order from the stream start.
struct Recorded {
    const char *name;
    AVRational timebase;
    int sample_rate;
    int initial_padding;
    bool priming_declared;
    int64_t first_frame_skip;
    int64_t first_packet_pts;
    std::vector<Frame> frames;
};

static const std::vector<Recorded> kStreams = {
#include "audio_pts_rescaler_cases.inc"
};

static AudioPtsRescaler rescaler_for(const Recorded &r, const size_t head_frames) {
    AudioPtsRescaler rescaler(r.timebase, r.sample_rate);
    AudioPtsRescaler::StreamStart start;
    start.frames.assign(
        r.frames.begin(), r.frames.begin() + std::min(head_frames, r.frames.size()));
    start.first_packet_pts = r.first_packet_pts;
    start.initial_padding  = r.initial_padding;
    start.priming_declared = r.priming_declared;
    start.first_frame_skip = r.first_frame_skip;
    rescaler.set_stream_start(start);
    return rescaler;
}

// where a count from the stream's start puts every frame: the truth
static std::vector<int64_t> counted(const Recorded &r, const int64_t first) {
    std::vector<int64_t> positions;
    int64_t at = first;
    for (const auto &f : r.frames) {
        positions.push_back(at);
        at += f.nb_samples;
    }
    return positions;
}

static bool one_frame_size(const Recorded &r) {
    // the last frame recorded may be the stream's tail
    for (size_t i = 0; i + 1 < r.frames.size(); ++i)
        if (r.frames[i].nb_samples != r.frames[0].nb_samples)
            return false;
    return true;
}

// Where the audio starts: at the declared priming's distance before zero, or
// where the first packet is when the container declares none, the decoder's
// own dropped lead-in (vorbis) not counted.
static const std::map<std::string, int64_t> kStart = {
    {"mkv_aac_48k", -1024},
    {"webm_opus_48k", -312},
    {"mkv_mp3_44k1", -1105},
    {"m4a_aac_edit_list_priming", -1024},
    {"mp4_opus_priming", -312},
    {"mp4_mp3_edit_list_priming", -1105},
    {"webm_vorbis_44k1", -128},
    {"webm_vorbis_48k_short_blocks", -128},
    {"avi_ac3_frame_ticks", 0},
    {"mkv_pcm_48k", 0},
    {"mpeg_ps_mp2_90khz_ticks", 25519},
    {"mp4_aac_delayed_track", 22944},
    {"mkv_vorbis_96k", -128},
};

// A recorded stream as a region table: four packets to a file position, as a
// matroska block or an ogg page holds several.
static int64_t pos_of(const size_t k) { return int64_t(1000 + 500 * (k / 4)); }
static void add_regions(AudioPtsRescaler &rescaler, const std::vector<Frame> &frames) {
    rescaler.use_region_table();
    for (size_t k = 0; k < frames.size(); ++k)
        rescaler.add_region(pos_of(k), frames[k].nb_samples, frames[k].pts);
}

// The first frame's position, before the samples the decoder asked dropped
// from it: the first kept sample is that much later.
TEST(AudioPtsRescaler, StreamStartsWhereThePrimingSays) {
    for (const auto &r : kStreams) {
        SCOPED_TRACE(r.name);
        auto rescaler = rescaler_for(r, 8);
        add_regions(rescaler, r.frames);
        EXPECT_EQ(rescaler.region_start(0), kStart.at(r.name));
    }
}

// A fixed frame size: any frame, from its own pts and nothing else, is where a
// count from the start of the stream puts it, however coarse the time_base.
TEST(AudioPtsRescaler, FrameSizePlacesAnyFrameFromItsOwnPts) {
    size_t streams = 0;
    for (const auto &r : kStreams) {
        if (!one_frame_size(r))
            continue;
        SCOPED_TRACE(r.name);
        ++streams;
        auto rescaler = rescaler_for(r, 8);
        rescaler.use_frame_size(r.frames[0].nb_samples);
        const auto truth = counted(r, kStart.at(r.name));
        for (size_t k = 0; k + 1 < r.frames.size(); ++k) {
            EXPECT_EQ(rescaler.position(r.frames[k].pts), truth[k]) << "frame " << k;
        }
    }
    EXPECT_GE(streams, 8u);
}

// An mp4 whose packet durations do not tile (a stitched file steps 1023 or
// 1025 now and then) has pts a sample out; the frame size puts the packet back.
TEST(AudioPtsRescaler, FrameSizeCorrectsSeamsInSampleAccuratePts) {
    AudioPtsRescaler rescaler({1, 48000}, 48000);
    AudioPtsRescaler::StreamStart start;
    for (int i = 0; i < 8; ++i)
        start.frames.push_back({1024LL * i, 1024});
    rescaler.set_stream_start(start);
    rescaler.use_frame_size(1024);
    for (const int64_t k : {239, 240, 300}) {
        EXPECT_EQ(rescaler.position(1024 * k - 1), 1024 * k);
        EXPECT_EQ(rescaler.position(1024 * k + 1), 1024 * k);
    }
}

// A packet, named by where it is in the file and which of the packets there it
// is, starts at the sum of the sizes of the regions before it, whatever its own
// size and whatever theirs.
TEST(AudioPtsRescaler, RegionTablePlacesAnyPacket) {
    for (const auto &r : kStreams) {
        SCOPED_TRACE(r.name);
        auto rescaler = rescaler_for(r, 8);
        add_regions(rescaler, r.frames);
        const auto truth = counted(r, kStart.at(r.name));
        for (size_t k = 0; k < r.frames.size(); ++k) {
            const int64_t region = rescaler.region_index(pos_of(k), int(k % 4));
            EXPECT_EQ(region, int64_t(k));
            EXPECT_EQ(rescaler.region_start(region), truth[k]) << "packet " << k;
        }
    }
}

// The other way: the region that holds a sample, and how deep into it the
// sample is, for the first, a middle and the last sample of every region.
TEST(AudioPtsRescaler, RegionTableFindsTheRegionThatHoldsASample) {
    for (const auto &r : kStreams) {
        SCOPED_TRACE(r.name);
        auto rescaler = rescaler_for(r, 8);
        add_regions(rescaler, r.frames);
        const auto truth = counted(r, kStart.at(r.name));
        for (size_t k = 0; k < r.frames.size(); ++k) {
            const int64_t n = r.frames[k].nb_samples;
            for (const int64_t deep : {int64_t(0), n / 2, n - 1}) {
                const int64_t region = rescaler.region_holding(truth[k] + deep);
                EXPECT_EQ(region, int64_t(k)) << "sample " << truth[k] + deep;
                EXPECT_EQ(truth[k] + deep - rescaler.region_start(region), deep);
            }
        }
        EXPECT_EQ(rescaler.covered_to(), truth.back() + r.frames.back().nb_samples);
        // a sample the table does not reach is not guessed at
        EXPECT_THROW((void)rescaler.region_holding(rescaler.covered_to()), std::runtime_error);
        // before the stream's first region: that region
        EXPECT_EQ(rescaler.region_holding(truth[0] - 5000), 0);
    }
}

// One block of a real film (vorbis in matroska, 48 kHz, 1/1000), 8 packets at
// file position 35798372. Read straight through their pts are 35299 35311 35313
// 35315 35317 35319 35321 35323. After a seek that lands on the block they are
// 35299 35301 35303 35305 35307 35309 35311 35313: only the block's own time is
// stored, the demuxer adds durations for the rest, and the first packet's
// duration depends on the packet before it, which a seek has not read. The
// table places nothing by pts, so both readings name the same regions, and the
// pts it seeks to for any of them is the block's, the one the file stores.
TEST(AudioPtsRescaler, RegionTablePlacesNothingByPts) {
    struct P {
        int64_t pos;
        int nb_samples;
        int64_t pts;
    };
    const std::vector<P> packets = {
        {35695741, 576, 35261},
        {35695741, 1024, 35273},
        {35798372, 576, 35299},
        {35798372, 128, 35311},
        {35798372, 128, 35313},
        {35798372, 128, 35315},
        {35798372, 128, 35317},
        {35798372, 128, 35319},
        {35798372, 128, 35321},
        {35798372, 576, 35323},
        {35799238, 1024, 35339}};
    AudioPtsRescaler rescaler({1, 1000}, 48000);
    rescaler.use_region_table();
    for (const auto &p : packets)
        rescaler.add_region(p.pos, p.nb_samples, p.pts);
    EXPECT_TRUE(rescaler.uses_region_table());
    int64_t expected = 576 + 1024;
    for (int ordinal = 0; ordinal < 8; ++ordinal) {
        const int64_t region = rescaler.region_index(35798372, ordinal);
        EXPECT_EQ(rescaler.region_start(region), expected);
        EXPECT_EQ(rescaler.seek_pts_for_region(region), 35299);
        expected += packets[size_t(2 + ordinal)].nb_samples;
    }
    EXPECT_EQ(rescaler.seek_pts_for_region(rescaler.region_index(35799238, 0)), 35339);
    EXPECT_THROW((void)rescaler.position(35301), std::logic_error);
}

// A packet or a region the table does not hold is an error, not a guess.
TEST(AudioPtsRescaler, RegionTableRejectsWhatItDoesNotHold) {
    AudioPtsRescaler rescaler({1, 1000}, 48000);
    rescaler.use_region_table();
    rescaler.add_region(100, 1024, 0);
    rescaler.add_region(100, 1024, 21);
    rescaler.add_region(900, 1024, 43);
    EXPECT_FALSE(rescaler.has_region(500, 0));
    EXPECT_THROW((void)rescaler.region_index(500, 0), std::runtime_error);
    EXPECT_THROW((void)rescaler.region_index(100, 2), std::runtime_error);
    EXPECT_THROW((void)rescaler.region_start(-1), std::runtime_error);
    EXPECT_THROW((void)rescaler.region_start(3), std::runtime_error);
}

// A codec with no frames, whose pts is in whole samples: a raw PCM demuxer cuts
// packets at any sample after a seek, and the pts is the position.
TEST(AudioPtsRescaler, PtsIsThePositionWhenItIsInWholeSamples) {
    AudioPtsRescaler rescaler({1, 44100}, 44100);
    AudioPtsRescaler::StreamStart start;
    for (int i = 0; i < 8; ++i)
        start.frames.push_back({4096LL * i, 4096});
    rescaler.set_stream_start(start);
    rescaler.use_pts();
    for (const int64_t landing : {3822, 3820, 4096, 31381}) {
        EXPECT_EQ(rescaler.position(landing), landing);
        EXPECT_EQ(rescaler.position(landing + 4096), landing + 4096);
    }
}
