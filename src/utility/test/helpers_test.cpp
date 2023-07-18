// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/serialise_headers.hpp"


#include "xstudio/utility/container.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/timecode.hpp"
#include "xstudio/utility/types.hpp"

using namespace caf;
using namespace xstudio::utility;

ACTOR_TEST_SETUP()


TEST(HelpersTest, Test) {
    FrameList fl;

    EXPECT_EQ(posix_path_to_uri("/file.mov", true), parse_cli_posix_path("/file.mov", fl));

    EXPECT_EQ(to_string(posix_path_to_uri("file.exr")), "file:file.exr");


    EXPECT_EQ(uri_to_posix_path(posix_path_to_uri("file.exr")), "file.exr");

    EXPECT_THROW(parse_cli_posix_path("file.{:04d}.exr", fl), std::runtime_error)
        << "Should be exception";

    EXPECT_THROW(parse_cli_posix_path("file.{:04d}.exr", fl, true), std::runtime_error)
        << "Should be exception";

    EXPECT_EQ(
        posix_path_to_uri("/file.{:04d}.exr", true),
        parse_cli_posix_path("/file.{:04d}.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri("/file.{:04d}.exr", true),
        parse_cli_posix_path("/file.1-10{:04d}.exr", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri("/file.{:04d}.exr", true),
        parse_cli_posix_path("/file.1-10#.exr", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri("/file.{:04d}.exr", true),
        parse_cli_posix_path("/file.@@@@.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri("/file.{:04d}.exr", true),
        parse_cli_posix_path("/file.#.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri("/file.{:03d}.exr", true),
        parse_cli_posix_path("/file.###.exr=1-10", fl));
    EXPECT_EQ("1-10", to_string(fl));

    EXPECT_EQ(
        posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm", true),
        parse_cli_posix_path(TEST_RESOURCE "/media/test.####.ppm", fl, true));
    EXPECT_EQ("1-10", to_string(fl));

    // EXPECT_EQ(
    //     uri_to_posix_path(url_to_uri(
    //         "file://localhost/user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");

    // EXPECT_EQ(
    //     uri_to_posix_path(
    //         url_to_uri("file:/user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");

    // EXPECT_EQ(
    //     uri_to_posix_path(
    //         url_to_uri("file:///user_data/test_files/demo_files/Beasts%20Of%20Burden/03.mov")),
    //     "/user_data/test_files/demo_files/Beasts Of Burden/03.mov");
}


TEST(Helpers2Test, Test) {
    EXPECT_EQ(expand_envvars("${HOME}"), std::getenv("HOME"));
    EXPECT_EQ(
        " " + expand_envvars("${HOME}") + " ", std::string(" ") + std::getenv("HOME") + " ");
}
