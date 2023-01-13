// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/frame_rate_and_duration.hpp"

namespace xstudio {
namespace utility {

    struct EditListSection {
      public:
        EditListSection()                         = default;
        EditListSection(const EditListSection &o) = default;
        EditListSection(Uuid uuid, FrameRateDuration dur, Timecode tc)
            : media_uuid_(std::move(uuid)),
              frame_rate_and_duration_(std::move(dur)),
              timecode_(std::move(tc)) {}

        EditListSection &operator=(const EditListSection &o) = default;

        bool operator==(const EditListSection &o) const {
            return media_uuid_ == o.media_uuid_ &&
                   frame_rate_and_duration_ == o.frame_rate_and_duration_ &&
                   timecode_ == o.timecode_;
        }

        template <class Inspector> friend bool inspect(Inspector &f, EditListSection &x) {
            return f.object(x).fields(
                f.field("source_uuid", x.media_uuid_),
                f.field("frame_rate_and_duration", x.frame_rate_and_duration_),
                f.field("timecode", x.timecode_));
        }

        Uuid media_uuid_;
        FrameRateDuration frame_rate_and_duration_;
        Timecode timecode_;
    };

    using ClipList = std::vector<EditListSection>;
} // namespace utility
} // namespace xstudio
