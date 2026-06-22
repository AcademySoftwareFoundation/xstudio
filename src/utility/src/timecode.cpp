// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <fmt/format.h>
#include <iostream>

#include "xstudio/utility/timecode.hpp"

using namespace xstudio::utility;

Timecode::Timecode(
    const unsigned int h,
    const unsigned int m,
    const unsigned int s,
    const unsigned int f,
    const double fr,
    const bool df)
    : hours_(h),
      minutes_(m),
      seconds_(s),
      frames_(f),
      frame_rate_(fr == 0.0 ? 24.0 : fr),
      drop_frame_(df) {
    validate();
}

Timecode::Timecode(const unsigned int f, const double fr, const bool df)
    : frame_rate_(fr == 0.0 ? 24.0 : fr), drop_frame_(df) {
    total_frames(f);
}

Timecode::Timecode(const std::string &timecode, const double fr, const bool df)
    : frame_rate_(fr == 0.0 ? 24.0 : fr), drop_frame_(df) {
    set_timecode(timecode);
    validate();
}

void Timecode::total_frames(const unsigned int f) {
    const unsigned int nominal_fps = nominal_framerate();
    unsigned int frame_input       = f % max_frames();

    if (drop_frame_ && (nominal_fps == 30 || nominal_fps == 60)) {

        // see note below
        const int drop_count = nominal_fps / 15;
        for (unsigned int min = 0; min * nominal_framerate() * 60 <= frame_input; min++) {
            if (min % 10)
                frame_input += drop_count;
        }
    }

    frames_  = frame_input % nominal_fps;
    seconds_ = (frame_input / nominal_fps) % 60;
    minutes_ = ((frame_input / nominal_fps) / 60) % 60;
    hours_   = ((frame_input / nominal_fps) / 60 / 60);
}


void Timecode::set_timecode(const std::string &timecode) {
    unsigned int h, m, s, f;

    if (sscanf(timecode.c_str(), "%2u%*1c%2u%*1c%2u%*1c%2u", &h, &m, &s, &f) == 4) {
        hours_   = h;
        minutes_ = m;
        seconds_ = s;
        frames_  = f;
    } else {
        throw std::invalid_argument("Invalid string passed.");
    }
}

unsigned int Timecode::nominal_framerate() const {
    int nominal_fps = static_cast<int>(frame_rate_);

    switch (nominal_fps) {
    case 23:
    case 29:
    case 59:
        nominal_fps++;
    default:
        break;
    }

    return std::max(1, nominal_fps);
}

unsigned int Timecode::max_frames() const {
    return Timecode(23, 59, 59, nominal_framerate() - 1, frame_rate_, drop_frame_)
        .total_frames();
}

void Timecode::validate() {
    if (frames_ >= nominal_framerate()) {
        frames_ %= nominal_framerate();
        seconds_++;
    }
    if (seconds_ > 59) {
        seconds_ %= 60;
        minutes_++;
    }
    if (minutes_ > 59) {
        minutes_ %= 60;
        hours_++;
    }
    if (hours_ > 23) {
        hours_ %= 24;
    }
    if (drop_frame_ && frames_ == 0 && seconds_ == 0 && minutes_ % 10 != 0) {
        frames_ += 2;
    }
}

unsigned int Timecode::total_frames() const {

    const int nominal_fps = nominal_framerate();

    int total_frames = (hours_ * 60 * 60 * nominal_fps) + minutes_ * 60 * nominal_fps +
                       seconds_ * nominal_fps + frames_;

    if (drop_frame_) {
        const int drop_count    = nominal_fps / 15;
        const int total_minutes = minutes_ + (hours_ * 60);
        total_frames -= drop_count * (total_minutes - total_minutes / 10);
    }

    return total_frames;
}

// Type conversion
std::string Timecode::to_string() const {
    return fmt::format(
        "{:02d}:{:02d}:{:02d}{}{:02d}", hours_, minutes_, seconds_, separator(), frames_);
}

Timecode Timecode::operator+(const Timecode &t) const { return operator+(t.total_frames()); }

Timecode Timecode::operator+(const int &i) const {
    int frameSum = total_frames() + i;
    frameSum %= (max_frames() + 1);
    return Timecode(frameSum, frame_rate_, drop_frame_);
}

Timecode Timecode::operator-(const Timecode &t) const { return operator-(t.total_frames()); }

Timecode Timecode::operator-(const int &i) const {
    int f = total_frames() - i;
    if (f < 0)
        f += max_frames();
    return Timecode(f, frame_rate_, drop_frame_);
}

Timecode Timecode::operator*(const int &i) const {
    int f = total_frames() * i;
    while (f < 0)
        f += max_frames();
    return Timecode(f, frame_rate_, drop_frame_);
}

bool Timecode::operator==(const Timecode &t) const {
    return total_frames() == t.total_frames();
}

bool Timecode::operator!=(const Timecode &t) const { return !(*this == t); }

bool Timecode::operator<(const Timecode &t) const { return total_frames() < t.total_frames(); }

bool Timecode::operator<=(const Timecode &t) const { return (*this < t || *this == t); }

bool Timecode::operator>(const Timecode &t) const { return total_frames() > t.total_frames(); }

bool Timecode::operator>=(const Timecode &t) const { return (*this > t || *this == t); }

void xstudio::utility::to_json(nlohmann::json &j, const Timecode &tc) {
    j["timecode"]  = to_string(tc);
    j["framerate"] = tc.framerate();
    j["dropframe"] = tc.dropframe();
}

void xstudio::utility::from_json(const nlohmann::json &j, Timecode &tc) {
    tc = Timecode(
        j["timecode"].get<std::string>(),
        j["framerate"].get<double>(),
        j["dropframe"].get<bool>());
}

// Note on drop frame - from reading a couple of articles found on google.
//
// There is no drop frame standard for 23.976. Therefore we ignore this case.
//
// For 29.97 drop frame timecode means at the start of every minute two frames are dropped,
// except in every 10th minute
//
// For 59.94 ..you only see this for interlaced video formats which again we are most
// unlikely to have to worry about but we simply drop 4 frames instead of 2.
//
// 00:00:00:00
// 00:00:00:01
// 00:00:00:02
// 00:00:00:03
// 00:00:00:00
// .
// .
// .
// 00:00:59:28
// 00:00:59:29    <----- drop frames happen after this frame
// 00:01:00:02
// 00:01:00:03
// .
// .
// .
// .
// 00:01:59:28
// 00:01:59:29    <----- and here
// 00:02:00:02
// 00:02:00:03
// .
// .
// .
// .
// 00:09:59:28
// 00:09:59:29    <----- not here!
// 00:10:00:00
// 00:10:00:01
