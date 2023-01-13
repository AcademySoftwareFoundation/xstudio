// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/mru_cache.hpp"

using namespace xstudio::utility;

TEST(MRUCacheTest, Test) {
    MRUCache<std::string, std::string> mc;
    EXPECT_EQ(mc.size(), unsigned(0));
    EXPECT_EQ(mc.count(), unsigned(0));

    mc.store("test", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(1));
    EXPECT_EQ(mc.size(), unsigned(7));

    mc.store("test", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(1));
    EXPECT_EQ(mc.size(), unsigned(7));

    mc.erase("test");
    EXPECT_EQ(mc.count(), unsigned(0));
    EXPECT_EQ(mc.size(), unsigned(0));

    mc.store("test", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(1));

    mc.erase("test");
    EXPECT_EQ(mc.count(), unsigned(0));

    mc.store("test", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(1));
    mc.clear();
    EXPECT_EQ(mc.count(), unsigned(0));

    std::shared_ptr<std::string> ptr = std::make_shared<std::string>("testing");
    ptr.reset();
    mc.store("test", ptr);
    EXPECT_EQ(mc.count(), unsigned(1));
    EXPECT_EQ(mc.size(), unsigned(0));
    mc.clear();

    mc.set_max_count(2);
    mc.store("test2", std::make_shared<std::string>("testing"));

    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));

    mc.store("test3", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(2));

    ptr = mc.retrieve("test1");
    EXPECT_TRUE(ptr);
    ptr = mc.retrieve("test3");
    EXPECT_TRUE(ptr);
    ptr = mc.retrieve("test2");
    EXPECT_FALSE(ptr);

    mc.release();
    mc.release();
    EXPECT_EQ(mc.count(), unsigned(0));
    EXPECT_EQ(mc.size(), unsigned(0));

    mc.clear();
    mc.set_max_count(2);
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    mc.set_max_count(1);
    EXPECT_EQ(mc.count(), unsigned(1));

    mc.set_max_count(2);
    mc.clear();
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    mc.preserve("test2");
    mc.set_max_count(1);
    ptr = mc.retrieve("test2");
    EXPECT_TRUE(ptr);

    mc.set_max_count(2);
    mc.clear();
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    mc.set_max_count(1);
    ptr = mc.retrieve("test1");
    EXPECT_TRUE(ptr);
}
