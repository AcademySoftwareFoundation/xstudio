// SPDX-License-Identifier: Apache-2.0
#include "audio_pts_rescaler.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

using namespace xstudio::media_reader::ffmpeg;

// pts are compared as exact rationals with denominator pkt_timebase.den: a
// position p in samples is p * den, a pts t is t * num * sample_rate.

AudioPtsRescaler::AudioPtsRescaler(const AVRational pkt_timebase, const int sample_rate)
    : pkt_timebase_(pkt_timebase), sample_rate_(sample_rate) {}

int64_t AudioPtsRescaler::pts_to_samples(const int64_t pts) const {
    return av_rescale_rnd(
        pts, int64_t(pkt_timebase_.num) * sample_rate_, pkt_timebase_.den, AV_ROUND_NEAR_INF);
}

int64_t AudioPtsRescaler::rescale(const int64_t pts) const {
    return pts_to_samples(pts) - ts_offset_;
}

// How far the frames' pts fall outside the precision of a pts, in samples, if
// the first of them starts at `first`. A pts is good to half a time_base unit,
// or to one sample where the unit is finer than that.
int64_t
AudioPtsRescaler::pts_error(const int64_t first, const std::vector<Frame> &frames) const {
    const int64_t den     = pkt_timebase_.den;
    const int64_t tb_unit = int64_t(pkt_timebase_.num) * sample_rate_;
    const int64_t slack   = std::max(tb_unit, 2 * den); // twice that precision
    int64_t worst = 0, count = 0;
    for (const auto &f : frames) {
        worst = std::max<int64_t>(
            worst, 2 * std::llabs((first + count) * den - f.pts * tb_unit) - slack);
        count += f.nb_samples;
    }
    return (worst + 2 * den - 1) / (2 * den);
}

// Where the first of these frames starts, from their pts alone: the position
// within a time_base unit of its pts that all their pts fit best, nearest its
// own pts on a tie.
int64_t AudioPtsRescaler::estimate_start(const std::vector<Frame> &frames) const {
    const int64_t ts      = pts_to_samples(frames.front().pts);
    const int64_t tb_unit = std::max<int64_t>(pts_to_samples(1) - pts_to_samples(0), 2);
    int64_t best = ts, best_error = -1, best_dist = 0;
    for (int64_t x = ts - tb_unit; x <= ts + tb_unit; ++x) {
        const int64_t e = pts_error(x, frames);
        const int64_t d = std::llabs(x - ts);
        if (best_error < 0 || e < best_error || (e == best_error && d < best_dist)) {
            best       = x;
            best_error = e;
            best_dist  = d;
        }
    }
    return best;
}

// Where the audio starts. A container that declares its codec's priming has
// placed the first packet, exactly where it says when the pts agree. One that
// declares none could not record the lead-in, so its first pts is that of the
// first kept sample: that sample belongs at the start.
void AudioPtsRescaler::set_stream_start(const StreamStart &start) {
    start_samples_ = 0;
    ts_offset_     = 0;
    if (start.frames.empty())
        return;
    start_samples_ = estimate_start(start.frames);
    if (start.priming_declared) {
        const int64_t declared = -int64_t(start.initial_padding);
        if (start.initial_padding > 0 && pts_error(declared, start.frames) == 0)
            start_samples_ = declared;
    } else {
        ts_offset_ =
            start_samples_ + start.first_frame_skip - pts_to_samples(start.first_packet_pts);
    }
}

void AudioPtsRescaler::use_pts() { method_ = Method::PTS; }

void AudioPtsRescaler::use_frame_size(const int frame_size) {
    method_     = Method::FRAME_SIZE;
    frame_size_ = std::max(frame_size, 1);
}

void AudioPtsRescaler::use_region_table() {
    method_ = Method::REGION_TABLE;
    region_pos_.clear();
    region_start_.clear();
    region_stored_pts_.clear();
    first_at_pos_.clear();
    next_region_start_ = start_samples_;
}

int64_t AudioPtsRescaler::position(const int64_t pts) const {
    if (method_ == Method::REGION_TABLE)
        throw std::logic_error("a region is placed from the region table, not from a pts");

    int64_t start = pts_to_samples(pts);
    if (method_ == Method::FRAME_SIZE) {
        // frame k of the stream: k is how many frame sizes deep the pts is
        const int64_t k =
            av_rescale_rnd(start - start_samples_, 1, frame_size_, AV_ROUND_NEAR_INF);
        start = start_samples_ + k * frame_size_;
    }
    return start - ts_offset_;
}

void AudioPtsRescaler::add_region(const int64_t pos, const int nb_samples, const int64_t pts) {
    // emplace keeps the first region at a pos
    const bool first_at_pos = first_at_pos_.emplace(pos, regions()).second;
    region_pos_.push_back(pos);
    region_start_.push_back(next_region_start_);
    region_stored_pts_.push_back(first_at_pos ? pts : AV_NOPTS_VALUE);
    next_region_start_ += nb_samples;
}

// Packets of one matroska block or one ogg page share a pos and follow one
// another in the file, so the regions at `pos` are a run in the table.
bool AudioPtsRescaler::has_region(const int64_t pos, const int ordinal) const {
    const auto first = first_at_pos_.find(pos);
    if (first == first_at_pos_.end() || ordinal < 0)
        return false;
    const int64_t index = first->second + ordinal;
    return index < regions() && region_pos_[size_t(index)] == pos;
}

int64_t AudioPtsRescaler::region_index(const int64_t pos, const int ordinal) const {
    if (!has_region(pos, ordinal))
        throw std::runtime_error(
            "audio packet " + std::to_string(ordinal) + " at file position " +
            std::to_string(pos) + " is not in the region table");
    return first_at_pos_.find(pos)->second + ordinal;
}

int64_t AudioPtsRescaler::region_start(const int64_t index) const {
    if (index < 0 || index >= regions())
        throw std::runtime_error(
            "audio region " + std::to_string(index) + " is not in the region table");
    return region_start_[size_t(index)] - ts_offset_;
}

// The last region that starts at or before the sample. A sample before the
// stream's first region belongs to that region's seek.
int64_t AudioPtsRescaler::region_holding(const int64_t sample) const {
    if (regions() == 0 || sample >= covered_to())
        throw std::runtime_error(
            "sample " + std::to_string(sample) + " is beyond the audio region table");
    const auto after =
        std::upper_bound(region_start_.begin(), region_start_.end(), sample + ts_offset_);
    return std::max<int64_t>(after - region_start_.begin() - 1, 0);
}

int64_t AudioPtsRescaler::seek_pts_for_region(int64_t index) const {
    (void)region_start(index); // throws for an index the table does not hold
    while (index > 0 && region_stored_pts_[size_t(index)] == AV_NOPTS_VALUE)
        --index;
    if (region_stored_pts_[size_t(index)] == AV_NOPTS_VALUE)
        throw std::runtime_error("the audio region table holds no pts to seek to");
    return region_stored_pts_[size_t(index)];
}
