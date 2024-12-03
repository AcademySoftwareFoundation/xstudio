// SPDX-License-Identifier: Apache-2.0
#ifndef _WIN32
#include <experimental/numeric>
#endif
#include <functional>
#include <iostream>

#include "xstudio/utility/frame_rate_and_duration.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;

double FrameRateDuration::seconds(const FrameRate &override) const {
    double seconds;
    // std::cout << override.count() << std::endl;
    // std::cout << rate_.count() << std::endl;

    if (override.count()) {
        seconds = timebase::to_seconds(duration_) /
                  (static_cast<double>(rate_.count()) / static_cast<double>(override.count()));
    } else {
        seconds = timebase::to_seconds(duration_);
    }
    return seconds;
}

int FrameRateDuration::frames(const FrameRate &override) const {
    long int frames = 0;
    if (override.count()) {
        frames = (long)std::round(duration_.count() / override.count());
    } else if (rate_.count()) {
        frames = (long)std::round(duration_.count() / rate_.count());
    }
    return static_cast<int>(frames);
}

timebase::flicks FrameRateDuration::duration(const FrameRate &override) const {
    timebase::flicks _flicks;

    if (override.count()) {
        _flicks = timebase::to_flicks(seconds(override));
    } else {
        _flicks = duration_;
    }

    return _flicks;
}

int FrameRateDuration::frame(const timebase::flicks flicks) const {
    return static_cast<int>(std::floor(flicks / rate_.to_flicks()));
}

int FrameRateDuration::frame(const FrameRateDuration &rt, const bool remapped) const {
    long int val;
    if (remapped)
        val = std::round(rt.seconds() / rate_.to_seconds());
    else
        val = std::round(rt.seconds() / rt.rate().to_seconds());

    return static_cast<int>(val);
}

void xstudio::utility::from_json(const nlohmann::json &j, FrameRateDuration &rt) {
    FrameRate r;
    long int ticks;

    j.at("rate").get_to(r);
    j.at("duration").get_to(ticks);

    rt.set_duration(timebase::flicks(ticks));
    rt.set_rate(r);
}

void xstudio::utility::to_json(nlohmann::json &j, const FrameRateDuration &rt) {
    j = nlohmann::json{
        {"rate", rt.rate()},
        {"duration", rt.duration().count()},
    };
}

FrameRateDuration FrameRateDuration::operator-(const FrameRateDuration &other) {
    return FrameRateDuration(frames() - frame(other), rate_);
}

FrameRateDuration FrameRateDuration::operator+(const FrameRateDuration &other) {
    return FrameRateDuration(frames() + frame(other), rate_);
}

FrameRateDuration &FrameRateDuration::operator+=(const FrameRateDuration &other) {
    set_frames(frames() + other.frames());
    return *this;
}

FrameRateDuration &FrameRateDuration::operator-=(const FrameRateDuration &other) {
    set_frames(frames() - other.frames());
    return *this;
}


FrameRateDuration
FrameRateDuration::subtract_frames(const FrameRateDuration &other, const bool remapped) const {
    return FrameRateDuration(frames() - other.frames(remapped ? FrameRate() : rate_), rate_);
}

FrameRateDuration
FrameRateDuration::subtract_seconds(const FrameRateDuration &other, const bool remapped) const {
    return FrameRateDuration(seconds() - other.seconds(remapped ? FrameRate() : rate_), rate_);
}

FrameRateDuration
FrameRateDuration::add_frames(const FrameRateDuration &other, const bool remapped) const {
    return FrameRateDuration(frames() + other.frames(remapped ? FrameRate() : rate_), rate_);
}

FrameRateDuration
FrameRateDuration::add_seconds(const FrameRateDuration &other, const bool remapped) const {
    return FrameRateDuration(seconds() + other.seconds(remapped ? FrameRate() : rate_), rate_);
}

void FrameRateDuration::set_rate(const FrameRate &rate, bool maintain_duration) {
    if (maintain_duration) {
        auto tmp = frames();
        rate_    = rate;
        set_frames(tmp);
    } else {
        rate_ = rate;
    }
}
