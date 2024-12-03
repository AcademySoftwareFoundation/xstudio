// SPDX-License-Identifier: Apache-2.0
#include <regex>

#include <fmt/format.h>

#include <caf/expected.hpp>
#include <caf/uri.hpp>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"

using namespace fmt::literals;

namespace xstudio::utility {

MediaReference::MediaReference(caf::uri uri, const bool container, const FrameRate &rate)
    : uri_(std::move(uri)), container_(container), timecode_("00:00:00:00") {

    if (not container_) {
        // try and extract range from uri ?
        const std::regex range_spec(R"(^.+?([,-\d]+)\{:.*$)", std::regex::optimize);
        std::cmatch m;
        std::string path = uri_to_posix_path(uri);

        if (std::regex_match(path.c_str(), m, range_spec) and not m[1].str().empty()) {
            path        = std::regex_replace(path, std::regex(m[1].str()), "");
            uri_        = posix_path_to_uri(path);
            frame_list_ = FrameList(m[1].str());
        } else {
            // single frame not sequence
            frame_list_ = FrameList("0");
        }
    }

    duration_ = FrameRateDuration(static_cast<int>(frame_list_.count()), rate);
}

MediaReference::MediaReference(
    caf::uri uri, std::string frame_list, const bool container, const FrameRate &rate)
    : uri_(std::move(uri)),
      container_(container),
      frame_list_(std::move(frame_list)),
      timecode_("00:00:00:00") {
    duration_ = FrameRateDuration(static_cast<int>(frame_list_.count()), rate);
}

MediaReference::MediaReference(
    caf::uri uri, FrameList frame_list, const bool container, const FrameRate &rate)
    : uri_(std::move(uri)),
      container_(container),
      frame_list_(std::move(frame_list)),
      timecode_("00:00:00:00") {
    duration_ = FrameRateDuration(static_cast<int>(frame_list_.count()), rate);
}


MediaReference::MediaReference(const JsonStore &jsn)
    : container_(jsn.at("container")), duration_(jsn.at("duration")) {

    auto uri = caf::make_uri(jsn.at("uri"));
    if (uri)
        uri_ = *uri;
    else {
        // try and fix bad URI first..
        // spdlog::warn("orig {}",jsn.at("uri").get<std::string>());
        // spdlog::warn("decode {}",uri_decode(jsn.at("uri")));
        // spdlog::warn("encode {}",uri_encode(uri_decode(jsn.at("uri"))));

        uri = caf::make_uri(uri_encode(uri_decode(jsn.at("uri"))));
        if (uri)
            uri_ = *uri;
        else
            throw XStudioError("Invalid URI " + jsn.at("uri").get<std::string>());
    }

    frame_list_ = jsn.at("frame_list");
    timecode_   = jsn.at("timecode");
    offset_     = jsn.value("offset", 0);
}

JsonStore MediaReference::serialise() const {
    JsonStore jsn;

    jsn["uri"]        = to_string(uri_);
    jsn["container"]  = container_;
    jsn["duration"]   = duration_;
    jsn["frame_list"] = static_cast<nlohmann::json>(frame_list_);
    jsn["timecode"]   = static_cast<nlohmann::json>(timecode_);
    jsn["offset"]     = offset_;

    return jsn;
}

void MediaReference::set_timecode_from_frames() {
    auto tmp = frame(0);
    if (tmp)
        timecode_ = Timecode((*tmp), duration_.rate().to_fps());
}

void MediaReference::set_offset(const int offset) {
    offset_ = offset;
    // offset timecode..
    timecode_ = timecode_ + offset_;
}

void MediaReference::set_frame_list(const FrameList &fl) {
    frame_list_ = fl;
    duration_.set_frames(frame_list_.count());
}

void MediaReference::set_rate(const FrameRate &rate) { duration_.set_rate(rate, true); }

caf::uri MediaReference::uri(const FramePadFormat fpf) const {
    auto result = uri_;

    if (not container_) {
        switch (fpf) {
        case FramePadFormat::FPF_SHAKE:
            // replace formet string with correct number of #/@
            {
                auto str = uri_decode(to_string(uri_));
                std::cmatch m;
                if (std::regex_match(str.c_str(), m, std::regex(".*\\{:(\\d+)d\\}.*"))) {
                    auto repstr = std::string(std::stoi(m[1].str()), '#');

                    result = *caf::make_uri(uri_encode(std::regex_replace(
                        str,
                        std::regex("(\\{:\\d+d\\})"),
                        repstr,
                        std::regex_constants::format_first_only)));
                }
            }
            break;

        case FramePadFormat::FPF_NUKE:
            // replace format string with %04d etc.
            // file://localhost//tmp/test/test.{:04d}.exr

            {
                result = *caf::make_uri(uri_encode(std::regex_replace(
                    uri_decode(to_string(uri_)),
                    std::regex("\\{:(\\d+d)\\}"),
                    "%$1",
                    std::regex_constants::format_first_only)));
            }
            break;

        case FramePadFormat::FPF_XSTUDIO:
        default:
            break;
        }
    }

    return result;
}

// EXPECT_EQ(mr2.uri(), posix_path_to_uri("/tmp/test/test.{:03d}.exr"));
// EXPECT_EQ(mr2.uri(MediaReference::FramePadFormat::FPF_NUKE),
// posix_path_to_uri("/tmp/test/test.%03d.exr"));


void MediaReference::set_uri(const caf::uri &uri) { uri_ = uri; }

std::vector<std::pair<caf::uri, int>> MediaReference::uris() const {
    std::vector<std::pair<caf::uri, int>> frames;
    int frame;
    for (auto i = 0; i < duration_.frames(); i++) {
        if (not container_) {
            auto _uri = uri(i, frame);
            if (_uri)
                frames.emplace_back(std::pair(*_uri, frame));
            else {
                spdlog::warn("{} Invalid uri {}", __PRETTY_FUNCTION__, to_string(uri_), i);
            }
        } else {
            frames.emplace_back(std::pair(uri_, i));
        }
    }

    return frames;
}

std::optional<int> MediaReference::frame(const int logical_frame) const {
    if (frame_list_.empty())
        return 0;

    try {
        return frame_list_.frame(logical_frame + offset_);
    } catch (...) {
    }
    return {};
}


std::optional<caf::uri> MediaReference::uri_from_frame(const int sequence_frame) const {
    if (container_)
        return uri_;

    auto _uri = caf::make_uri(
        uri_encode(fmt::format(fmt::runtime(uri_decode(to_string(uri_))), sequence_frame)));
    if (_uri)
        return *_uri;

    return {};
}

void MediaReference::set_timecode(const Timecode &tc) { timecode_ = tc; }

const Timecode &MediaReference::timecode() const { return timecode_; }

std::optional<caf::uri> MediaReference::uri(const int logical_frame, int &file_frame) const {
    // throw if out of range ?

    auto mapped_frame = frame(logical_frame);
    if (mapped_frame) {
        file_frame = *mapped_frame;
        auto uri   = uri_from_frame(file_frame);
        if (uri)
            return *uri;
    }

    return {};
}
} // namespace xstudio::utility
