// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/history/history_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::history;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"
#include "xstudio/history/history_actor.hpp"


ACTOR_TEST_SETUP()

TEST(HistoryActorTest, Test) {
    fixture f;
    // start_logger();
    auto h = f.self->spawn<HistoryActor<int>>();

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, plugin_manager::enable_atom_v));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 0);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 1));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 1);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, utility::clear_atom_v));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 0);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 1));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 2));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 3));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 4));

    EXPECT_EQ(request_receive<int>(*(f.self), h, history::undo_atom_v), 4);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::undo_atom_v), 3);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::redo_atom_v), 3);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::redo_atom_v), 4);
}

TEST(HistoryMapActorTest, Test) {
    fixture f;
    // start_logger();
    auto h = f.self->spawn<HistoryMapActor<int, int>>();

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, plugin_manager::enable_atom_v));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 0);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 1, 1));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 1);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, utility::clear_atom_v));

    EXPECT_EQ(request_receive<int>(*(f.self), h, media_cache::count_atom_v), 0);

    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 1, 1));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 2, 2));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 3, 3));
    EXPECT_TRUE(request_receive<bool>(*(f.self), h, history::log_atom_v, 4, 4));

    EXPECT_EQ(request_receive<int>(*(f.self), h, history::undo_atom_v), 4);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::undo_atom_v), 3);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::redo_atom_v), 3);
    EXPECT_EQ(request_receive<int>(*(f.self), h, history::redo_atom_v), 4);
}
