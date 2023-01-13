// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/timeline/gap_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(GapActorTest, Test) {
    fixture f;
    // start_logger();
    auto duration1 = FrameRateDuration(10, timebase::k_flicks_24fps);
    auto range1    = FrameRange(duration1);
    auto g = f.self->spawn<GapActor>("Gap1", FrameRateDuration(10, timebase::k_flicks_24fps));

    auto item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_EQ(item.trimmed_frame_duration(), FrameRateDuration(10, timebase::k_flicks_24fps));
    EXPECT_EQ(item.trimmed_range(), range1);

    // test undo/redo
    auto hist1 = request_receive<JsonStore>(*(f.self), g, plugin_manager::enable_atom_v, false);
    item       = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_FALSE(item.enabled());

    request_receive<bool>(*(f.self), g, history::undo_atom_v, hist1);
    item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_TRUE(item.enabled());

    request_receive<bool>(*(f.self), g, history::redo_atom_v, hist1);
    item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_FALSE(item.enabled());

    // change active range.

    auto range2 = FrameRange(FrameRateDuration(20, timebase::k_flicks_24fps));
    auto hist2  = request_receive<JsonStore>(*(f.self), g, active_range_atom_v, range2);
    item        = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_EQ(item.trimmed_range(), range2);

    request_receive<bool>(*(f.self), g, history::undo_atom_v, hist2);
    item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_EQ(item.trimmed_range(), range1);

    request_receive<bool>(*(f.self), g, history::redo_atom_v, hist2);
    item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_EQ(item.trimmed_range(), range2);

    // unroll all changes.
    request_receive<bool>(*(f.self), g, history::undo_atom_v, hist2);
    request_receive<bool>(*(f.self), g, history::undo_atom_v, hist1);
    item = request_receive<Item>(*(f.self), g, item_atom_v);
    EXPECT_EQ(item.trimmed_range(), range1);
    EXPECT_TRUE(item.enabled());

    // test serialise
    auto serialise = request_receive<JsonStore>(*(f.self), g, serialise_atom_v);
    auto gg        = f.self->spawn<GapActor>(serialise);
    EXPECT_EQ(item, request_receive<Item>(*(f.self), gg, item_atom_v));

    f.self->send_exit(g, caf::exit_reason::user_shutdown);
    f.self->send_exit(gg, caf::exit_reason::user_shutdown);
}
