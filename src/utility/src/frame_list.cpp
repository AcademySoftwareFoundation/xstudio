// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <limits>
#include <regex>

#include <fmt/format.h>

#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/string_helpers.hpp"

namespace fs = std::filesystem;


using namespace xstudio::utility;

FrameGroup::FrameGroup(const int start, const int end, const int step)
    : start_(start), end_(end), step_(step) {}

FrameGroup::FrameGroup(const std::string &frm_string) {
    const std::regex range_spec(R"((-?\d+)(-(-?\d+)(x(\d+))?)?)", std::regex::optimize);
    std::cmatch m;
    if (std::regex_match(frm_string.c_str(), m, range_spec)) {
        start_ = end_ = std::atoi(m[1].str().c_str());
        if (not m[3].str().empty())
            end_ = std::atoi(m[3].str().c_str());
        if (not m[5].str().empty())
            step_ = std::atoi(m[5].str().c_str());
    } else
        throw std::runtime_error("Invalid FrameGroup");
}


int FrameGroup::end(const bool implied) const {
    if (implied)
        return end_;

    return end_ - ((end_ - start_) % step_);
}

size_t FrameGroup::count(const bool implied) const {
    if (implied)
        return (end_ - start_) + 1;
    return ((end_ - start_) / step_) + 1;
}

int FrameGroup::frame(const size_t index, const bool implied, const bool valid) const {
    int _frame;
    if (implied) {
        if (valid) {
            // find previous valid frame.
            _frame = ((index / step_) * step_) + start_;
        } else
            _frame = start_ + index;
    } else {
        _frame = (index * step_) + start_;
    }

    if (_frame > end(implied))
        throw std::runtime_error("Invalid index");

    return _frame;
}

std::vector<int> FrameGroup::frames() const {
    std::vector<int> frms;
    frms.reserve(count(false));
    for (size_t i = 0; i < count(false); i++)
        frms.push_back(frame(i, false, false));
    return frms;
}

void xstudio::utility::to_json(nlohmann::json &j, const FrameGroup &fg) { j = to_string(fg); }

void xstudio::utility::from_json(const nlohmann::json &j, FrameGroup &fg) {
    fg = FrameGroup(j.get<std::string>());
}

std::string xstudio::utility::to_string(const FrameGroup &frame_group) {
    if (frame_group.start_ == frame_group.end_)
        return fmt::format("{0}", frame_group.start_);

    else if (frame_group.step_ == 1)
        return fmt::format("{0}-{1}", frame_group.start_, frame_group.end_);

    return fmt::format("{0}-{1}x{2}", frame_group.start_, frame_group.end_, frame_group.step_);
}

// scan spec path to build frame list
// return empty list if duff..
// assumes native xstudio format spec..
FrameList::FrameList(const caf::uri &from_path)
    : frame_groups_(frame_groups_from_sequence_spec(from_path)) {}

FrameList::FrameList(const int start, const int end, const int step)
    : frame_groups_({FrameGroup(start, end, step)}) {}

FrameList::FrameList(const std::string &from_string) {
    for (const auto &i : split(from_string, ',')) {
        frame_groups_.emplace_back(FrameGroup(i));
    }
}

FrameList::FrameList(std::vector<FrameGroup> frame_groups)
    : frame_groups_(std::move(frame_groups)) {}

size_t FrameList::count(const bool implied) const {
    size_t count = 0;

    if (implied) {
        count = (end(implied) - start()) + 1;
    } else {
        for (const auto &i : frame_groups_)
            count += i.count();
    }

    return count;
}

int FrameList::start() const {
    int _start = std::numeric_limits<int>::max();

    for (const auto &i : frame_groups_) {
        if (i.start() < _start)
            _start = i.start();
    }
    return _start;
}

int FrameList::end(const bool implied) const {
    int _end = std::numeric_limits<int>::min();

    for (const auto &i : frame_groups_) {
        if (i.end(implied) > _end)
            _end = i.end(implied);
    }
    return _end;
}

int FrameList::frame(const size_t index, const bool implied, const bool valid) const {
    if (frame_groups_.empty())
        throw std::runtime_error("No frames");

    // assumes correct ordering..

    if (implied) {
        if (valid) {
            size_t tmp = index;
            for (auto i = std::begin(frame_groups_); i != std::end(frame_groups_); ++i) {
                if (i->count(true) > tmp) {
                    return i->frame(tmp, true, true);
                } else {
                    auto ii = i + 1;

                    if (ii != std::end(frame_groups_)) {
                        if (static_cast<int>(tmp) + i->start() < ii->start())
                            return i->end();

                        tmp -= (ii->start() - i->end(true)) + 1;
                    } else
                        tmp -= i->count(true);
                }
            }
        } else
            return index + start();
    } else {
        size_t tmp = index;
        for (const auto &i : frame_groups_) {
            if (i.count() > tmp) {
                return i.frame(tmp);
            } else {
                tmp -= i.count();
            }
        }
    }

    throw std::runtime_error("Invalid index");
}

std::vector<int> FrameList::frames() const {
    std::vector<int> frms;

    for (const auto &i : frame_groups_) {
        const auto ii = i.frames();
        frms.insert(frms.end(), ii.begin(), ii.end());
    }

    return frms;
}

// dirty hack.
bool FrameList::pop_front() {
    if (frame_groups_.empty() or frame_groups_[0].count() == 1)
        return false;

    frame_groups_[0].set_start(frame_groups_[0].start() + 1);

    return true;
}


std::string xstudio::utility::to_string(const FrameList &frame_list) {
    return join_as_string(frame_list.frame_groups(), ",");
}

void xstudio::utility::to_json(nlohmann::json &j, const FrameList &fl) {
    j = fl.frame_groups();
}

void xstudio::utility::from_json(const nlohmann::json &j, FrameList &fl) {
    std::vector<FrameGroup> groups;
    j.get_to(groups);
    fl.set_frame_groups(groups);
}

std::vector<FrameGroup>
xstudio::utility::frame_groups_from_frame_set(const std::set<int> &frames) {
    // set should already be ordered..
    std::vector<FrameGroup> fg;

    if (not frames.empty()) {
        long start = 0, end = 0, offset = 0;

        for (const auto &frame : frames) {
            // new range
            if (!offset) {
                start  = frame;
                end    = frame;
                offset = 1;
            } else {
                // simple, one one element in current frag.
                if (start == end) {
                    end    = frame;
                    offset = end - start;
                    // easy we`re part of the current sequence
                } else if ((end + offset) == frame) {
                    end = frame;
                } else {
                    // new frag..
                    fg.emplace_back(FrameGroup(start, end, offset));
                    start  = frame;
                    end    = frame;
                    offset = 1;
                }
            }
        }

        fg.emplace_back(FrameGroup(start, end, offset));
    }

    return fg;
}

std::vector<FrameGroup>
xstudio::utility::frame_groups_from_sequence_spec(const caf::uri &from_path) {
    std::vector<FrameGroup> fg;

    try {
        std::string path = uri_to_posix_path(from_path);
        const std::regex spec_re("\\{[^}]+\\}");
        const std::regex path_re("^" + std::regex_replace(path, spec_re, "([0-9-]+)") + "$");
#ifdef _WIN32
        std::smatch m;
#else
        std::cmatch m;
#endif
        std::set<int> frames;

        for (const auto &entry : fs::directory_iterator(fs::path(path).parent_path())) {
            if (not fs::is_regular_file(entry.status())) {
                continue;
            }
#ifdef _WIN32
            auto entryPath = entry.path().string(); // Convert to std::string
#else
            auto entryPath = entry.path().c_str();
#endif
            if (std::regex_match(entryPath, m, path_re)) {
                int frame = std::atoi(m[1].str().c_str());
                if (fmt::format(fmt::runtime(path), frame) == entry.path()) {
                    frames.insert(frame);
                }
            }
        }
        fg = frame_groups_from_frame_set(frames);

    } catch (const std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }

    return fg;
}
