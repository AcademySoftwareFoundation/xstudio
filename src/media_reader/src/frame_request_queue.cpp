// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/frame_request_queue.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::media_reader;
using namespace xstudio;

void FrameRequestQueue::add_frame_request(
    const media::AVFrameID &frame_info,
    const utility::time_point &required_by,
    const utility::Uuid &requesting_playhead_uuid) {

    // auto tt = utility::clock::now();
    // spdlog::warn("{}",to_string(frame_info.uri_));

    bool matches_existing_request = false;
    for (auto pp = queue_.rbegin(); pp != queue_.rend(); ++pp) {

        if ((*pp)->requested_frame_->frame() == frame_info.frame() &&
            (*pp)->requested_frame_->uri() == frame_info.uri()) {
            if ((*pp)->required_by_ > required_by) {
                (*pp)->required_by_ = required_by;
            }
            matches_existing_request = true;
            break;
        }
    }

    if (!matches_existing_request) {

        queue_.emplace_back(new FrameRequest(
            std::shared_ptr<const media::AVFrameID>(new media::AVFrameID(frame_info)),
            required_by,
            requesting_playhead_uuid));
    }

    std::sort(
        queue_.begin(),
        queue_.end(),
        [](const std::shared_ptr<FrameRequest> &a, const std::shared_ptr<FrameRequest> &b)
            -> bool { return a->required_by_ < b->required_by_; });
}

void FrameRequestQueue::add_frame_requests(
    const media::AVFrameIDsAndTimePoints &frames_info,
    const utility::Uuid &requesting_playhead_uuid) {

    for (const auto &p : frames_info) {
        const std::shared_ptr<const media::AVFrameID> &frame_info = (p.second);
        const utility::time_point &when_we_want_it                = p.first;
        queue_.emplace_back(
            new FrameRequest(frame_info, when_we_want_it, requesting_playhead_uuid));
    }

    std::sort(
        queue_.begin(),
        queue_.end(),
        [](const std::shared_ptr<FrameRequest> &a, const std::shared_ptr<FrameRequest> &b)
            -> bool { return a->required_by_ < b->required_by_; });
}

std::optional<FrameRequest>
FrameRequestQueue::pop_request(const std::map<utility::Uuid, int> &exclude_playheads) {
    std::optional<FrameRequest> rt = {};

    for (auto p = queue_.begin(); p != queue_.end(); p++) {
        if (!exclude_playheads.count((*p)->requesting_playhead_uuid_)) {
            rt = *(*p);
            queue_.erase(p);
            break;
        }
    }
    return rt;
}

void FrameRequestQueue::prune_stale_frame_requests() {

    auto now = utility::clock::now();
    // bool found_stale_request = false;
    for (auto pp = queue_.begin(); pp != queue_.end(); ++pp) {
        if (queue_.size() > 20) { //&& (*pp).required_by < now) {
            auto ppp = pp;
            ppp++;
            if (ppp != queue_.end() && (*ppp)->required_by_ < now) {
                pp = queue_.erase(pp);
            }
        }
    }
}

void FrameRequestQueue::clear_pending_requests(const utility::Uuid &playhead_uuid) {

    queue_.erase(
        std::remove_if(
            queue_.begin(),
            queue_.end(),
            [&playhead_uuid](const std::shared_ptr<FrameRequest> &x) {
                return x->requesting_playhead_uuid_ == playhead_uuid; // put your condition here
            }),
        queue_.end());

    /*auto pp = queue_.begin();
    auto a = queue_.end();
    auto b = queue_.end();

    while (pp != queue_.end()) {
        if ((*pp)->requesting_playhead_uuid_ == playhead_uuid) {
            if (a == queue_.end()) {
                a = pp;
            }
            b = pp;
        } else {
            if (a != queue_.end()) {
                queue_.erase(a, b);
            }
            pp++;
        }
    }*/
}