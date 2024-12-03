// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <string>

#include "xstudio/utility/frame_range.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {

    class Marker;
    using Markers = std::list<Marker>;

    class Marker {
      public:
        Marker(const std::string name = "Marker")
            : name_(std::move(name)), uuid_(std::move(utility::Uuid::generate())) {}

        Marker(const utility::JsonStore &jsn);

        [[nodiscard]] utility::Uuid uuid() const { return uuid_; }

        [[nodiscard]] utility::FrameRange range() const { return range_; }
        [[nodiscard]] utility::FrameRate rate() const { return range_.rate(); }

        [[nodiscard]] utility::FrameRate duration() const { return range_.duration(); }
        [[nodiscard]] utility::FrameRate start() const { return range_.start(); }
        [[nodiscard]] utility::FrameRateDuration frame_start() const {
            return range_.frame_start();
        }
        [[nodiscard]] utility::FrameRateDuration frame_duration() const {
            return range_.frame_duration();
        }

        [[nodiscard]] std::string name() const { return name_; }
        [[nodiscard]] std::string flag() const { return flag_; }
        [[nodiscard]] utility::JsonStore prop() const { return prop_; }

        [[nodiscard]] utility::JsonStore serialise() const;

        void set_uuid(const utility::Uuid &uuid) { uuid_ = uuid; }
        void reset_uuid() { uuid_.generate_in_place(); }

        void set_name(const std::string &value) { name_ = value; }
        void set_flag(const std::string &value) { flag_ = value; }
        void set_prop(const utility::JsonStore &value) { prop_ = value; }
        void set_range(const utility::FrameRange &value) { range_ = value; }

        template <class Inspector> friend bool inspect(Inspector &f, Marker &x) {
            return f.object(x).fields(
                f.field("u", x.uuid_),
                f.field("rng", x.range_),
                f.field("name", x.name_),
                f.field("flag", x.flag_),
                f.field("prop", x.prop_));
        }

        bool operator==(const Marker &other) const {
            return uuid_ == other.uuid_ and range_ == other.range_ and name_ == other.name_ and
                   flag_ == other.flag_ and prop_ == other.prop_;
        }

        // private:

      private:
        utility::Uuid uuid_{};
        utility::FrameRange range_{};
        std::string name_{};
        std::string flag_{};
        utility::JsonStore prop_{};
    };

    inline utility::JsonStore serialise_markers(const Markers &markers) {
        auto result = R"([])"_json;
        for (const auto &i : markers)
            result.emplace_back(i.serialise());

        return result;
    }

    // inline std::string to_string(const ItemType it) {
    //     std::string str;
    //     switch (it) {
    //     case IT_NONE:
    //         str = "None";
    //         break;
    //     case IT_GAP:
    //         str = "Gap";
    //         break;
    //     case IT_CLIP:
    //         str = "Clip";
    //         break;
    //     case IT_AUDIO_TRACK:
    //         str = "Audio Track";
    //         break;
    //     case IT_VIDEO_TRACK:
    //         str = "Video Track";
    //         break;
    //     case IT_STACK:
    //         str = "Stack";
    //         break;
    //     case IT_TIMELINE:
    //         str = "Timeline";
    //         break;
    //     }
    //     return str;
    // }

} // namespace timeline
} // namespace xstudio