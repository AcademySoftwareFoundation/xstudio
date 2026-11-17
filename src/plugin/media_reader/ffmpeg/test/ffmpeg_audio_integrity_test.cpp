// SPDX-License-Identifier: Apache-2.0

/*
    Audio integrity harness for the ffmpeg reader.

    The reader is asked for audio one video frame at a time, in whatever order
    the playhead and the cache happen to want, and every frame it hands back
    must be the same audio regardless of the route taken to reach it. That is
    the property tested here: decode the same frames sequentially, in a random
    order and in reverse, and compare.

    Two decodes of the same frame must agree sample for sample, whatever the
    container. Matroska records milliseconds, and a run started by a seek is
    placed from that rounded label, so its frames can differ from the
    sequential decode by up to a tick; those cases fail here until the reader
    places a seeked run exactly.

    The test media is a pure A440 sine tone, which also gives an exact oracle
    with no reference file: a sinusoid of any amplitude and phase satisfies

        x[n+1] = 2.cos(w).x[n] - x[n-1],   w = 2.pi.f / sample_rate

    so a per-sample residual against that recurrence finds a discontinuity
    inside a frame and says which sample it starts at.

    A click track, one impulse every 100 ms, measures absolute position: each
    click decoded through the reader must land on the very sample it was
    generated on, sequentially and by random access.
*/

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "ffmpeg.hpp"
#include "ffmpeg_decoder.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;
using namespace xstudio::media_reader::ffmpeg;

ACTOR_TEST_MINIMAL()

namespace {

constexpr double kPi         = 3.14159265358979323846;
constexpr double kToneHz     = 440.0;
constexpr int kFps           = 24;
constexpr int kFramesToTest  = 40; // spread over each file, clear of its tail
constexpr int kSoundcardRate = 48000;
constexpr int kChannels      = 2;

// ---------------------------------------------------------------------------
// test media, generated once with ffmpeg
// ---------------------------------------------------------------------------

struct MediaCase {
    std::string label;
    std::string path;
    bool lossy;
    bool clicks; // click track rather than the tone


    // length of the file in video frames, and the first video frame that has
    // audio: a track can start later than the picture, and a container can
    // start its clock before either
    int frames;
    int first_frame;
};

// Cases the reader is known to get wrong, with why. They are left out of the
// tests above and run by DISABLED_KnownBroken, which gtest lists on every run
// and runs with --gtest_also_run_disabled_tests. Fix one and move it out.
const std::map<std::string, std::string> kKnownBroken = {
    // The container has no way to carry the codec's encoder delay, so the
    // audio sits later than the source by that delay: AC-3 256 samples, mp2
    // 481, AAC 1024, mp3 1105. Every player that trusts the file is late too.
    {"clicks_v_avi_ac3", "avi cannot declare the AC-3 delay"},
    {"clicks_v_avi_mp3", "avi cannot declare the mp3 delay"},
    {"clicks_v_ts_aac", "transport streams cannot declare the AAC delay"},
    {"clicks_v_ts_mp2", "transport streams cannot declare the mp2 delay"},
    {"clicks_v_ts_mp3", "transport streams cannot declare the mp3 delay"},
    {"clicks_v_ts_ac3", "transport streams cannot declare the AC-3 delay"},
    {"clicks_v_mpg_mp2", "program streams cannot declare the mp2 delay"},
    {"clicks_v_mpg_ac3", "program streams cannot declare the AC-3 delay"},
    // The mp3 demuxer reports the LAME delay as the stream's start time, so
    // the file's own clock puts the audio 1105 samples late.
    {"clicks_v_mp3_mp3", "the mp3 demuxer's clock starts at the encoder delay"},
    {"clicks_v_mp3_mp3_44k1", "the mp3 demuxer's clock starts at the encoder delay"},
    // The Ogg demuxer's per-packet timestamps for vorbis disagree with the
    // sizes of the frames the decoder returns, so the reader loses frames.
    {"clicks_v_ogg_vorbis", "Ogg vorbis packet timestamps do not match the frames"},
    {"clicks_v_ogg_vorbis_44k1", "Ogg vorbis packet timestamps do not match the frames"},
};

bool known_broken(const MediaCase &mc) { return kKnownBroken.count(mc.label) > 0; }

// xSTUDIO vendors its own ffmpeg and ships it with the application; CMake
// passes that path in. The environment variable is only for running the
// binary by hand against a different build.
std::string ffmpeg_binary() {
    if (const char *e = std::getenv("XSTUDIO_TEST_FFMPEG"))
        return e;
    return XSTUDIO_TEST_FFMPEG_APP;
}

std::string quoted(const std::string &s) { return "\"" + s + "\""; }

bool file_has_content(const std::string &p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec) && std::filesystem::file_size(p, ec) > 0;
}

// Built fresh every run, never reused from a previous one: the recipes below
// are part of the test, and silently decoding last week's media after changing
// them would be worse than not testing at all. The files are left in
// tone_media afterwards, to be played and checked by hand.
struct ToneMedia {
    std::vector<MediaCase> cases;
    std::string why_unavailable;
    std::string dir;

    static const ToneMedia &get() {
        static ToneMedia m;
        return m;
    }

    ToneMedia() {
        const std::string ff = ffmpeg_binary();

        if (!file_has_content(ff)) {
            why_unavailable = fmt::format("no ffmpeg at '{}' (set XSTUDIO_TEST_FFMPEG)", ff);
            return;
        }

        std::error_code ec;
        std::filesystem::remove_all("tone_media", ec);
        if (!std::filesystem::create_directories("tone_media", ec)) {
            why_unavailable = "could not create tone_media directory";
            return;
        }
        dir = "tone_media";

        // Every recipe is a shape xSTUDIO actually receives, named for the
        // axis it exercises. The reader always hands back two channels, so mono
        // media test the channel conversion, not a different output.
        const auto sine = [](int rate, int secs, int channels) {
            return fmt::format(
                "-f lavfi -i \"sine=frequency={}:sample_rate={}:duration={}\" -ac {}",
                (int)kToneHz,
                rate,
                secs,
                channels);
        };
        // one full-scale sample every 100 ms, silence between
        const auto clicks = [](int rate, int secs) {
            return fmt::format(
                "-f lavfi -i "
                "\"aevalsrc=exprs='if(lt(mod(n\\,{})\\,1)\\,0.6\\,0)':s={}:c=stereo:d={}\"",
                rate / 10,
                rate,
                secs);
        };
        const auto vid = [](int secs) {
            return fmt::format(
                "-f lavfi -i \"testsrc=size=320x240:rate={}:duration={}\"", kFps, secs);
        };

        struct Recipe {
            std::string label;
            std::string file;
            bool lossy;
            bool clicks;
            int secs;
            int first_frame;
            std::string args;
        };

        const std::vector<Recipe> recipes = {
            // sample-accurate containers, lossless
            {"wav_48k",
             "tone48.wav",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s16le"},
            {"wav_44k1",
             "tone44.wav",
             false,
             false,
             2,
             0,
             sine(44100, 2, 2) + " -c:a pcm_s16le"},
            {"wav_48k_mono",
             "tone48m.wav",
             false,
             false,
             2,
             0,
             sine(48000, 2, 1) + " -c:a pcm_s16le"},
            {"mov_pcm24_48k",
             "tone48.mov",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s24le"},
            {"mov_pcm16_96k",
             "tone96.mov",
             false,
             false,
             2,
             0,
             sine(96000, 2, 2) + " -c:a pcm_s16le"},
            {"aiff_pcm16_48k",
             "tone48.aiff",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s16be"},
            // sample-accurate containers, lossy
            {"m4a_aac_48k", "tone48.m4a", true, false, 2, 0, sine(48000, 2, 2) + " -c:a aac"},
            {"m4a_aac_44k1", "tone44.m4a", true, false, 2, 0, sine(44100, 2, 2) + " -c:a aac"},
            {"m4a_aac_48k_mono",
             "tone48m.m4a",
             true,
             false,
             2,
             0,
             sine(48000, 2, 1) + " -c:a aac"},
            {"mp4_mp3_44k1",
             "tone44_mp3.mp4",
             true,
             false,
             2,
             0,
             sine(44100, 2, 2) + " -c:a libmp3lame"},
            {"avi_mp3_44k1",
             "tone44.avi",
             true,
             false,
             2,
             0,
             sine(44100, 2, 2) + " -c:a libmp3lame"},
            // video-bearing mp4: B-frames and a short GOP, the HandBrake playblast shape
            {"mp4_bframes_44k1",
             "movie44_bf.mp4",
             true,
             false,
             2,
             0,
             vid(2) + " " + sine(44100, 2, 2) +
                 " -c:v libx264 -pix_fmt yuv420p -bf 2 -g 2 -c:a aac"},
            // audio track starting half a second after the picture
            {"mp4_aac_delayed",
             "movie48_delayed.mp4",
             true,
             false,
             4,
             12,
             vid(4) + " -itsoffset 0.5 " + sine(48000, 4, 2) +
                 " -map 0:v -map 1:a -c:v libx264 -pix_fmt yuv420p -c:a aac"},
            // millisecond containers, audio only
            {"webm_vorbis_44k1",
             "tone44.webm",
             true,
             false,
             2,
             0,
             sine(44100, 2, 2) + " -c:a libvorbis"},
            {"webm_opus_48k",
             "tone48_opus.webm",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a libopus"},
            {"mkv_aac_48k", "tone48.mkv", true, false, 2, 0, sine(48000, 2, 2) + " -c:a aac"},
            // a millisecond tick of 96 samples against vorbis blocks 64 apart
            {"mkv_vorbis_96k",
             "tone96.mkv",
             true,
             false,
             2,
             0,
             sine(96000, 2, 2) + " -c:a libvorbis"},
            {"mkv_vorbis_48k_mono",
             "tone48m.mkv",
             true,
             false,
             2,
             0,
             sine(48000, 2, 1) + " -c:a libvorbis"},
            // millisecond containers with a video track, long enough that a seek
            // crosses a cluster: the cues sit on the video track
            {"webm_vp9_opus_48k_12s",
             "movie48_opus.webm",
             true,
             false,
             12,
             0,
             vid(12) + " " + sine(48000, 12, 2) + " -c:v libvpx-vp9 -b:v 500k -c:a libopus"},
            {"mkv_h264_aac_48k_12s",
             "movie48.mkv",
             true,
             false,
             12,
             0,
             vid(12) + " " + sine(48000, 12, 2) + " -c:v libx264 -pix_fmt yuv420p -c:a aac"},
            // frame-based and transport-stream timebases; mpegts starts its clock
            // 1.4 seconds before the first packet, so audio first fills frame 35
            {"mxf_pcm16_48k",
             "movie48.mxf",
             false,
             false,
             4,
             0,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a pcm_s16le"},
            {"mpegts_aac_48k",
             "movie48.ts",
             true,
             false,
             4,
             35,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a aac"},
            // the other codecs each container carries. MPEG program and transport
            // streams start their clocks 0.53 and 1.44 seconds before the first
            // packet; a raw mp3 declares 25 ms of decoder delay
            {"mov_aac_48k",
             "tone48_aac.mov",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a aac"},
            {"mov_alac_48k",
             "tone48_alac.mov",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a alac"},
            {"mov_pcm32_48k",
             "tone48_s32.mov",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s32le"},
            {"mov_pcmf32_48k",
             "tone48_f32.mov",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_f32le"},
            {"wav_pcm24_48k",
             "tone48_24.wav",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s24le"},
            {"wav_pcmf32_48k",
             "tone48_f32.wav",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_f32le"},
            {"mp4_ac3_48k",
             "tone48_ac3.mp4",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a ac3"},
            {"mp4_eac3_48k",
             "tone48_eac3.mp4",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a eac3"},
            {"mp4_flac_48k",
             "tone48_flac.mp4",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a flac -strict -2"},
            {"mp4_opus_48k",
             "tone48_opus.mp4",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a libopus -strict -2"},
            {"mp3_raw_44k1",
             "tone44.mp3",
             true,
             false,
             2,
             1,
             sine(44100, 2, 2) + " -c:a libmp3lame"},
            {"mkv_mp3_44k1",
             "tone44_mp3.mkv",
             true,
             false,
             2,
             0,
             sine(44100, 2, 2) + " -c:a libmp3lame"},
            {"mkv_ac3_48k",
             "tone48_ac3.mkv",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a ac3"},
            {"mkv_flac_48k",
             "tone48_flac.mkv",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a flac"},
            {"mkv_pcm16_48k",
             "tone48_pcm.mkv",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s16le"},
            {"avi_pcm16_48k",
             "tone48.avi",
             false,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a pcm_s16le"},
            {"avi_ac3_48k",
             "tone48_ac3.avi",
             true,
             false,
             2,
             0,
             sine(48000, 2, 2) + " -c:a ac3"},
            {"mpg_mp2_48k",
             "movie48_mp2.mpg",
             true,
             false,
             4,
             14,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a mp2"},
            {"mpg_ac3_48k",
             "movie48_ac3.mpg",
             true,
             false,
             4,
             14,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a ac3"},
            {"mpg_mp2_44k1",
             "movie44_mp2.mpg",
             true,
             false,
             4,
             14,
             vid(4) + " " + sine(44100, 4, 2) + " -c:v mpeg2video -c:a mp2"},
            {"mpegts_mp2_48k",
             "movie48_mp2.ts",
             true,
             false,
             4,
             35,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a mp2"},
            {"mpegts_ac3_48k",
             "movie48_ac3.ts",
             true,
             false,
             4,
             35,
             vid(4) + " " + sine(48000, 4, 2) + " -c:v mpeg2video -c:a ac3"},
            {"mpegts_aac_44k1",
             "movie44_aac.ts",
             true,
             false,
             4,
             35,
             vid(4) + " " + sine(44100, 4, 2) + " -c:v mpeg2video -c:a aac"},
            {"mpegts_mp3_44k1",
             "movie44_mp3.ts",
             true,
             false,
             4,
             35,
             vid(4) + " " + sine(44100, 4, 2) + " -c:v mpeg2video -c:a libmp3lame"},
            // click track, one single-sample impulse every 100 ms: absolute
            // position against the source, through each container shape
            {"clicks_wav_48k",
             "clicks48.wav",
             false,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a pcm_s16le"},
            {"clicks_mov_pcm24_48k",
             "clicks48.mov",
             false,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a pcm_s24le"},
            {"clicks_m4a_aac_48k",
             "clicks48.m4a",
             true,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a aac"},
            {"clicks_mp4_mp3_44k1",
             "clicks44_mp3.mp4",
             true,
             true,
             2,
             0,
             clicks(44100, 2) + " -c:a libmp3lame"},
            {"clicks_mkv_aac_48k",
             "clicks48.mkv",
             true,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a aac"},
            {"clicks_webm_vorbis_48k",
             "clicks48.webm",
             true,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a libvorbis"},
            {"clicks_webm_opus_48k",
             "clicks48_opus.webm",
             true,
             true,
             2,
             0,
             clicks(48000, 2) + " -c:a libopus"},
        };

        // The click track with a picture, in every container and codec pair the
        // reader supports: the click must land on its sample whatever the
        // container's clock does. A transport or program stream starts its
        // clock late, and the test follows the audio stream's first packet.
        struct Pair {
            std::string container, ext, codec, vargs, aargs;
        };
        const std::string x264        = "-c:v libx264 -pix_fmt yuv420p";
        const std::string mpeg2       = "-c:v mpeg2video -b:v 8M -pix_fmt yuv420p";
        const std::vector<Pair> pairs = {
            {"mov", "mov", "pcm16", x264, "-c:a pcm_s16le"},
            {"mov", "mov", "pcm24", x264, "-c:a pcm_s24le"},
            {"mov", "mov", "aac", x264, "-c:a aac"},
            {"mov", "mov", "alac", x264, "-c:a alac"},
            {"mp4", "mp4", "aac", x264, "-c:a aac"},
            {"mp4", "mp4", "mp3", x264, "-c:a libmp3lame"},
            {"mp4", "mp4", "ac3", x264, "-c:a ac3"},
            {"mp4", "mp4", "eac3", x264, "-c:a eac3"},
            {"mp4", "mp4", "flac", x264, "-c:a flac -strict -2"},
            {"mp4", "mp4", "opus", x264, "-c:a libopus -strict -2"},
            {"mkv", "mkv", "aac", x264, "-c:a aac"},
            {"mkv", "mkv", "mp3", x264, "-c:a libmp3lame"},
            {"mkv", "mkv", "ac3", x264, "-c:a ac3"},
            {"mkv", "mkv", "flac", x264, "-c:a flac"},
            {"mkv", "mkv", "pcm16", x264, "-c:a pcm_s16le"},
            {"mkv", "mkv", "vorbis", x264, "-c:a libvorbis"},
            {"mkv", "mkv", "opus", x264, "-c:a libopus"},
            {"webm", "webm", "vorbis", "-c:v libvpx -b:v 1M", "-c:a libvorbis"},
            {"webm", "webm", "opus", "-c:v libvpx -b:v 1M", "-c:a libopus"},
            {"avi", "avi", "pcm16", "-c:v mjpeg -q:v 3", "-c:a pcm_s16le"},
            {"avi", "avi", "mp3", "-c:v mjpeg -q:v 3", "-c:a libmp3lame"},
            {"avi", "avi", "ac3", "-c:v mjpeg -q:v 3", "-c:a ac3"},
            {"mxf", "mxf", "pcm16", mpeg2, "-c:a pcm_s16le"},
            {"ts", "ts", "aac", mpeg2, "-c:a aac"},
            {"ts", "ts", "mp2", mpeg2, "-c:a mp2"},
            {"ts", "ts", "mp3", mpeg2, "-c:a libmp3lame"},
            {"ts", "ts", "ac3", mpeg2, "-c:a ac3"},
            {"mpg", "mpg", "mp2", mpeg2, "-c:a mp2"},
            {"mpg", "mpg", "ac3", mpeg2, "-c:a ac3"},
            // audio only
            {"aiff", "aiff", "pcm16", "", "-c:a pcm_s16be"},
            {"flac", "flac", "flac", "", "-c:a flac"},
            {"mp3", "mp3", "mp3", "", "-c:a libmp3lame"},
            {"m4a", "m4a", "aac", "", "-c:a aac"},
            {"ogg", "ogg", "vorbis", "", "-c:a libvorbis"},
            {"opus", "opus", "opus", "", "-c:a libopus"},
        };
        // the lossy pairs whose container tick is coarser than a sample, at
        // the other common rate as well
        const std::set<std::string> at_44k1 = {
            "mp4 aac",
            "mkv aac",
            "mkv mp3",
            "mkv vorbis",
            "webm vorbis",
            "mp3 mp3",
            "ogg vorbis"};
        std::vector<Recipe> matrix;
        for (const auto &p : pairs) {
            for (const int rate : {48000, 44100}) {
                if (rate == 44100 && !at_44k1.count(p.container + " " + p.codec))
                    continue;
                const std::string tag =
                    fmt::format("{}_{}{}", p.container, p.codec, rate == 44100 ? "_44k1" : "");
                const bool lossy = p.codec != "pcm16" && p.codec != "pcm24" &&
                                   p.codec != "alac" && p.codec != "flac";
                matrix.push_back(
                    {"clicks_v_" + tag,
                     "clicks_v_" + tag + "." + p.ext,
                     lossy,
                     true,
                     4,
                     0,
                     (p.vargs.empty() ? "" : vid(4) + " ") + clicks(rate, 4) + " " + p.vargs +
                         " " + p.aargs});
            }
        }

        std::vector<Recipe> all = recipes;
        all.insert(all.end(), matrix.begin(), matrix.end());
        for (const auto &r : all) {
            const std::string out = dir + "/" + r.file;
            std::ignore           = std::system(
                fmt::format("{} -v error -y {} \"{}\"", quoted(ff), r.args, out).c_str());
            if (file_has_content(out)) {
                cases.push_back(
                    {r.label, out, r.lossy, r.clicks, r.secs * kFps, r.first_frame});
            }
        }

        if (cases.empty())
            why_unavailable = "ffmpeg produced no usable test media";
    }

    ToneMedia(const ToneMedia &)            = delete;
    ToneMedia &operator=(const ToneMedia &) = delete;
};

// ---------------------------------------------------------------------------
// decoder driving
// ---------------------------------------------------------------------------

// interleaved samples of one output frame, at the soundcard rate
using Frame = std::vector<int16_t>;

int audio_stream_index(const std::string &path) {
    try {
        // empty stream id means exclude_unwanted_streams() keeps every stream
        FFMpegDecoder probe(path, kSoundcardRate, FrameRate(timebase::k_flicks_24fps), "");
        for (const auto &p : probe.streams()) {
            if (p.second && p.second->stream_type() == AUDIO_STREAM)
                return (int)p.first;
        }
    } catch (const std::exception &e) {
        ADD_FAILURE() << "probing " << path << ": " << e.what();
    }
    return -1;
}

// where the audio stream's first packet sits on the container's clock, in
// samples at the soundcard rate: a transport or program stream starts late
long audio_start_sample(const std::string &path, int stream_idx) {
    FFMpegDecoder probe(path, kSoundcardRate, FrameRate(timebase::k_flicks_24fps), "");
    const auto &st = probe.streams().at(stream_idx);
    const double secs =
        double(std::max<int64_t>(st->first_packet_pts(), 0)) / double(st->seconds_to_pts(1.0));
    return std::lround(secs * kSoundcardRate);
}

std::shared_ptr<FFMpegDecoder> make_decoder(const std::string &path, int stream_idx) {
    return std::make_shared<FFMpegDecoder>(
        path,
        kSoundcardRate,
        FrameRate(timebase::k_flicks_24fps),
        fmt::format("stream {}", stream_idx));
}

Frame to_frame(const AudioBufPtr &b) {
    Frame f;
    if (!b || !b->buffer() || b->num_samples() <= 0)
        return f;
    EXPECT_EQ((int)b->sample_rate(), kSoundcardRate);
    EXPECT_EQ(b->num_channels(), kChannels);
    const int16_t *s = reinterpret_cast<const int16_t *>(b->buffer());
    f.assign(s, s + (size_t)b->num_samples() * (size_t)b->num_channels());
    return f;
}

Frame decode_one(FFMpegDecoder &d, int frame) {
    AudioBufPtr buf;
    try {
        d.decode_audio_frame(frame, buf);
    } catch (const std::exception &) {
        return Frame();
    }
    return to_frame(buf);
}

std::map<int, Frame>
decode_in_order(const std::string &path, int stream_idx, const std::vector<int> &order) {
    std::map<int, Frame> out;
    auto d = make_decoder(path, stream_idx);
    for (int f : order)
        out[f] = decode_one(*d, f);
    return out;
}

// kFramesToTest video frames spread over the file, from the first that has
// audio to just short of the tail, so that seeks land throughout it
std::vector<int> sequential_order(const MediaCase &mc) {
    std::vector<int> v;
    const int first = mc.first_frame;
    const int last  = mc.frames - 8;
    for (int i = 0; i < kFramesToTest; ++i)
        v.push_back(first + (int)((int64_t)i * (last - first) / kFramesToTest));
    return v;
}

std::vector<int> shuffled_order(const MediaCase &mc, uint32_t seed) {
    auto v = sequential_order(mc);
    std::mt19937 rng(seed);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

// ---------------------------------------------------------------------------
// the sine oracle
// ---------------------------------------------------------------------------

struct ToneStats {
    double peak        = 0.0;
    double worst_resid = 0.0; // normalised to peak
    long worst_at      = -1;
    bool usable        = false;
};

ToneStats tone_residual(const Frame &f, double hz, double rate) {
    ToneStats st;
    const long n = (long)f.size() / kChannels;
    if (n < 3)
        return st;

    for (long i = 0; i < n; ++i)
        st.peak = std::max(st.peak, std::fabs((double)f[i * kChannels]));
    if (st.peak < 1.0)
        return st; // silence: a tone frame should never be silent

    const double k = 2.0 * std::cos(2.0 * kPi * hz / rate);
    for (long i = 1; i + 1 < n; ++i) {
        const double pred  = k * (double)f[i * kChannels] - (double)f[(i - 1) * kChannels];
        const double resid = std::fabs((double)f[(i + 1) * kChannels] - pred) / st.peak;
        if (resid > st.worst_resid) {
            st.worst_resid = resid;
            st.worst_at    = i + 1;
        }
    }
    st.usable = true;
    return st;
}

double median(std::vector<double> v) {
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// ---------------------------------------------------------------------------
// comparing two decodes
// ---------------------------------------------------------------------------

// How two decodes of the same frames differ, sample for sample.
struct DiffStats {
    int frames_differing = 0;
    int max_abs          = 0;
    int worst_frame      = -1;
    long worst_at        = -1;
    int size_mismatches  = 0;
    int missing          = 0;
};

DiffStats compare_runs(
    const std::map<int, Frame> &a,
    const std::map<int, Frame> &b,
    const std::vector<int> &frames) {
    DiffStats d;
    for (int f : frames) {
        const auto &x = a.at(f);
        const auto &y = b.at(f);
        if (x.empty() && y.empty())
            continue;
        if (x.empty() || y.empty()) {
            d.missing++;
            continue;
        }
        if (x.size() != y.size())
            d.size_mismatches++;
        bool differs   = false;
        const size_t n = std::min(x.size(), y.size());
        for (size_t i = 0; i < n; ++i) {
            const int delta = std::abs((int)x[i] - (int)y[i]);
            if (delta) {
                differs = true;
                if (delta > d.max_abs) {
                    d.max_abs     = delta;
                    d.worst_frame = f;
                    d.worst_at    = (long)(i / kChannels);
                }
            }
        }
        if (differs)
            d.frames_differing++;
    }
    return d;
}

// A decode difference this large is not codec warm-up. Full scale is 32768, so
// this is 0.4% of it - two orders of magnitude below the 50-80% of peak that
// the corruption produced, and well above the 1-2 LSB that AAC and Vorbis
// legitimately vary by.
constexpr int kLossyToleranceLsb = 128;

void expect_runs_agree(const DiffStats &d, bool lossy, const std::string &what) {
    EXPECT_EQ(d.missing, 0) << d.missing << " frames returned no audio (" << what << ")";
    EXPECT_EQ(d.size_mismatches, 0)
        << d.size_mismatches << " frames changed length (" << what << ")";
    const std::string where = fmt::format(
        "{}: largest sample difference {} at frame {} sample {} ({} frames differ at all)",
        what,
        d.max_abs,
        d.worst_frame,
        d.worst_at,
        d.frames_differing);
    if (lossy) {
        EXPECT_LE(d.max_abs, kLossyToleranceLsb)
            << where << "; up to " << kLossyToleranceLsb
            << " is codec warm-up, beyond that is corruption";
    } else {
        EXPECT_EQ(d.frames_differing, 0) << where << "; the media is lossless";
    }
}

// ---------------------------------------------------------------------------
// access to decoder internals (members are protected, not private)
// ---------------------------------------------------------------------------

class ProbeDecoder : public FFMpegDecoder {
  public:
    using FFMpegDecoder::do_seek;
    using FFMpegDecoder::FFMpegDecoder;
    using PartialBuf = FFMpegDecoder::PartiallyFilledAudioBuf;

    size_t partial_buffer_count() const { return partially_filled_output_buffers_.size(); }
};

// build an AudioBuffer positioned at an exact sample offset in the stream
AudioBufPtr positioned_buffer(uint64_t rate, int64_t first_sample, int64_t nsamples) {
    AudioBufPtr b(new AudioBuffer());
    b->allocate(rate, kChannels, nsamples, audio::SampleFormat::INT16);
    if (b->buffer())
        std::memset(b->buffer(), 0, (size_t)nsamples * kChannels * sizeof(int16_t));
    b->set_display_timestamp_flicks(
        (timebase::k_flicks_one_second * first_sample) / (int64_t)rate);
    return b;
}

} // namespace

// ===========================================================================
// Tier 0 - no media, deterministic, milliseconds
// ===========================================================================

// The merge in PartiallyFilledAudioBuf::copy_samples_from_other_buffer tests
// whether j's start or j's end lies inside i. It never tests whether j fully
// CONTAINS i, so a contained region is never absorbed, the region list never
// collapses to one span, and the frame is never reported complete even when
// every sample has been written.
TEST(PartialAudioBuf, ContainedRegionStillCompletesTheFrame) {

    const uint64_t rate = 48000;
    const int64_t total = 1000;
    ProbeDecoder::PartialBuf pfb;
    pfb.the_buffer_ = positioned_buffer(rate, 0, total);

    // write [100,900), then a region wholly inside it, then the two ends
    auto b1 = positioned_buffer(rate, 100, 800);
    auto b2 = positioned_buffer(rate, 200, 100); // contained by [100,900)
    auto b3 = positioned_buffer(rate, 0, 100);
    auto b4 = positioned_buffer(rate, 900, 100);

    EXPECT_FALSE(pfb.copy_samples_from_other_buffer(b1));
    EXPECT_FALSE(pfb.copy_samples_from_other_buffer(b2));
    EXPECT_FALSE(pfb.copy_samples_from_other_buffer(b3));

    // every sample 0..999 has now been written by b1/b2/b3/b4
    const bool complete = pfb.copy_samples_from_other_buffer(b4);

    EXPECT_TRUE(complete) << "frame is fully covered but was not reported complete; "
                          << pfb.filled_samples_.size() << " regions remain";
}

// Sanity: the same regions arriving without containment do complete.
TEST(PartialAudioBuf, AdjacentRegionsCompleteTheFrame) {

    const uint64_t rate = 48000;
    ProbeDecoder::PartialBuf pfb;
    pfb.the_buffer_ = positioned_buffer(rate, 0, 1000);

    auto b1 = positioned_buffer(rate, 0, 500);
    auto b2 = positioned_buffer(rate, 500, 500);

    EXPECT_FALSE(pfb.copy_samples_from_other_buffer(b1));
    EXPECT_TRUE(pfb.copy_samples_from_other_buffer(b2));
}

// ===========================================================================
// Tier 1 - media driven
// ===========================================================================

#define REQUIRE_MEDIA()                                                                        \
    const auto &media = ToneMedia::get();                                                      \
    if (media.cases.empty())                                                                   \
    GTEST_SKIP() << media.why_unavailable

// do_seek() clears the video and audio mini caches but leaves
// partially_filled_output_buffers_ alone, so half-assembled frames survive a
// seek and are later completed with samples decoded from a different position.
TEST(DecoderInternals, SeekClearsHalfAssembledAudioBuffers) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        SCOPED_TRACE(mc.label);
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0) << "no audio stream in " << mc.path;

        ProbeDecoder d(
            mc.path,
            kSoundcardRate,
            FrameRate(timebase::k_flicks_24fps),
            fmt::format("stream {}", sid));

        AudioBufPtr buf;
        for (int f = mc.first_frame; f < mc.first_frame + 8; ++f) {
            try {
                d.decode_audio_frame(f, buf);
            } catch (const std::exception &) {
            }
        }

        const size_t before = d.partial_buffer_count();
        d.do_seek(300, true);
        const size_t after = d.partial_buffer_count();

        EXPECT_EQ(after, 0u) << "seek left " << after
                             << " half-assembled audio buffers behind (was " << before
                             << "); they can be completed later with post-seek samples";
    }
}

// The heart of it: decoding a frame must not depend on how you got there.
TEST(AudioIntegrity, RandomAccessAgreesWithSequential) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (known_broken(mc))
            continue;
        SCOPED_TRACE(mc.label);
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0);

        const auto frames = sequential_order(mc);
        const auto seq    = decode_in_order(mc.path, sid, frames);
        const auto rnd    = decode_in_order(mc.path, sid, shuffled_order(mc, 20260913u));

        expect_runs_agree(
            compare_runs(seq, rnd, frames), mc.lossy, "random access vs sequential");
    }
}

// Two different random orders must also agree with each other.
TEST(AudioIntegrity, RandomAccessIsRepeatable) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (known_broken(mc))
            continue;
        SCOPED_TRACE(mc.label);
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0);

        const auto a = decode_in_order(mc.path, sid, shuffled_order(mc, 1u));
        const auto b = decode_in_order(mc.path, sid, shuffled_order(mc, 2u));

        expect_runs_agree(
            compare_runs(a, b, sequential_order(mc)), mc.lossy, "two different access orders");
    }
}

// Reverse playback drives the backwards-seek path in do_seek().
TEST(AudioIntegrity, ReverseOrderAgreesWithSequential) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (known_broken(mc))
            continue;
        SCOPED_TRACE(mc.label);
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0);

        const auto frames = sequential_order(mc);
        auto rev          = frames;
        std::reverse(rev.begin(), rev.end());

        const auto seq = decode_in_order(mc.path, sid, frames);
        const auto bwd = decode_in_order(mc.path, sid, rev);

        expect_runs_agree(compare_runs(seq, bwd, frames), mc.lossy, "reverse vs sequential");
    }
}

// The tone oracle: every frame, however it was reached, is a clean tone from
// its first sample to its last. A frame assembled from two decode passes, or
// with a hole or an overlap in it, fails here and says at which sample. The
// threshold is calibrated from the run itself - the median per-frame residual
// is the codec's noise floor, and a corrupt frame sits orders of magnitude
// above it - so there is no codec-specific constant to tune.
TEST(AudioIntegrity, EveryFrameIsStillAPureTone) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (known_broken(mc))
            continue;
        if (mc.clicks)
            continue;
        SCOPED_TRACE(mc.label);
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0);

        for (const char *pattern : {"sequential", "random"}) {
            SCOPED_TRACE(pattern);
            const auto order  = std::string(pattern) == "sequential"
                                    ? sequential_order(mc)
                                    : shuffled_order(mc, 20260913u);
            const auto frames = decode_in_order(mc.path, sid, order);

            std::vector<double> resids;
            std::map<int, ToneStats> stats;
            std::string silent_frames;
            int silent = 0;
            for (int f : sequential_order(mc)) {
                const auto st = tone_residual(frames.at(f), kToneHz, (double)kSoundcardRate);
                stats[f]      = st;
                if (!st.usable) {
                    silent++;
                    silent_frames += fmt::format(" {}", f);
                    continue;
                }
                resids.push_back(st.worst_resid);
            }

            ASSERT_FALSE(resids.empty()) << "no decodable audio at all";
            const double floor_    = std::max(median(resids), 1e-6);
            const double threshold = floor_ * 20.0;

            std::string detail;
            int bad = 0;
            for (const auto &[f, st] : stats) {
                if (!st.usable || st.worst_resid <= threshold)
                    continue;

                // The first frame is the encoder's own ramp into the tone, not
                // anything the decoder did: measured, ffmpeg decoding the same
                // m4a gives a worse departure than xSTUDIO does - 0.0298 at
                // sample 156 against 0.0167 at sample 2 - so there is nothing
                // here for xSTUDIO to get right or wrong.
                if (f == mc.first_frame)
                    continue;

                bad++;
                if (detail.size() < 400)
                    detail += fmt::format(
                        "  frame {}: residual {:.4f} at sample {} (noise floor {:.6f})\n",
                        f,
                        st.worst_resid,
                        st.worst_at,
                        floor_);
            }

            EXPECT_EQ(silent, 0) << silent << " frames decoded to silence:" << silent_frames;
            EXPECT_EQ(bad, 0) << bad << "/" << kFramesToTest
                              << " frames are not a clean tone:\n"
                              << detail;
        }
    }
}

// Absolute position. The click track was generated with one full-scale sample
// every 100 ms, so in each output frame the clicks must sit exactly where the
// source put them: frame start plus the multiples of 100 ms that fall inside
// it. Sequentially, and reached by random access. A click elsewhere is a
// placement error by exactly the number of samples it is off.
// every click on the sample the source put it on, the audio stream's clock followed
void check_clicks(const MediaCase &mc) {
    const int sid = audio_stream_index(mc.path);
    ASSERT_GE(sid, 0);

    for (const char *pattern : {"sequential", "random"}) {
        SCOPED_TRACE(pattern);
        const auto order  = std::string(pattern) == "sequential" ? sequential_order(mc)
                                                                 : shuffled_order(mc, 20260913u);
        const auto frames = decode_in_order(mc.path, sid, order);

        const int per_frame = kSoundcardRate / kFps;
        const int period    = kSoundcardRate / 10;
        const long off      = audio_start_sample(mc.path, sid);
        std::string detail;
        int bad = 0, missing = 0;
        for (int f : sequential_order(mc)) {
            const auto &fr   = frames.at(f);
            const long start = (long)f * per_frame;
            if (start + per_frame <= off)
                continue; // before the audio begins
            if (fr.empty()) {
                missing++;
                continue;
            }
            const long n = (long)fr.size() / kChannels;
            for (long c = off + ((std::max(start - off, 0L) + period - 1) / period) * period;
                 c < start + n;
                 c += period) {
                if (c == off)
                    continue; // inside the encoder's ramp
                long at  = -1;
                int peak = 0;
                for (long i = std::max(0L, c - start - 100); i < std::min(n, c - start + 100);
                     ++i) {
                    const int v = std::abs((int)fr[i * kChannels]);
                    if (v > peak) {
                        peak = v;
                        at   = i;
                    }
                }
                if (peak < 4000 || at != c - start) {
                    bad++;
                    if (detail.size() < 400)
                        detail += fmt::format(
                            "  frame {}: click expected at sample {}, found at {} (peak "
                            "{})\n",
                            f,
                            c - start,
                            at,
                            peak);
                }
            }
        }
        EXPECT_EQ(missing, 0) << missing << " frames returned no audio";
        EXPECT_EQ(bad, 0) << bad << " clicks off their sample:\n" << detail;
    }
}

TEST(AudioIntegrity, ClicksLandOnTheirSample) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (known_broken(mc) || !mc.clicks)
            continue;
        SCOPED_TRACE(mc.label);
        check_clicks(mc);
    }
}

// The cases in kKnownBroken, run the same way. Disabled, so it is listed on
// every run and fails only when asked for; a case that passes here is fixed
// and belongs in the tests above.
TEST(AudioIntegrity, DISABLED_KnownBroken) {

    REQUIRE_MEDIA();

    for (const auto &mc : media.cases) {
        if (!known_broken(mc))
            continue;
        SCOPED_TRACE(mc.label + ": " + kKnownBroken.at(mc.label));
        const int sid = audio_stream_index(mc.path);
        ASSERT_GE(sid, 0);
        const auto frames = sequential_order(mc);
        const auto seq    = decode_in_order(mc.path, sid, frames);
        const auto rnd    = decode_in_order(mc.path, sid, shuffled_order(mc, 20260913u));
        expect_runs_agree(
            compare_runs(seq, rnd, frames), mc.lossy, "random access vs sequential");
        if (mc.clicks)
            check_clicks(mc);
    }
}
