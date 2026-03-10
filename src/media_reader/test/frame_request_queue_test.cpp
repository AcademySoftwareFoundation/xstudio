// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include <stdexcept>

#include "xstudio/media_reader/frame_request_queue.hpp"

using namespace xstudio;
using namespace xstudio::media_reader;

namespace {

media::AVFrameID make_frame_id(const std::string &uri, const int frame) {
    auto parsed_uri = caf::make_uri(uri);
    if (!parsed_uri)
        throw std::runtime_error("Failed to parse test uri");
    return media::AVFrameID(*parsed_uri, frame);
}

size_t drain_queue(FrameRequestQueue &queue) {
    size_t count = 0;
    while (queue.pop_request({})) {
        ++count;
    }
    return count;
}

} // namespace

TEST(FrameRequestQueueTest, DeduplicatesSingleFrameAndKeepsEarliestDeadline) {
    FrameRequestQueue queue;
    const auto playhead = utility::Uuid::generate();
    const auto base_tp  = utility::clock::now();

    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0001.exr", 10),
        base_tp + std::chrono::milliseconds(20),
        playhead);
    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0001.exr", 10),
        base_tp + std::chrono::milliseconds(5),
        playhead);

    const auto request = queue.pop_request({});
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->requested_frame_->frame(), 10);
    EXPECT_EQ(request->requesting_playhead_uuid_, playhead);
    EXPECT_EQ(request->required_by_, base_tp + std::chrono::milliseconds(5));
    EXPECT_FALSE(queue.pop_request({}).has_value());
}

TEST(FrameRequestQueueTest, BatchInsertDeduplicatesByMediaKey) {
    FrameRequestQueue queue;
    const auto playhead = utility::Uuid::generate();
    const auto base_tp  = utility::clock::now();

    media::AVFrameIDsAndTimePoints requests;
    requests.emplace_back(
        base_tp + std::chrono::milliseconds(30),
        std::make_shared<const media::AVFrameID>(
            make_frame_id("file:///tmp/test.0001.exr", 1)));
    requests.emplace_back(
        base_tp + std::chrono::milliseconds(5),
        std::make_shared<const media::AVFrameID>(
            make_frame_id("file:///tmp/test.0001.exr", 1)));
    requests.emplace_back(
        base_tp + std::chrono::milliseconds(10),
        std::make_shared<const media::AVFrameID>(
            make_frame_id("file:///tmp/test.0002.exr", 2)));

    queue.add_frame_requests(requests, playhead);

    const auto first = queue.pop_request({});
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->requested_frame_->frame(), 1);
    EXPECT_EQ(first->required_by_, base_tp + std::chrono::milliseconds(5));

    const auto second = queue.pop_request({});
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->requested_frame_->frame(), 2);
    EXPECT_FALSE(queue.pop_request({}).has_value());
}

TEST(FrameRequestQueueTest, PopRequestSkipsExcludedPlayheads) {
    FrameRequestQueue queue;
    const auto excluded_playhead = utility::Uuid::generate();
    const auto included_playhead = utility::Uuid::generate();
    const auto base_tp           = utility::clock::now();

    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0001.exr", 1),
        base_tp + std::chrono::milliseconds(5),
        excluded_playhead);
    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0002.exr", 2),
        base_tp + std::chrono::milliseconds(10),
        included_playhead);

    const auto request = queue.pop_request({{excluded_playhead, 1}});
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->requested_frame_->frame(), 2);
    EXPECT_EQ(request->requesting_playhead_uuid_, included_playhead);
}

TEST(FrameRequestQueueTest, ClearPendingRequestsRemovesOnlyMatchingPlayhead) {
    FrameRequestQueue queue;
    const auto removed_playhead = utility::Uuid::generate();
    const auto kept_playhead    = utility::Uuid::generate();
    const auto base_tp          = utility::clock::now();

    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0001.exr", 1),
        base_tp + std::chrono::milliseconds(5),
        removed_playhead);
    queue.add_frame_request(
        make_frame_id("file:///tmp/test.0002.exr", 2),
        base_tp + std::chrono::milliseconds(10),
        kept_playhead);

    queue.clear_pending_requests(removed_playhead);

    const auto request = queue.pop_request({});
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->requesting_playhead_uuid_, kept_playhead);
    EXPECT_FALSE(queue.pop_request({}).has_value());
}

TEST(FrameRequestQueueTest, PruneStaleRequestsKeepsLatestStaleRequest) {
    FrameRequestQueue queue;
    const auto playhead = utility::Uuid::generate();
    const auto now      = utility::clock::now();

    for (int frame = 0; frame < 24; ++frame) {
        queue.add_frame_request(
            make_frame_id("file:///tmp/test." + std::to_string(frame) + ".exr", frame),
            now - std::chrono::milliseconds(100 - frame),
            playhead);
    }

    queue.prune_stale_frame_requests();

    ASSERT_EQ(drain_queue(queue), 1U);
}
