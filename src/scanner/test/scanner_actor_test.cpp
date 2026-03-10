// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/scanner/scanner_actor.hpp"
#include "xstudio/utility/helpers.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace xstudio;
using namespace caf;
using namespace xstudio::scanner;
using namespace xstudio::utility;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(ScannerHelperActorTest, ChecksumCacheRefreshesWhenFileChanges) {
    fixture f;
    auto helper = f.self->spawn<ScanHelperActor>();

    const auto tmpdir = testing::TempDir();
    const auto path   = std::filesystem::path(tmpdir) / "scanner_checksum_test.mov";

    {
        std::ofstream stream(path);
        stream << "alpha";
    }

    auto uri = posix_path_to_uri(path.string());
    ASSERT_FALSE(uri.empty());

    const auto first = request_receive<std::pair<std::string, uintmax_t>>(
        *(f.self), helper, media::checksum_atom_v, uri);
    const auto second = request_receive<std::pair<std::string, uintmax_t>>(
        *(f.self), helper, media::checksum_atom_v, uri);

    EXPECT_EQ(first, second);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    {
        std::ofstream stream(path);
        stream << "beta-beta";
    }

    const auto third = request_receive<std::pair<std::string, uintmax_t>>(
        *(f.self), helper, media::checksum_atom_v, uri);

    EXPECT_NE(first.first, third.first);
    EXPECT_NE(first.second, third.second);
}

TEST(ScannerHelperActorTest, RelinkFindsMatchingFileByChecksum) {
    fixture f;
    auto helper = f.self->spawn<ScanHelperActor>();

    const auto tmpdir = std::filesystem::path(testing::TempDir()) / "scanner_relink";
    std::filesystem::create_directories(tmpdir);
    const auto file_path = tmpdir / "shot.0001.exr";

    {
        std::ofstream stream(file_path);
        stream << "frame-data";
    }

    auto file_uri = posix_path_to_uri(file_path.string());
    auto dir_uri  = posix_path_to_uri(tmpdir.string());
    ASSERT_FALSE(file_uri.empty());
    ASSERT_FALSE(dir_uri.empty());

    const auto [checksum, size] = request_receive<std::pair<std::string, uintmax_t>>(
        *(f.self), helper, media::checksum_atom_v, file_uri);
    const media::MediaSourceChecksum pin{file_path.filename().string(), checksum, size};

    const auto relinked =
        request_receive<caf::uri>(*(f.self), helper, media::relink_atom_v, pin, dir_uri, false);
    EXPECT_EQ(relinked, file_uri);
}
