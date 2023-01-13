// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/bookmark/bookmarks_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace std::chrono_literals;
using namespace xstudio::utility;
using namespace xstudio::playlist;
using namespace xstudio::bookmark;
using namespace xstudio;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(BookmarkActorTest, Test) { fixture f; }

TEST(BookmarkSerializerTest, Test) {
    fixture f;

    binary_serializer::container_type buf;
    binary_serializer bs{f.system, buf};
}
