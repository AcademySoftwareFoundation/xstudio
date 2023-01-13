// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/embedded_python/embedded_python.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::embedded_python;

TEST(EmbeddedPython, Test) {
    start_logger(spdlog::level::debug);
    EmbeddedPython s{"test"};

    s.exec("print ('hello')");

    auto a = s.eval("print ('hello')");

    auto b = s.eval("1+1");
    EXPECT_EQ(b, 2);

    auto c = s.eval("a+b", nlohmann::json({{"a", 1}, {"b", 2}}));
    EXPECT_EQ(c, 3);

    auto d =
        s.eval_locals("c=locals()['a']+locals()['b']", nlohmann::json({{"a", 1}, {"b", 2}}));
    EXPECT_EQ(d["c"], 3);

    EXPECT_FALSE(s.connect(45500));
    try {
        s.exec("print ('hello')");
    } catch (...) {
    }
    try {
        s.exec("print hello')");
    } catch (...) {
    }
    try {
        s.exec("print ('hello')");
    } catch (...) {
    }

    // try{
    // 	auto u = s.create_session(true);
    // 	auto c = s.eval("'"+to_string(u)+"' in xstudio_sessions");

    // }catch(...){}
}
