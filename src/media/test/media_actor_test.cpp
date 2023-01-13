// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/json_store/json_store_helper.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/ui/viewport/viewport.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media;
using namespace xstudio::media_metadata;
using namespace xstudio::global;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"

ACTOR_TEST_SETUP()

TEST(MediaActorTest, Test) {
    fixture f;
    // start_logger(spdlog::level::debug);
    auto gsa = f.self->spawn<GlobalActor>();

    auto su1   = Uuid::generate();
    auto media = f.self->spawn<MediaActor>(
        "test",
        Uuid(),
        UuidActorVector({UuidActor(
            su1,
            f.self->spawn<MediaSourceActor>(
                "test",
                posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
                utility::FrameRate(timebase::k_flicks_24fps),
                su1))}));

    auto media_s = f.self->spawn<MediaSourceActor>(
        "test", posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.exr"), FrameList(1, 10));

    Uuid media_s_uuid;

    f.self->request(media, infinite, add_media_source_atom_v, media_s)
        .receive(
            [&](const Uuid &uuid) { media_s_uuid = uuid; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });

    f.self->request(media, infinite, uuid_atom_v)
        .receive(
            [&](const Uuid &uuid) {
                EXPECT_FALSE(uuid.is_null()) << "Should return valid uuid";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });

    JsonStoreHelper jsh(f.system, media);
    f.self->request(media, infinite, get_metadata_atom_v)
        .receive(
            [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });
    EXPECT_EQ(jsh.get<std::string>("/metadata/media/@/format/size"), "95566")
        << "Should be 95566";

    f.self->request(media, infinite, current_media_source_atom_v, media_s_uuid)
        .receive(
            [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

    EXPECT_THROW(
        static_cast<void>(jsh.get<std::string>("/metadata/media/@/format/size")),
        std::runtime_error);

    f.self->request(media, infinite, get_metadata_atom_v)
        .receive(
            [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

    EXPECT_EQ(
        jsh.get<std::string>("/metadata/media/@1/headers/0/capDate/value"),
        "2020:01:21 09:38:59")
        << "Should be 2020:01:21 09:38:59";

    JsonStore serial;
    f.self->request(media, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &jsn) { serial = jsn; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

    // std::cout << serial.dump() << std::endl;
    media = f.self->spawn<MediaActor>(serial);
    JsonStoreHelper jsh2(f.system, media);

    // fails and crashes..
    EXPECT_EQ(
        jsh2.get<std::string>("/metadata/media/@1/headers/0/capDate/value"),
        "2020:01:21 09:38:59")
        << "Should be 2020:01:21 09:38:59";

    f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}


TEST(MediaSourceActorTest, Test) {
    fixture f;

    auto tmp = f.self->spawn<MediaSourceActor>();
    auto c   = make_function_view(tmp);

    // const Uuid u = c(Uuid, get_uuid_atom_v);

    f.self->request(tmp, std::chrono::seconds(10), uuid_atom_v)
        .receive(
            [&](const Uuid &uuid) {
                EXPECT_FALSE(uuid.is_null()) << "Should return valid uuid";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });

    JsonStore serial;
    f.self->request(tmp, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &jsn) { serial = jsn; },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });
    // we should be able to spawn a clone of the above...

    auto tmp2 = f.self->spawn<MediaSourceActor>(serial);
    f.self->request(tmp2, std::chrono::seconds(10), uuid_atom_v)
        .receive(
            [&](const Uuid &uuid) {
                EXPECT_FALSE(uuid.is_null()) << "Should return valid uuid";
            },
            [&](const caf::error &) { EXPECT_TRUE(false) << "Should return valid uuid"; });
}

TEST(MediaSourceActorMetaTest, Test) {
    fixture f;
    try {
        auto gsa = f.self->spawn<GlobalActor>();

        auto media = f.self->spawn<MediaSourceActor>(
            "test", posix_path_to_uri(TEST_RESOURCE "/media/test.mov"));
        auto m = make_function_view(media);

        JsonStoreHelper jsh(f.system, media);

        f.self->request(media, infinite, get_metadata_atom_v)
            .receive(
                [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
                [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

        EXPECT_EQ(jsh.get<std::string>("/metadata/media/@/format/size"), "95566")
            << "Should be 95566";

        media = f.self->spawn<MediaSourceActor>(
            "test",
            posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.exr"),
            FrameList(1, 100));
        m = make_function_view(media);

        JsonStoreHelper jsh2(f.system, media);

        f.self->request(media, infinite, get_metadata_atom_v)
            .receive(
                [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
                [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

        EXPECT_EQ(
            jsh2.get<std::string>("/metadata/media/@1/headers/0/capDate/value"),
            "2020:01:21 09:38:59")
            << "Should be 2020:01:21 09:38:59";
        f.self->request(media, infinite, get_metadata_atom_v)
            .receive(
                [&](const bool &done) { EXPECT_TRUE(done) << "Should return true"; },
                [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });

        EXPECT_EQ(
            jsh2.get<std::string>("/metadata/media/@100/headers/0/capDate/value"),
            "2020:01:21 09:39:05")
            << "Should be 2020:01:21 09:39:05";

        JsonStore serial;
        f.self->request(media, infinite, serialise_atom_v)
            .receive(
                [&](const JsonStore &jsn) { serial = jsn; },
                [&](const caf::error &) { EXPECT_TRUE(false) << "Should return true"; });
        // we should be able to spawn a clone of the above...

        auto media3 = f.self->spawn<MediaSourceActor>(serial);

        JsonStoreHelper jsh3(f.system, media3);
        EXPECT_EQ(
            jsh3.get<std::string>("/metadata/media/@100/headers/0/capDate/value"),
            "2020:01:21 09:39:05")
            << "Should be 2020:01:21 09:39:05";

        f.self->send_exit(gsa, caf::exit_reason::user_shutdown);

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
