// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/serialise_headers.hpp"

#include "xstudio/utility/file_system_item.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio::utility;

ACTOR_TEST_SETUP()


TEST(FileTest, Test) {
    start_logger();
    FileSystemItem root;
    root.bind_ignore_entry_func(ignore_not_session);

    EXPECT_EQ(root.dump()["type_name"], "ROOT");

    // root.insert(root.end(), FileSystemItem("test", posix_path_to_uri("/user_data/XSTUDIO")));
    // root.scan();

    // EXPECT_EQ(root.dump().dump(2), "ROOT");
}
