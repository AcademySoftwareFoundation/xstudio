// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/json_store.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;
using namespace nlohmann;

TEST(JsonStoreTest, Test) {
    JsonStore j;
    EXPECT_TRUE(j.empty()) << "Structure should be empty on init";
    j["hello"] = 1;
    EXPECT_FALSE(j.empty()) << "Structure should not be empty";

    EXPECT_EQ(j.get("/hello"), 1) << "Should be == 1";

    EXPECT_THROW(static_cast<void>(j.get("/hell")), nlohmann::detail::exception)
        << "Should throw";

    j.set(1, "/hell");

    EXPECT_EQ(j.get("/hell"), 1) << "Should be  == 1";

    j["hello"] = {0, 1, 2, 3};

    EXPECT_EQ(j.get("/hello/1"), 1) << "Should be == 1";

    j.remove("/hello");
    EXPECT_THROW(static_cast<void>(j.get("/hello")), nlohmann::detail::exception)
        << "Should be null";

    j["test"] = {{"currency", "UK"}};

    EXPECT_EQ(j.get("/test/currency"), "UK") << "Should be UK";

    j = j.patch(R"([{"op":"replace","path":"/test/currency","value":"ER"}])"_json);

    EXPECT_EQ(j.get("/test/currency"), "ER") << "Should be ER";

    j["test"] = j["test"].patch(R"([{"op":"replace","path":"/currency","value":"OR"}])"_json);

    EXPECT_EQ(j.get("/test/currency"), "OR") << "Should be OR";

    j.clear();

    // j["hello"] = 1;
    // j["foo"] = 1;

    j.merge(R"({
	  "foo": "bar"
	})"_json);

    j.merge(
        R"({
	  "foo": "bar"
	})"_json,
        "/test");

    EXPECT_EQ(j.get("/foo"), "bar") << "Should be bar";
    EXPECT_EQ(j.get("/test/foo"), "bar") << "Should be bar";


    j.clear();
    j = R"({"test": {"one": 3 }})"_json;
    j.merge(R"({"test": {"two": 3 }, "doube":[1]})"_json);

    EXPECT_EQ(j.dump(2), "bar") << "Should be bar";
}
