// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>

#include <caf/type_id.hpp>
#include <nlohmann/json.hpp>

namespace xstudio {
namespace utility {
    class Timecode {
      public:
        // constructors
        Timecode() = default;
        Timecode(
            const unsigned int hour,
            const unsigned int minute,
            const unsigned int second,
            const unsigned int frame,
            const double frame_rate = 30.0,
            const bool drop_frame   = false);
        Timecode(
            const unsigned int frames,
            const double frame_rate = 30.0,
            const bool drop_frame   = false);
        Timecode(
            const std::string &timecode,
            const double frame_rate = 30.0,
            const bool drop_frame   = false);

        [[nodiscard]] unsigned int hours() const { return hours_; }
        [[nodiscard]] unsigned int minutes() const { return minutes_; }
        [[nodiscard]] unsigned int seconds() const { return seconds_; }
        [[nodiscard]] unsigned int frames() const { return frames_; }
        [[nodiscard]] double framerate() const { return frame_rate_; }
        [[nodiscard]] bool dropframe() const { return drop_frame_; }
        [[nodiscard]] unsigned int total_frames() const;

        void hours(const unsigned int h) { hours_ = h; }
        void minutes(const unsigned int m) { minutes_ = m; }
        void seconds(const unsigned int s) { seconds_ = s; }
        void frames(const unsigned int f) { frames_ = f; }
        void framerate(const double fr) { frame_rate_ = fr; }
        void dropframe(const bool df) { drop_frame_ = df; }
        void total_frames(const unsigned int f);

        [[nodiscard]] std::string to_string() const;
        operator int() const { return total_frames(); }

        // operators
        Timecode operator+(const Timecode &) const;
        Timecode operator+(const int &i) const;

        Timecode operator-(const Timecode &) const;
        Timecode operator-(const int &i) const;

        Timecode operator*(const int &i) const;

        bool operator==(const Timecode &) const;
        bool operator!=(const Timecode &) const;

        bool operator<(const Timecode &) const;
        bool operator<=(const Timecode &) const;

        bool operator>(const Timecode &) const;
        bool operator>=(const Timecode &) const;

        friend std::ostream &operator<<(std::ostream &, const Timecode &);

        template <class Inspector> friend bool inspect(Inspector &f, Timecode &x) {
            return f.object(x).fields(
                f.field("hr", x.hours_),
                f.field("mi", x.minutes_),
                f.field("se", x.seconds_),
                f.field("fr", x.frames_),
                f.field("rat", x.frame_rate_),
                f.field("df", x.drop_frame_));
        }

      private:
        unsigned int hours_{0};
        unsigned int minutes_{0};
        unsigned int seconds_{0};
        unsigned int frames_{0};
        double frame_rate_{30.0};
        bool drop_frame_{false};

        void set_timecode(const std::string &timecode);

        [[nodiscard]] char separator() const { return drop_frame_ ? ';' : ':'; }

        [[nodiscard]] unsigned int max_frames() const;
        [[nodiscard]] unsigned int nominal_framerate() const;

        void validate();
    };

    inline std::ostream &operator<<(std::ostream &out, const Timecode &t) {
        out << t.to_string();
        return out;
    }
    inline std::string to_string(const Timecode &tc) { return tc.to_string(); }
    void to_json(nlohmann::json &j, const Timecode &uuid);
    void from_json(const nlohmann::json &j, Timecode &uuid);
} // namespace utility
} // namespace xstudio
