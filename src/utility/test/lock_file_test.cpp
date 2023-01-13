// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"

#include "xstudio/utility/serialise_headers.hpp"

#include "xstudio/utility/lock_file.hpp"

using namespace caf;
using namespace xstudio::utility;


ACTOR_TEST_MINIMAL()

TEST(LockFileTest, Test) {
    start_logger(spdlog::level::warn);
    // EXPECT_THROW(LockFile("wibble"), std::runtime_error) << "Should be exception";


    auto tmpdir = testing::TempDir();

    auto source = tmpdir + "/source.xst";

    std::ofstream myfile;
    myfile.open(source);
    myfile << "File to lock.\n";
    myfile.close();

    auto lock_file = LockFile(source);

    EXPECT_EQ(lock_file.source(), posix_path_to_uri(source));

    // not locked.
    EXPECT_FALSE(lock_file.locked());
    EXPECT_FALSE(lock_file.owned());

    // lock
    EXPECT_TRUE(lock_file.lock());
    EXPECT_TRUE(lock_file.lock());
    EXPECT_TRUE(lock_file.locked());
    EXPECT_TRUE(lock_file.owned());

    // unlock
    EXPECT_TRUE(lock_file.unlock());
    EXPECT_FALSE(lock_file.unlock());
    EXPECT_FALSE(lock_file.locked());
    EXPECT_FALSE(lock_file.owned());

    // lock
    EXPECT_TRUE(lock_file.lock());

    // same owner
    auto try_lock_file = LockFile(source);
    EXPECT_TRUE(try_lock_file.locked());
    EXPECT_TRUE(try_lock_file.owned());
    EXPECT_FALSE(try_lock_file.unlock());
    EXPECT_TRUE(try_lock_file.lock());

    unlink(source.c_str());
}