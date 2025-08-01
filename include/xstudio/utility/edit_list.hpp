// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/edit_section.hpp"

namespace xstudio {
namespace utility {

    // container for handling clip time/frames positions
    class EditList {
      public:
        EditList(ClipList sl = ClipList()) : sl_(std::move(sl)) {}
        EditList(const EditList &) = default;
        EditList(EditList &&)      = default;
        virtual ~EditList()        = default;

        EditList &operator=(const EditList &) = default;
        EditList &operator=(EditList &&)      = default;

        void extend(const EditList &o);

        void append(const EditListSection &s) { sl_.push_back(s); }

        [[nodiscard]] bool empty() const { return sl_.empty(); }
        [[nodiscard]] size_t size() const { return sl_.size(); }
        void clear() { sl_.clear(); }
        [[nodiscard]] const ClipList &section_list() const { return sl_; }

        [[nodiscard]] size_t duration_frames(
            const TimeSourceMode tsm = TimeSourceMode::FIXED,
            const FrameRate &fr      = FrameRate()) const;
        [[nodiscard]] double duration_seconds(
            const TimeSourceMode tsm = TimeSourceMode::FIXED,
            const FrameRate &fr      = FrameRate()) const;
        [[nodiscard]] timebase::flicks duration_flicks(
            const TimeSourceMode tsm = TimeSourceMode::FIXED,
            const FrameRate &fr      = FrameRate()) const;

        [[nodiscard]] std::vector<std::pair<Uuid, size_t>>
        frame_durations(const TimeSourceMode tsm, const FrameRate &fr = FrameRate()) const;
        [[nodiscard]] std::vector<std::pair<Uuid, double>>
        second_durations(const TimeSourceMode tsm, const FrameRate &fr = FrameRate()) const;
        [[nodiscard]] std::vector<std::pair<Uuid, timebase::flicks>>
        flick_durations(const TimeSourceMode tsm, const FrameRate &fr = FrameRate()) const;

        [[nodiscard]] timebase::flicks flicks_from_logical(
            const int logical_frame,
            const TimeSourceMode tsm,
            const FrameRate &fr = FrameRate()) const;

        EditListSection media_frame(const int logical_frame, int &media_frame) const;

        [[nodiscard]] timebase::flicks media_frame_to_flicks(
            const utility::Uuid &media_source,
            const int logical_frame,
            const TimeSourceMode tsm,
            const FrameRate &rate) const;

        [[nodiscard]] timebase::flicks media_flick_to_flicks(
            const utility::Uuid &media_source,
            const timebase::flicks media_flick,
            const TimeSourceMode tsm,
            const FrameRate &rate) const;

        [[nodiscard]] int logical_frame(
            const TimeSourceMode tsm,
            const FrameRate &rate,
            const FrameRateDuration &frame) const;

        [[nodiscard]] int logical_frame(
            const TimeSourceMode tsm,
            const timebase::flicks target_flicks,
            const utility::FrameRate &rate = utility::FrameRate(1.0 / 24.0)) const;

        [[nodiscard]] EditListSection skip_sections(int ref_frame, int skip_by) const;

        [[nodiscard]] timebase::flicks flicks_from_frame(
            const TimeSourceMode tsm,
            const int frame,
            const utility::FrameRate &rate = utility::FrameRate(1.0 / 24.0)) const;

        [[nodiscard]] FrameRate frame_rate_at_frame(const int logical_frame) const;

        int step(
            const TimeSourceMode tsm,
            const FrameRate &rate,
            const bool forward,
            const float velocity,
            const FrameRateDuration &frame,
            FrameRateDuration &new_frame,
            timebase::flicks &new_seconds) const;

        int step(
            const TimeSourceMode tsm,
            const FrameRate &override_rate,
            const bool forward,
            const float velocity,
            const int logical_frame,
            const int in_frame,
            const int out_frame,
            timebase::flicks &step_period) const;

        [[nodiscard]] std::pair<int, int> flicks_range_to_logical_frame_range(
            const timebase::flicks &in,
            const timebase::flicks &out,
            const TimeSourceMode tsm,
            const FrameRate &rate) const;

        [[nodiscard]] EditListSection next_section(
            const timebase::flicks &ref_timepoint,
            const int skip_by,
            const TimeSourceMode tsm,
            const FrameRate &rate) const;

        timebase::flicks section_start_time(
            const utility::Uuid &media_uuid,
            const TimeSourceMode tsm,
            const FrameRate &override_rate,
            FrameRate &out_rate) const;

        void set_uuid(const Uuid &uuid);

        void push_back(const EditListSection &s) { sl_.push_back(s); }

        bool operator==(const EditList &other) const {
            if (sl_.size() != other.sl_.size())
                return false;
            for (size_t i = 0; i < sl_.size(); i++) {
                if (not(sl_[i] == other.sl_[i]))
                    return false;
            }

            return true;
        }

        template <class Inspector> friend bool inspect(Inspector &f, EditList &x) {
            return f.object(x).fields(f.field("sl", x.sl_));
        }

      private:
        ClipList sl_;
    };
} // namespace utility
} // namespace xstudio
