// SPDX-License-Identifier: Apache-2.0
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/utility/time_cache.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;
using namespace xstudio::media;

TEST(TimeCacheTest, Test) {
    using namespace std::chrono_literals;
    TimeCache<std::string, std::shared_ptr<std::string>> mc;
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

    Uuid uuid(Uuid::generate());
    mc.store("test", std::make_shared<std::string>("testing"), clock::now(), true, uuid);
    mc.store("test", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(1));
    mc.erase("test", Uuid());
    EXPECT_EQ(mc.count(), unsigned(1));
    mc.erase("test", uuid);
    EXPECT_EQ(mc.count(), unsigned(0));

    mc.store("test", std::make_shared<std::string>("testing"), clock::now(), true, uuid);
    EXPECT_EQ(mc.count(), unsigned(1));
    mc.clear();
    EXPECT_EQ(mc.count(), unsigned(0));

    mc.store("test", std::make_shared<std::string>("testing"), clock::now(), true, uuid);
    mc.store("test2", std::make_shared<std::string>("testing"), clock::now(), true, uuid);
    mc.store("test2", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(2));
    mc.erase(uuid);
    EXPECT_EQ(mc.count(), unsigned(1));
    mc.erase(Uuid());
    EXPECT_EQ(mc.count(), unsigned(0));

    std::shared_ptr<std::string> ptr = std::make_shared<std::string>("testing");
    ptr.reset();
    mc.store("test", ptr);
    EXPECT_EQ(mc.count(), unsigned(1));
    EXPECT_EQ(mc.size(), unsigned(0));
    mc.clear();

    mc.set_max_count(2);
    mc.store(
        "test2",
        std::make_shared<std::string>("testing"),
        clock::now() + std::chrono::microseconds(1));
    mc.store(
        "test2",
        std::make_shared<std::string>("testing"),
        clock::now() + std::chrono::microseconds(1000));
    mc.store(
        "test1",
        std::make_shared<std::string>("testing"),
        clock::now() + std::chrono::microseconds(500));
    mc.store("test3", std::make_shared<std::string>("testing"));
    EXPECT_EQ(mc.count(), unsigned(2));

    ptr = mc.retrieve("test2");
    EXPECT_TRUE(ptr);
    ptr = mc.retrieve("test3");
    EXPECT_TRUE(ptr);
    ptr = mc.retrieve("test1");
    EXPECT_FALSE(ptr);

    mc.release();
    mc.release();
    EXPECT_EQ(mc.count(), unsigned(0));
    EXPECT_EQ(mc.size(), unsigned(0));

    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    EXPECT_TRUE(mc.reuse(4));
    EXPECT_EQ(mc.count(), unsigned(1));
    EXPECT_FALSE(mc.reuse(4));
    EXPECT_EQ(mc.count(), unsigned(1));

    mc.clear();
    mc.set_max_count(20);
    mc.set_max_size(15);
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    EXPECT_FALSE(mc.reuse(8));
    EXPECT_EQ(mc.count(), unsigned(1));

    mc.clear();
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    mc.set_max_count(1);
    EXPECT_EQ(mc.count(), unsigned(1));

    mc.set_max_count(10);
    mc.clear();
    mc.set_max_size(15);
    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));
    EXPECT_FALSE(mc.reuse(14));
    EXPECT_EQ(mc.count(), unsigned(0));

    mc.store("test2", std::make_shared<std::string>("testing"));
    mc.store("test1", std::make_shared<std::string>("testing"));


    mc.set_max_count(5);
    mc.set_max_size(1500000);
    mc.clear();
    auto nowtime = clock::now();

    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(mc.store("test1", std::make_shared<std::string>("testing")));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(mc.store("test2", std::make_shared<std::string>("testing")));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(mc.store("test3", std::make_shared<std::string>("testing")));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(mc.store("test4", std::make_shared<std::string>("testing")));
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(mc.store("test5", std::make_shared<std::string>("testing")));
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(mc.count(), unsigned(5));

    EXPECT_FALSE(mc.store("test6", std::make_shared<std::string>("testing"), nowtime));
    EXPECT_EQ(mc.count(), unsigned(5));
    EXPECT_TRUE(mc.store("test7", std::make_shared<std::string>("testing"), nowtime, true));
    EXPECT_EQ(mc.count(), unsigned(5));
}

TEST(TimeCacheTest2, Test) {
    using namespace std::chrono_literals;
    TimeCache<std::string, std::shared_ptr<std::string>> mc;

    // no limit always true.
    EXPECT_TRUE(mc.store_check("test", 1000));
    mc.set_max_count(1);
    EXPECT_TRUE(mc.store_check("test", 1000));

    auto buffer = std::make_shared<std::string>("testing");

    // empty will work
    EXPECT_TRUE(mc.store("test", buffer));
    // wait for it to expire
    std::this_thread::sleep_for(20ms);


    auto futuretime = clock::now();
    // newer will work
    EXPECT_TRUE(mc.store("testing", buffer, futuretime));
    // duplicate will work
    EXPECT_TRUE(mc.store("testing", buffer, futuretime));

    // old should fail.
    auto wastime = clock::now() - std::chrono::hours(1);
    EXPECT_FALSE(mc.store("test old", buffer, wastime));

    // duplicate
    EXPECT_TRUE(mc.store_check("testing", 1000));
    EXPECT_TRUE(mc.store_check("testing", 1000));

    std::this_thread::sleep_for(20ms);

    // new
    EXPECT_TRUE(mc.store_check("test", 1000));
}


TEST(TimeCacheSpeedTest, Test) {
    using namespace std::chrono_literals;
    TimeCache<int, std::shared_ptr<std::string>> mc;

    spdlog::stopwatch sw;
    start_logger(spdlog::level::info);

    auto buffer     = std::make_shared<std::string>("testing");
    auto futuretime = clock::now();

    sw.reset();
    for (int i = 0; i < 100000; i++)
        mc.store(i, buffer, futuretime);
    spdlog::info("store {:.3} seconds.", sw);

    sw.reset();
    for (int i = 0; i < 100000; i++)
        mc.retrieve(i);
    spdlog::info("retrieve {:.3} seconds.", sw);
}


TEST(TimeCacheSpeedTestmk, Test) {
    using namespace std::chrono_literals;
    TimeCache<MediaKey, std::shared_ptr<std::string>> mc;

    spdlog::stopwatch sw;
    start_logger(spdlog::level::info);

    auto buffer     = std::make_shared<std::string>("testing");
    auto futuretime = clock::now();

    sw.reset();
    for (int i = 0; i < 100000; i++)
        mc.store(MediaKey(std::to_string(i)), buffer, futuretime);
    spdlog::info("store {:.3} seconds.", sw);

    sw.reset();
    for (int i = 0; i < 100000; i++)
        mc.retrieve(MediaKey(std::to_string(i)));
    spdlog::info("retrieve {:.3} seconds.", sw);
}


TEST(TimeCacheSpeedTest2, Test) {
    using namespace std::chrono_literals;
    TimeCache<int, std::shared_ptr<std::string>> mc;

    mc.set_max_count(5000);

    spdlog::stopwatch sw;
    start_logger(spdlog::level::info);

    auto buffer     = std::make_shared<std::string>("testing");
    auto futuretime = clock::now();

    sw.reset();
    for (int i = 0; i < 10000; i++)
        mc.store(i, buffer, futuretime);
    spdlog::info("store {:.3} seconds.", sw);

    sw.reset();
    for (int i = 0; i < 10000; i++)
        mc.retrieve(i);
    spdlog::info("retrieve {:.3} seconds.", sw);
}

TEST(TimeCacheSpeedTest3, Test) {
    using namespace std::chrono_literals;
    TimeCache<int, std::shared_ptr<std::string>> mc;

    mc.set_max_count(5000);

    spdlog::stopwatch sw;
    start_logger(spdlog::level::info);

    auto buffer     = std::make_shared<std::string>("testing");
    auto futuretime = clock::now();
    auto uuid       = Uuid();

    sw.reset();
    for (int i = 0; i < 10000; i++)
        mc.store(i, buffer, futuretime, uuid, futuretime);
    spdlog::info("store {:.3} seconds.", sw);
}
