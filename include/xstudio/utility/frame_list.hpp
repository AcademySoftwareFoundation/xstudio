// SPDX-License-Identifier: Apache-2.0
// container to handle sequences/mov files etc..
#pragma once

#include <limits>
#include <set>

#include <caf/type_id.hpp>
#include <caf/uri.hpp>
#include <nlohmann/json.hpp>

namespace xstudio {
namespace utility {
    class FrameList;
    class FrameGroup {
        friend class FrameList;

      public:
        FrameGroup() = default;
        FrameGroup(const std::string &from_string);
        FrameGroup(const int start, const int end, const int step = 1);
        virtual ~FrameGroup() = default;

        void set_start(const int start) { start_ = start; }
        void set_end(const int end) { end_ = end; }
        void set_step(const int step) { step_ = step; }

        [[nodiscard]] int start() const { return start_; }
        [[nodiscard]] int end(const bool implied = false) const;
        [[nodiscard]] int step() const { return step_; }

        [[nodiscard]] size_t count(const bool implied = false) const;

        [[nodiscard]] int
        frame(const size_t index, const bool implied = false, const bool valid = false) const;
        [[nodiscard]] std::vector<int> frames() const;

        bool operator==(const FrameGroup &other) const {
            return start_ == other.start_ and end_ == other.end_ and step_ == other.step_;
        }
        friend std::string to_string(const FrameGroup &frame_group);

        template <class Inspector> friend bool inspect(Inspector &f, FrameGroup &x) {
            return f.object(x).fields(
                f.field("start", x.start_), f.field("end", x.end_), f.field("step", x.step_));
        }

      private:
        int start_{0};
        int end_{0}; // exclusive ?
        int step_{1};
    };

    std::string to_string(const FrameGroup &frame_group);

    void to_json(nlohmann::json &j, const FrameGroup &fg);
    void from_json(const nlohmann::json &j, FrameGroup &fg);

    class FrameList {
      public:
        FrameList() = default;
        FrameList(const caf::uri &from_path);
        FrameList(const std::string &from_string);
        FrameList(const int start, const int end, const int step = 1);
        FrameList(std::vector<FrameGroup> frame_groups);
        virtual ~FrameList() = default;

        bool pop_front();

        [[nodiscard]] const std::vector<FrameGroup> &frame_groups() const {
            return frame_groups_;
        }

        std::vector<FrameGroup> &frame_groups() { return frame_groups_; }

        void set_frame_groups(const std::vector<FrameGroup> &frame_groups) {
            frame_groups_ = frame_groups;
        }

        [[nodiscard]] int start() const;
        [[nodiscard]] int end(const bool implied = false) const;
        [[nodiscard]] size_t count(const bool implied = false) const;
        [[nodiscard]] int
        frame(const size_t index, const bool implied = false, const bool valid = false) const;
        [[nodiscard]] bool empty() const { return frame_groups_.empty(); }
        [[nodiscard]] size_t size() const { return frame_groups_.size(); }
        void clear() { frame_groups_.clear(); }

        [[nodiscard]] std::vector<int> frames() const;

        template <class Inspector> friend bool inspect(Inspector &f, FrameList &x) {
            return f.object(x).fields(f.field("groups", x.frame_groups_));
        }
        bool operator==(const FrameList &other) const {
            if (frame_groups_.size() != other.size())
                return false;

            for (size_t i = 0; i < frame_groups_.size(); i++) {
                if (not(frame_groups_[i] == other.frame_groups_[i]))
                    return false;
            }

            return true;
        }
        bool operator!=(const FrameList &other) const { return not(*this == other); }

      private:
        std::vector<FrameGroup> frame_groups_;
    };

    extern std::vector<FrameGroup> frame_groups_from_sequence_spec(const caf::uri &from_path);
    extern std::vector<FrameGroup> frame_groups_from_frame_set(const std::set<int> &frames);

    std::string to_string(const FrameList &frame_list);

    void to_json(nlohmann::json &j, const FrameList &fl);
    void from_json(const nlohmann::json &j, FrameList &fl);
} // namespace utility
} // namespace xstudio