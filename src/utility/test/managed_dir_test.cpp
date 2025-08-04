#include <gtest/gtest.h>

#include <chrono>
#include "xstudio/utility/managed_dir.hpp"
#include "xstudio/utility/chrono.hpp"

using namespace xstudio;
using namespace xstudio::utility;

TEST(MANAGEDDIR, Test) {

    auto t = ManagedDir();

    EXPECT_EQ(t.max_size(), size_t(0));
    EXPECT_EQ(t.max_count(), size_t(0));
    EXPECT_EQ(t.max_age(), std::chrono::seconds(0));
    EXPECT_EQ(t.current_size(), size_t(0));
    EXPECT_EQ(t.current_count(), size_t(0));
    EXPECT_EQ(t.current_max_age(), fs::file_time_type::max());
    EXPECT_EQ(t.target_string(), "");
    EXPECT_FALSE(t.prune_on_exit());

    auto test_time = std::chrono::seconds(5);

    t = ManagedDir("test", 10, 20, test_time);

    EXPECT_EQ(t.max_size(), size_t(10));
    EXPECT_EQ(t.max_count(), size_t(20));
    EXPECT_EQ(t.max_age(), test_time);
    EXPECT_EQ(t.target_string(), "test");

    t.prune_on_exit(true);
    EXPECT_TRUE(t.prune_on_exit());
    t.prune_on_exit(false);
    EXPECT_FALSE(t.prune_on_exit());


    t = ManagedDir("./test");
    // t.scan();
    // EXPECT_EQ(t.current_count(), size_t(2));
    // EXPECT_EQ(t.current_size(), size_t(12));

    // t.max_age(std::chrono::seconds(0));
    // t.max_count(0);
    // t.max_size(0);

    // t.prune(true);

    // t.max_count(1);
    // t.prune();

    // t.max_count(0);
    // t.max_size(1);
    // t.prune();

    // t.max_count(0);
    // t.max_size(0);

    t.max_age(std::chrono::seconds(5));
    t.prune();
}