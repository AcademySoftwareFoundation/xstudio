// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio::utility;
using namespace xstudio::global_store;


TEST(GlobalStoreTest, Test) {
    JsonStore j = global_store_builder(std::vector<std::string>{ROOT_DIR "/share/preference"});


    // TODO: re-do these tests. GlobalStoreDef not used anywhere in the application
    // so it's getting removed.

    // EXPECT_EQ(j.get("/core/image_cache/max_count").get<GlobalStoreDef>().datatype(), "int")
    //   << "Should be == int";

    // EXPECT_EQ(j.get("/hell"), nullptr) << "Should be null";

    // j.set("/hell", 1);

    // EXPECT_EQ(j.get("/hell"), 1) << "Should be  == 1";

    // j["hello"] = {0,1,2,3};

    // EXPECT_EQ(j.get("/hello/1"), 1) << "Should be == 1";

    // j.remove("/hello");
    // EXPECT_EQ(j.get("/hello"), nullptr) << "Should be null";

    // j["test"] = {{"currency", "UK"}};

    // EXPECT_EQ(j.get("/test/currency"), "UK") << "Should be UK";

    // j = j.patch(R"([{"op":"replace","path":"/test/currency","value":"ER"}])"_json);

    // EXPECT_EQ(j.get("/test/currency"), "ER") << "Should be ER";

    // j["test"] =
    // j["test"].patch(R"([{"op":"replace","path":"/currency","value":"OR"}])"_json);

    // EXPECT_EQ(j.get("/test/currency"), "OR") << "Should be OR";
}
