// SPDX-License-Identifier: Apache-2.0
#include "xstudio/utility/remote_session_file.hpp"
#include <gtest/gtest.h>

using namespace xstudio::utility;

TEST(SessionFileTest, Test) {
    RemoteSessionFile local(".", 12345);
    RemoteSessionFile remote(".", 12346, "remotetest", "cookham", true);
    RemoteSessionFile remote2(remote.filepath());
    remote2.set_remove_on_delete();


    EXPECT_EQ(remote2.pid(), 0);

    local.cleanup();
    remote.cleanup();
    remote2.cleanup();
}

TEST(SessionFileManager, Test) {
    RemoteSessionManager rsm(".");
    rsm.create_session_file(1234);
    rsm.create_session_file(12346, "remotetest", "cookham", true);
    rsm.create_session_file(12346, "local", "localhost");

    auto first_local = rsm.first_api();
    auto find_remote = rsm.find("remotetest");


    EXPECT_EQ(first_local->session_name(), "local");

    EXPECT_EQ(find_remote->session_name(), "remotetest");
    rsm.remove_session("local");

    auto find_local = rsm.find("local");
    EXPECT_FALSE(find_local);
}
