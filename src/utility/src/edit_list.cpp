// SPDX-License-Identifier: Apache-2.0
#ifdef _WIN32
#include <numeric>
#else
#include <experimental/numeric>
#endif
#include <functional>
#include <iostream>

#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;

void EditList::extend(const EditList &o) { sl_.insert(sl_.end(), o.sl_.begin(), o.sl_.end()); }

std::vector<std::pair<Uuid, size_t>>
EditList::frame_durations(const TimeSourceMode tsm, const FrameRate &fr) const {
    std::vector<std::pair<Uuid, size_t>> frames;

    switch (tsm) {
    case TimeSourceMode::FIXED:
    case TimeSourceMode::DYNAMIC:
        // sum frames.
        for (const auto &i : sl_)
            frames.emplace_back(std::make_pair(
                i.media_uuid_, static_cast<size_t>(i.frame_rate_and_duration_.frames())));
        break;
    case TimeSourceMode::REMAPPED:
        for (const auto &i : sl_)
            frames.emplace_back(std::make_pair(
                i.media_uuid_, static_cast<size_t>(i.frame_rate_and_duration_.frames(fr))));
        break;
    }
    return frames;
}

std::vector<std::pair<Uuid, double>>
EditList::second_durations(const TimeSourceMode tsm, const FrameRate &fr) const {
    std::vector<std::pair<Uuid, double>> seconds;

    switch (tsm) {
    case TimeSourceMode::FIXED:
        // sum frames.
        for (const auto &i : sl_)
            seconds.emplace_back(
                std::make_pair(i.media_uuid_, i.frame_rate_and_duration_.seconds(fr)));
        break;

    case TimeSourceMode::DYNAMIC:
    case TimeSourceMode::REMAPPED:
        for (const auto &i : sl_)
            seconds.emplace_back(
                std::make_pair(i.media_uuid_, i.frame_rate_and_duration_.seconds()));
        break;
    }

    return seconds;
}

std::vector<std::pair<Uuid, timebase::flicks>>
EditList::flick_durations(const TimeSourceMode tsm, const FrameRate &fr) const {
    std::vector<std::pair<Uuid, timebase::flicks>> _flicks;
    switch (tsm) {
    case TimeSourceMode::FIXED:
        // sum frames.
        for (const auto &i : sl_)
            _flicks.emplace_back(
                std::make_pair(i.media_uuid_, i.frame_rate_and_duration_.duration(fr)));
        break;

    case TimeSourceMode::REMAPPED:
    case TimeSourceMode::DYNAMIC:
        for (const auto &i : sl_)
            _flicks.emplace_back(
                std::make_pair(i.media_uuid_, i.frame_rate_and_duration_.duration()));
        break;
    }

    return _flicks;
}


size_t EditList::duration_frames(const TimeSourceMode tsm, const FrameRate &fr) const {
    long int frames = 0;

    switch (tsm) {
    case TimeSourceMode::FIXED:
    case TimeSourceMode::DYNAMIC:
        // sum frames.
        for (const auto &i : sl_)
            frames += i.frame_rate_and_duration_.frames();

        break;
    case TimeSourceMode::REMAPPED:
        for (const auto &i : sl_)
            frames += i.frame_rate_and_duration_.frames(fr);
        break;
    }

    return static_cast<size_t>(frames);
}

double EditList::duration_seconds(const TimeSourceMode tsm, const FrameRate &fr) const {
    double seconds = 0.0;

    switch (tsm) {
    case TimeSourceMode::FIXED:
        // sum frames.
        for (const auto &i : sl_)
            seconds += i.frame_rate_and_duration_.seconds(fr);
        break;

    case TimeSourceMode::DYNAMIC:
    case TimeSourceMode::REMAPPED:
        for (const auto &i : sl_)
            seconds += i.frame_rate_and_duration_.seconds();
        break;
    }

    return seconds;
}

timebase::flicks
EditList::duration_flicks(const TimeSourceMode tsm, const FrameRate &fr) const {
    timebase::flicks _flicks = timebase::k_flicks_zero_seconds;

    switch (tsm) {
    case TimeSourceMode::FIXED:
        // sum frames.
        for (const auto &i : sl_)
            _flicks += i.frame_rate_and_duration_.duration(fr);
        break;

    case TimeSourceMode::REMAPPED:
    case TimeSourceMode::DYNAMIC:
        for (const auto &i : sl_)
            _flicks += i.frame_rate_and_duration_.duration();
        break;
    }

    return _flicks;
}

EditListSection EditList::media_frame(const int logical_frame, int &media_frame) const {
    if (empty())
        throw std::runtime_error("No frames");

    int tot    = 0;
    bool found = false;
    EditListSection s;
    for (const auto &i : sl_) {
        if ((logical_frame - tot) < i.frame_rate_and_duration_.frames()) {
            s           = i;
            media_frame = logical_frame - tot;
            found       = true;
            break;
        } else
            tot += i.frame_rate_and_duration_.frames();
    }

    if (not found) {
        throw std::runtime_error("No frames left");
    }

    return s;
}

timebase::flicks EditList::media_flick_to_flicks(
    const utility::Uuid &media_source,
    const timebase::flicks media_flick,
    const TimeSourceMode /*tsm*/,
    const FrameRate & /*rate*/) const {

    timebase::flicks tot(timebase::k_flicks_zero_seconds);
    bool found = false;
    for (const auto &i : sl_) {
        if (i.media_uuid_ == media_source) {
            found = true;
            // if (tsm != TimeSourceMode::FIXED) {
            tot += media_flick;
            // } else {
            // tot+= rate.to_flicks()*logical_frame;
            // }
            break;
        }
        tot += i.frame_rate_and_duration_.duration();
    }
    if (not found) {
        throw std::runtime_error("EditList::media_flick_to_flicks - media uuid not found.");
    }
    return tot;
}


timebase::flicks EditList::media_frame_to_flicks(
    const utility::Uuid &media_source,
    const int logical_frame,
    const TimeSourceMode tsm,
    const FrameRate &rate) const {
    timebase::flicks tot(timebase::k_flicks_zero_seconds);
    bool found = false;
    for (const auto &i : sl_) {
        if (i.media_uuid_ == media_source) {
            found = true;
            if (tsm != TimeSourceMode::FIXED) {
                tot += i.frame_rate_and_duration_.rate_.to_flicks() * logical_frame;
            } else {
                tot += rate.to_flicks() * logical_frame;
            }
            break;
        }
        tot += i.frame_rate_and_duration_.duration();
    }

    if (not found) {
        throw std::runtime_error("EditList::media_frame_to_flicks - media uuid not found.");
    }
    return tot;
}

// yup this one is a monster..

int EditList::step(
    const TimeSourceMode tsm,
    const FrameRate &rate,
    const bool forward,
    const float velocity,
    const FrameRateDuration &frame,
    FrameRateDuration &new_frame,
    timebase::flicks &new_seconds) const {
    FrameRateDuration tmp_frame;
    FrameRateDuration seek_frame;
    int tmp_logical_frame = 0;

    seek_frame.set_rate(frame.rate());
    seek_frame.set_duration(frame.duration());
    seek_frame.step(forward, velocity);

    tmp_frame = seek_frame;
    tmp_frame.set_rate(frame.rate());

    tmp_logical_frame = logical_frame(tsm, rate, tmp_frame);

    new_frame   = tmp_frame;
    new_seconds = flicks_from_logical(tmp_logical_frame, tsm, rate);
    return tmp_logical_frame;
}

int EditList::step(
    const TimeSourceMode tsm,
    const FrameRate &override_rate,
    const bool forward,
    const float velocity,
    const int _logical_frame,
    const int playhead_in_frame,
    const int playhead_out_frame,
    timebase::flicks &step_period) const {

    const int in_frame = std::max(0, playhead_in_frame);
    const int out_frame =
        std::min((int)duration_frames(tsm, override_rate) - 1, playhead_out_frame);

    /* This needs more work to deal with velocity and remapped tsm and ping/pong
    and also MAPPED tsm is not working yet*/
    int new_logical_frame = _logical_frame;

    auto step =
        std::chrono::duration_cast<timebase::flicks>(
            (tsm == TimeSourceMode::DYNAMIC ? frame_rate_at_frame(_logical_frame).to_flicks()
                                            : override_rate.to_flicks())) /
        velocity;

    step_period = timebase::flicks(0) + std::chrono::duration_cast<timebase::flicks>(step);

    if (tsm == TimeSourceMode::REMAPPED) {

        // what time does the frame we have map to?
        const timebase::flicks ctime =
            flicks_from_frame(TimeSourceMode::DYNAMIC, _logical_frame);

        // now get the frame stepped by the remap rate
        new_logical_frame = logical_frame(
            TimeSourceMode::DYNAMIC, ctime + override_rate.to_flicks(), override_rate);

    } else {

        if (forward) {
            new_logical_frame = _logical_frame + 1;
            if (new_logical_frame > out_frame) {
                new_logical_frame = in_frame;
            }
        } else {
            new_logical_frame = _logical_frame - 1;
            if (new_logical_frame < in_frame) {
                new_logical_frame = out_frame;
            }
        }
    }

    return new_logical_frame;
}

timebase::flicks EditList::flicks_from_logical(
    const int logical_frame, const TimeSourceMode tsm, const FrameRate &fr) const {
    timebase::flicks tmp_seconds(timebase::k_flicks_zero_seconds);

    switch (tsm) {
    case TimeSourceMode::FIXED:
        tmp_seconds = fr * logical_frame;
        break;

    case TimeSourceMode::REMAPPED:
    case TimeSourceMode::DYNAMIC:
        // seconds need to be calculated..
        {
            int tot = 0;
            for (const auto &i : sl_) {
                if ((logical_frame - tot) < i.frame_rate_and_duration_.frames()) {
                    // get flicks for frames..
                    tmp_seconds += i.frame_rate_and_duration_.rate() * (logical_frame - tot);
                    break;
                } else {
                    tmp_seconds += i.frame_rate_and_duration_.duration();
                    tot += i.frame_rate_and_duration_.frames();
                }
            }
        }
        break;
    }

    // spdlog::debug("{}",logical_frame,timebase::to_seconds(tmp_seconds));

    return tmp_seconds;
}

int EditList::logical_frame(
    const TimeSourceMode tsm,
    const utility::FrameRate &rate,
    const utility::FrameRateDuration &frame) const {
    int tmp_logical_frame = 0;

    switch (tsm) {
    case TimeSourceMode::FIXED:
    case TimeSourceMode::DYNAMIC:
        tmp_logical_frame = frame.frames();
        break;

    case TimeSourceMode::REMAPPED: {
        double target_seconds = frame.seconds(rate);
        double tot            = 0;

        for (const auto &i : sl_) {
            if ((target_seconds - tot) < i.frame_rate_and_duration_.seconds()) {
                // get flicks for frames..
                tmp_logical_frame += std::min(
                    i.frame_rate_and_duration_.frames() - 1,
                    i.frame_rate_and_duration_.frame(
                        FrameRateDuration(target_seconds - tot, frame.rate()), true));
                break;
            } else {
                tmp_logical_frame += i.frame_rate_and_duration_.frames();
                tot += i.frame_rate_and_duration_.seconds();
            }
        }
    } break;
    }

    if (!empty()) {
        if (tmp_logical_frame < 0 or
            tmp_logical_frame >= static_cast<int>(duration_frames(TimeSourceMode::FIXED, rate)))
            throw std::runtime_error("No frames left");
    }

    return tmp_logical_frame;
}

int EditList::logical_frame(
    const TimeSourceMode tsm,
    const timebase::flicks target_flicks,
    const utility::FrameRate &fixed_rate) const {

    int tmp_logical_frame = 0;

    if (tsm == TimeSourceMode::FIXED) {

        tmp_logical_frame = FrameRateDuration(target_flicks, fixed_rate).frames();

    } else { // TimeSourceMode::DYNAMIC or TimeSourceMode::REMAPPED

        timebase::flicks tot(0);

        for (const auto &i : sl_) {

            const FrameRateDuration &section_duration_and_rate = i.frame_rate_and_duration_;

            if ((target_flicks - tot) < section_duration_and_rate.duration()) {

                int frames = tsm == TimeSourceMode::REMAPPED
                                 ? section_duration_and_rate.frame(
                                       FrameRateDuration(target_flicks - tot, fixed_rate), true)
                                 : section_duration_and_rate.frame(target_flicks - tot);

                tmp_logical_frame += std::min(section_duration_and_rate.frames() - 1, frames);
                break;
            } else {
                tmp_logical_frame += section_duration_and_rate.frames();
                tot += section_duration_and_rate.duration();
            }
        }
    }

    if (!empty()) {
        if (tmp_logical_frame < 0 or
            tmp_logical_frame >=
                static_cast<int>(duration_frames(TimeSourceMode::FIXED, fixed_rate))) {
            throw std::runtime_error("No frames left");
        }
    }

    return tmp_logical_frame;
}

EditListSection EditList::skip_sections(int ref_frame, int skip_by) const {
    if (empty())
        throw std::runtime_error("No frames");

    int tot           = 0;
    EditListSection s = sl_[0];
    // find the section corresponding to 'ref_frame', then jump from this to
    // another section, jumping through 'skip_by' sections.
    for (auto i = sl_.begin(); i != sl_.end(); i++) {
        if ((ref_frame - tot) < (*i).frame_rate_and_duration_.frames()) {
            if (skip_by >= 0) {
                while (skip_by--) {
                    i++;
                    if (i == sl_.end()) {
                        --i;
                        break;
                    }
                }
                s = (*i);
            } else {
                while (skip_by++) {
                    --i;
                    if (i == sl_.begin()) {
                        break;
                    }
                }
                s = (*i);
            }
            break;
        } else {
            tot += (*i).frame_rate_and_duration_.frames();
        }
    }
    return s;
}

timebase::flicks EditList::flicks_from_frame(
    const TimeSourceMode tsm, const int logical_frame, const utility::FrameRate &rate) const {
    timebase::flicks result(0);
    if (tsm == TimeSourceMode::FIXED || tsm == TimeSourceMode::REMAPPED) {
        result = FrameRateDuration(logical_frame, rate).duration();
    } else // DYNAMIC
    {
        if ((int)duration_frames(tsm, rate) >= logical_frame) {

            int ltot = 0;
            for (const auto &i : sl_) {
                if ((logical_frame - ltot) < i.frame_rate_and_duration_.frames()) {
                    result += timebase::flicks(
                        i.frame_rate_and_duration_.rate().to_flicks().count() *
                        (logical_frame - ltot));
                    break;
                } else {
                    ltot += i.frame_rate_and_duration_.frames();
                    result += i.frame_rate_and_duration_.duration();
                }
            }
        } else {

            const int overrun_frames = logical_frame - duration_frames(tsm, rate);
            result                   = duration_flicks(tsm, rate);
            if (sl_.size()) {
                const FrameRateDuration &final_section = sl_.back().frame_rate_and_duration_;
                result +=
                    timebase::flicks(overrun_frames * final_section.rate().to_flicks().count());
            }
        }
    }
    return result;
}


FrameRate EditList::frame_rate_at_frame(const int logical_frame) const {
    FrameRate result;
    int frame = 0;
    for (const auto &i : sl_) {

        const FrameRateDuration &section_duration_and_rate = i.frame_rate_and_duration_;
        if (logical_frame < (frame + section_duration_and_rate.frames())) {
            result = section_duration_and_rate.rate();
            break;
        }
        frame += section_duration_and_rate.frames();
    }
    if (!result && !empty()) {
        return sl_.back().frame_rate_and_duration_.rate();
    }
    return result;
}

void EditList::set_uuid(const Uuid &uuid) {
    for (auto &i : sl_)
        i.media_uuid_ = uuid;
}

std::pair<int, int> EditList::flicks_range_to_logical_frame_range(
    const timebase::flicks &in,
    const timebase::flicks &out,
    const TimeSourceMode tsm,
    const FrameRate &override_rate) const {

    std::pair<int, int> rt(0, duration_frames(tsm, override_rate));

    if (in != timebase::flicks(std::numeric_limits<timebase::flicks::rep>::lowest())) {
        try {
            rt.first = logical_frame(tsm, in, override_rate);
        } catch (...) {
        } // ignore out of range
    }
    if (out != timebase::flicks(std::numeric_limits<timebase::flicks::rep>::max())) {
        try {
            rt.second = logical_frame(tsm, out, override_rate);
        } catch (...) {
        } // ignore out of range
    }

    return rt;
}

EditListSection EditList::next_section(
    const timebase::flicks &ref_time,
    const int skip_sections,
    const TimeSourceMode tsm,
    const FrameRate &rate) const {

    timebase::flicks t(0);
    int current_section_index = std::numeric_limits<int>::max();
    for (size_t idx = 0; idx < sl_.size(); ++idx) {

        const FrameRateDuration &section_duration_and_rate = sl_[idx].frame_rate_and_duration_;
        const timebase::flicks section_duration            = section_duration_and_rate.duration(
            tsm == TimeSourceMode::FIXED ? rate : FrameRate());

        if ((t + section_duration) > ref_time) {
            current_section_index = static_cast<int>(idx);
            break;
        }
        t += section_duration;
    }

    if (current_section_index == std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "EditList::next_section error: ref time is at or beyond last section.");
    }

    const int skip_to_index = std::max(
        int(0),
        std::min(static_cast<int>(sl_.size() - 1), current_section_index + skip_sections));

    return sl_[skip_to_index];
}

timebase::flicks EditList::section_start_time(
    const utility::Uuid &media_uuid,
    const TimeSourceMode tsm,
    const FrameRate &override_rate,
    FrameRate &out_rate) const {

    bool found = false;
    timebase::flicks result(0);
    for (const auto &i : sl_) {
        if (i.media_uuid_ == media_uuid) {
            found    = true;
            out_rate = tsm == TimeSourceMode::FIXED ? override_rate
                                                    : i.frame_rate_and_duration_.rate();
            break;
        }
        result += i.frame_rate_and_duration_.duration(
            tsm == TimeSourceMode::FIXED ? override_rate : FrameRate());
    }
    if (!found) {
        throw std::runtime_error("EditList::section_start_time - supplied media uuid is not in "
                                 "this source time list.");
    }
    return result;
}
