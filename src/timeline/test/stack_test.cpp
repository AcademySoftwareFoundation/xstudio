// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/media/media_actor.hpp"
#include "xstudio/timeline/stack.hpp"
#include "xstudio/timeline/gap.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::timeline;
using namespace xstudio::media;

TEST(StackTest, Test) {
    Stack s;
    // utility::start_logger();
    auto gd1 = utility::FrameRateDuration(10, timebase::k_flicks_24fps);
    auto g1  = Gap("Gap1", gd1);
    auto gd2 = utility::FrameRateDuration(15, timebase::k_flicks_24fps);
    auto g2  = Gap("Gap2", gd2);
    auto gd3 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto g3  = Gap("Gap3", gd3);

    auto ru1 = s.item().insert(s.item().end(), g1.item());
    auto ru2 = s.item().insert(s.item().end(), g2.item());
    auto ru3 = s.item().insert(s.item().end(), g3.item());

    auto it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    s.item().undo(ru3);
    s.item().undo(ru2);
    s.item().undo(ru1);

    EXPECT_TRUE(s.item().empty());

    s.item().redo(ru1);
    s.item().redo(ru2);
    s.item().redo(ru3);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    auto ru4 = s.item().erase(s.item().begin());
    it       = s.item().begin();
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    EXPECT_EQ(s.item().size(), 2);

    s.item().undo(ru4);
    EXPECT_EQ(s.item().size(), 3);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());


    auto start = std::next(s.item().begin(), 0);
    auto end   = std::next(s.item().begin(), 1);
    auto pos   = std::next(s.item().begin(), 3);

    auto ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    // move / undo
    start = std::next(s.item().begin(), 0);
    end   = std::next(s.item().begin(), 2);
    pos   = std::next(s.item().begin(), 3);

    ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());

    // move / undo
    start = std::next(s.item().begin(), 0);
    end   = std::next(s.item().begin(), 2);
    pos   = std::next(s.item().begin(), 3);

    ru5 = s.item().splice(pos, s.item().children(), start, end);

    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g3.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());

    s.item().undo(ru5);
    it = s.item().begin();
    EXPECT_EQ(it->uuid(), g1.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g2.item().uuid());
    it++;
    EXPECT_EQ(it->uuid(), g3.item().uuid());


    // do nested changes work ?
    auto ru6 = s.item().children().front().set_enabled(false);
    EXPECT_FALSE(s.item().children().front().enabled());
    s.item().undo(ru6);
    EXPECT_TRUE(s.item().children().front().enabled());
    s.item().redo(ru6);
    EXPECT_FALSE(s.item().children().front().enabled());
    s.item().undo(ru6);
    EXPECT_TRUE(s.item().children().front().enabled());

    // check no op
    s.item().set_enabled(true);
    auto ru7 = s.item().set_enabled(true);
    // no op
    s.item().undo(ru7);
    EXPECT_TRUE(s.item().enabled());

    // we should be able to replay events. on cloned trees
    Stack s2(s.serialise());
    s2.item().update(ru6);
    EXPECT_FALSE(s2.item().children().front().enabled());
}

TEST(StackRefreshTest, Test) {
    Stack s;

    // utility::start_logger();
    auto gd1 = utility::FrameRateDuration(10, timebase::k_flicks_24fps);
    auto g1  = Gap("Gap1", gd1);
    auto gd2 = utility::FrameRateDuration(15, timebase::k_flicks_24fps);
    auto g2  = Gap("Gap2", gd2);
    auto gd3 = utility::FrameRateDuration(20, timebase::k_flicks_24fps);
    auto g3  = Gap("Gap3", gd3);

    auto ru1 = s.item().insert(s.item().end(), g1.item());
    auto ru2 = s.item().insert(s.item().end(), g2.item());
    auto ru3 = s.item().insert(s.item().end(), g3.item());

    auto av = s.item().available_range();
    EXPECT_FALSE(av);

    // we've added gaps so we need to refresh our state.
    auto ru4 = s.item().refresh(1);
    // this will modify our availiable range. So we capture that.
    // Or should it be part of the insertion?
    EXPECT_TRUE(s.item().available_range());
    EXPECT_NE(av, s.item().available_range());

    s.item().undo(ru4);
    s.item().undo(ru3);
    s.item().undo(ru2);
    s.item().undo(ru1);

    // we should be back to original state..
    EXPECT_FALSE(s.item().available_range());
}
