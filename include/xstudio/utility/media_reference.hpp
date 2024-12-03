// SPDX-License-Identifier: Apache-2.0
// container to handle sequences/mov files etc..
#pragma once

#include <caf/uri.hpp>
#include <limits>

#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/frame_rate_and_duration.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/timecode.hpp"

namespace xstudio {
namespace utility {
    class MediaReference {
      public:
        enum FramePadFormat { FPF_XSTUDIO = 0, FPF_SHAKE, FPF_NUKE };

        MediaReference(
            caf::uri uri          = caf::uri(),
            const bool container  = true,
            const FrameRate &rate = FrameRate(timebase::k_flicks_one_twenty_fourth_of_second));

        MediaReference(
            caf::uri uri,
            std::string frame_list,
            const bool container  = false,
            const FrameRate &rate = FrameRate(timebase::k_flicks_one_twenty_fourth_of_second));

        MediaReference(
            caf::uri uri,
            FrameList frame_list,
            const bool container  = false,
            const FrameRate &rate = FrameRate(timebase::k_flicks_one_twenty_fourth_of_second));

        MediaReference(const JsonStore &jsn);

        MediaReference(const MediaReference &o) = default;

        MediaReference &operator=(const MediaReference &o) = default;

        [[nodiscard]] virtual JsonStore serialise() const;
        virtual ~MediaReference() = default;

        [[nodiscard]] caf::uri uri(const FramePadFormat fpf = FPF_XSTUDIO) const;
        [[nodiscard]] bool container() const { return container_; }
        [[nodiscard]] int frame_count() const { return duration_.frames(); }
        [[nodiscard]] int offset() const { return offset_; }
        [[nodiscard]] double seconds(const FrameRate &override = FrameRate()) const {
            return duration_.seconds(override);
        }
        [[nodiscard]] FrameRateDuration duration() const { return duration_; }

        [[nodiscard]] std::vector<std::pair<caf::uri, int>> uris() const;
        [[nodiscard]] std::optional<caf::uri> uri_from_frame(const int sequence_frame) const;
        [[nodiscard]] std::optional<caf::uri>
        uri(const int logical_frame, int &file_frame) const;

        void set_timecode_from_frames();
        void set_uri(const caf::uri &uri);
        void set_frame_list(const FrameList &fl);
        void set_rate(const FrameRate &rate);
        void set_offset(const int offset);

        void set_duration(const FrameRateDuration &frd) { duration_ = frd; }
        [[nodiscard]] FrameRate rate() const { return duration_.rate(); }
        [[nodiscard]] const FrameList &frame_list() const { return frame_list_; }

        [[nodiscard]] std::optional<int> frame(const int logical_frame) const;

        void set_timecode(const Timecode &tc);
        [[nodiscard]] const Timecode &timecode() const;
        friend std::string to_string(const MediaReference &value);

        template <class Inspector> friend bool inspect(Inspector &f, MediaReference &x) {
            return f.object(x).fields(
                f.field("uri", x.uri_),
                f.field("cont", x.container_),
                f.field("dur", x.duration_),
                f.field("fl", x.frame_list_),
                f.field("tc", x.timecode_),
                f.field("off", x.offset_));
        }

        bool operator==(const MediaReference &other) const {
            return uri_ == other.uri_ and container_ == other.container_ and
                   duration_ == other.duration_ and frame_list_ == other.frame_list_ and
                   timecode_ == other.timecode_ and offset_ == other.offset_;
        }

        bool operator!=(const MediaReference &other) const {
            return not(
                uri_ == other.uri_ and container_ == other.container_ and
                duration_ == other.duration_ and frame_list_ == other.frame_list_ and
                timecode_ == other.timecode_ and offset_ == other.offset_);
        }

      private:
        caf::uri uri_;
        bool container_;
        FrameRateDuration duration_;
        FrameList frame_list_;
        Timecode timecode_;

        int offset_{0};
    };
    inline std::string to_string(const MediaReference &v) {
        return to_string(v.uri_) + " " + to_string(v.duration_) + " " +
               std::to_string(v.container());
    }
} // namespace utility
} // namespace xstudio