// SPDX-License-Identifier: Apache-2.0
#include "xstudio/media_reader/frame_request_queue.hpp"
#include "xstudio/utility/helpers.hpp"

#include <vector>

using namespace xstudio::media_reader;
using namespace xstudio;

bool FrameRequestQueue::RequestOrder::operator()(
    const RequestHandle &a, const RequestHandle &b) const {
    if (a->request_.required_by_ != b->request_.required_by_)
        return a->request_.required_by_ < b->request_.required_by_;

    if (a->request_.requesting_playhead_uuid_ != b->request_.requesting_playhead_uuid_)
        return a->request_.requesting_playhead_uuid_ < b->request_.requesting_playhead_uuid_;

    return a->request_.requested_frame_->key() < b->request_.requested_frame_->key();
}

FrameRequestQueue::RequestHandle FrameRequestQueue::emplace_request(
    std::shared_ptr<const media::AVFrameID> frame_info,
    const utility::time_point &required_by,
    const utility::Uuid &requesting_playhead_uuid) {

    auto inserted = std::make_shared<RequestEntry>(FrameRequest(
        std::move(frame_info), required_by, requesting_playhead_uuid));

    inserted->ordered_it_ =
        ordered_requests_.insert(inserted);
    inserted->playhead_it_ =
        requests_by_playhead_.emplace(requesting_playhead_uuid, inserted);
    requests_by_media_key_[inserted->request_.requested_frame_->key()] = inserted;

    return inserted;
}

void FrameRequestQueue::erase_request(const RequestHandle &request) {
    requests_by_media_key_.erase(request->request_.requested_frame_->key());
    requests_by_playhead_.erase(request->playhead_it_);
    ordered_requests_.erase(request->ordered_it_);
}

void FrameRequestQueue::update_required_by(
    const RequestHandle &request, const utility::time_point &required_by) {
    if (required_by >= request->request_.required_by_)
        return;

    ordered_requests_.erase(request->ordered_it_);
    request->request_.required_by_ = required_by;
    request->ordered_it_           = ordered_requests_.insert(request);
}

void FrameRequestQueue::add_frame_request(
    const media::AVFrameID &frame_info,
    const utility::time_point &required_by,
    const utility::Uuid &requesting_playhead_uuid) {

    const auto existing = requests_by_media_key_.find(frame_info.key());
    if (existing != requests_by_media_key_.end()) {
        update_required_by(existing->second, required_by);
        return;
    }

    emplace_request(
        std::make_shared<const media::AVFrameID>(frame_info),
        required_by,
        requesting_playhead_uuid);
}

void FrameRequestQueue::add_frame_requests(
    const media::AVFrameIDsAndTimePoints &frames_info,
    const utility::Uuid &requesting_playhead_uuid) {

    for (const auto &p : frames_info) {
        const std::shared_ptr<const media::AVFrameID> &frame_info = (p.second);
        const utility::time_point &when_we_want_it                = p.first;

        const auto existing = requests_by_media_key_.find(frame_info->key());
        if (existing != requests_by_media_key_.end()) {
            update_required_by(existing->second, when_we_want_it);
            continue;
        }

        emplace_request(frame_info, when_we_want_it, requesting_playhead_uuid);
    }
}

std::optional<FrameRequest>
FrameRequestQueue::pop_request(const std::map<utility::Uuid, int> &exclude_playheads) {
    for (auto request = ordered_requests_.begin(); request != ordered_requests_.end(); ++request) {
        const auto current = *request;
        if (exclude_playheads.count(current->request_.requesting_playhead_uuid_))
            continue;

        auto result = current->request_;
        erase_request(current);
        return result;
    }

    return {};
}

void FrameRequestQueue::prune_stale_frame_requests() {
    if (ordered_requests_.size() <= 20)
        return;

    auto now = utility::clock::now();
    auto request = ordered_requests_.begin();
    while (request != ordered_requests_.end()) {
        auto next = std::next(request);
        if (next == ordered_requests_.end())
            break;

        if ((*next)->request_.required_by_ >= now)
            break;

        auto current = *request;
        request      = next;
        erase_request(current);
    }
}

void FrameRequestQueue::clear_pending_requests(const utility::Uuid &playhead_uuid) {
    auto [begin, end] = requests_by_playhead_.equal_range(playhead_uuid);
    std::vector<RequestHandle> to_remove;
    to_remove.reserve(std::distance(begin, end));

    for (auto it = begin; it != end; ++it) {
        to_remove.emplace_back(it->second);
    }

    for (const auto &request : to_remove) {
        erase_request(request);
    }
}
