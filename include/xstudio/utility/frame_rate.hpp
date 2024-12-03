// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/type_id.hpp>
#include <chrono>
#include <flicks.hpp>
#include <nlohmann/json.hpp>

#include "xstudio/utility/enums.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace utility {

    static timebase::flicks fps_to_flicks(const double s) {
        auto result = std::chrono::duration_cast<timebase::flicks>(
            std::chrono::duration<double>{1.0 / s});

        // if result is close to a standard rate tweak it.
        std::vector<timebase::flicks> std_rates = {
            timebase::flicks(3675),     // 192000 fps
            timebase::flicks(7350),     // 96000 fps
            timebase::flicks(8000),     // 88200 fps
            timebase::flicks(14700),    // 48000 fps
            timebase::flicks(16000),    // 44100 fps
            timebase::flicks(22050),    // 32000 fps
            timebase::flicks(29400),    // 24000 fps
            timebase::flicks(32000),    // 22050 fps
            timebase::flicks(44100),    // 16000 fps
            timebase::flicks(88200),    // 8000 fps
            timebase::flicks(5880000),  // 120 fps
            timebase::flicks(5885880),  // 120 * 1000/1001 (~119.88) fps
            timebase::flicks(7056000),  // 100 fps
            timebase::flicks(7840000),  // 90 fps
            timebase::flicks(11760000), // 60 fps
            timebase::flicks(11771760), // 60 * 1000/1001 (~59.94) fps
            timebase::flicks(14112000), // 50 fps
            timebase::flicks(14700000), // 48 fps
            timebase::flicks(23520000), // 30 fps
            timebase::flicks(23543520), // 30 * 1000/1001 (~29.97)  fps
            timebase::flicks(28224000), // 25 fps
            timebase::flicks(29400000), // 24 fps
            timebase::flicks(29429400)  // 24 * 1000/1001 (~23.976) fps fps
        };

        for (const auto &i : std_rates) {
            if (result.count() > i.count() - 3 and result.count() < i.count() + 3) {
                result = i;
                break;
            }
            if (result < i)
                break;
        }

        return result;
    }


    class FrameRate : public timebase::flicks {
        
      public:

        inline static const std::map<std::string, timebase::flicks> rate_string_to_flicks = {
            {"23.976", timebase::flicks(29429400)},
            {"24.0", timebase::flicks(29400000)},
            {"25.0", timebase::flicks(28224000)},
            {"29.97", timebase::flicks(23543520)},
            {"30.0", timebase::flicks(23520000)},
            {"48.0", timebase::flicks(14700000)},
            {"50.0", timebase::flicks(14112000)},
            {"59.94", timebase::flicks(11771760)},
            {"60.0", timebase::flicks(11760000)},
            {"90.0", timebase::flicks(7840000)},
            {"100.0", timebase::flicks(7056000)},
            {"119.88", timebase::flicks(5885880)},
            {"120.0", timebase::flicks(5880000)}};

        FrameRate(const timebase::flicks &flicks = timebase::k_flicks_zero_seconds)
            : timebase::flicks(flicks) {}
        FrameRate(const double seconds) : FrameRate(timebase::to_flicks(seconds)) {}
        FrameRate(const std::string &rate_string) {
            const auto p = rate_string_to_flicks.find(rate_string);
            if (p != rate_string_to_flicks.end()) {
                *this = p->second;
            } else {
                try {
                    double d = std::stod(rate_string);
                    *this    = timebase::to_flicks(1.0 / d);
                } catch (...) {
                    *this = timebase::k_flicks_zero_seconds;
                }
            }
        }

        FrameRate(const FrameRate &) = default;
        FrameRate(FrameRate &&)      = default;
        ~FrameRate()                 = default;

        [[nodiscard]] std::chrono::microseconds to_microseconds() const {
            return std::chrono::duration_cast<std::chrono::microseconds>(*this);
        }

        operator timebase::flicks() const { return this->to_flicks(); }
        operator const timebase::flicks & () const { return *this; }        

        // [[nodiscard]] std::chrono::nanoseconds::rep count() const { return count(); }
        [[nodiscard]] double to_seconds() const { return timebase::to_seconds(*this); }
        [[nodiscard]] double to_fps() const {
            double fps = 0.0;
            if (count())
                fps = 1.0 / timebase::to_seconds(*this);
            return fps;
        }
        [[nodiscard]] timebase::flicks to_flicks() const { return timebase::flicks(this->count()); }

        FrameRate &operator=(const FrameRate &) = default;
        FrameRate &operator=(FrameRate &&)      = default;

        //operator bool() const { return count() != 0; }

        template <class Inspector> friend bool inspect(Inspector &f, FrameRate &x) {
            return f.object(x).fields(f.field("flick", static_cast<timebase::flicks &>(x)));
        }
    };

    inline std::string to_string(const FrameRate &v) { return std::to_string(v.to_fps()); }
    inline void from_json(const nlohmann::json &j, FrameRate &r) { r = timebase::flicks(j); }
    inline void to_json(nlohmann::json &j, const FrameRate &r) { j = r.count(); }
} // namespace utility
} // namespace xstudio