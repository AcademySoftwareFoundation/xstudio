// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>
#include <caf/actor_registry.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store_actor.hpp"
#include "xstudio/json_store/json_store_helper.hpp"
#include "xstudio/media_metadata/media_metadata_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::json_store;
using namespace xstudio::media_metadata;
using namespace xstudio::global_store;
using namespace xstudio::global;

using namespace caf;
using namespace std::chrono_literals;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()


TEST(GlobalMediaMetadataActorTest, Test) {
    fixture fix;

    auto gsa = fix.self->spawn<GlobalActor>();

    caf::actor tmp;

    try {

        tmp = fix.self->system().registry().template get<caf::actor>(
            xstudio::media_metadata_registry);

    } catch (const std::exception &) {
    }

    auto a = fix.self->request(
        tmp,
        std::chrono::seconds(10),
        get_metadata_atom_v,
        posix_path_to_uri(TEST_RESOURCE "/media/test.mov"));
    auto b = fix.self->request(
        tmp,
        std::chrono::seconds(10),
        get_metadata_atom_v,
        posix_path_to_uri(TEST_RESOURCE "/media/test.0001.exr"));
    auto c = fix.self->request(
        tmp,
        std::chrono::seconds(10),
        get_metadata_atom_v,
        posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
        "OpenExr");
    auto d = fix.self->request(
        tmp,
        std::chrono::seconds(10),
        get_metadata_atom_v,
        posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
        "Exr");
    auto e = fix.self->request(
        tmp,
        std::chrono::seconds(10),
        get_metadata_atom_v,
        posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
        "FFProbe");

    a.receive(
        [&](const std::pair<JsonStore, int> &_json) {
            EXPECT_FALSE(_json.first.empty()) << "Should return valid json";
        },
        [&](const caf::error &err) {
            EXPECT_TRUE(false) << "Should return valid json" << std::endl << to_string(err);
        });

    b.receive(
        [&](const std::pair<JsonStore, int> &_json) {
            EXPECT_FALSE(_json.first.empty()) << "Should return valid json";
        },
        [&](const caf::error &err) {
            EXPECT_TRUE(false) << "Should return valid json" << std::endl << to_string(err);
        });

    c.receive(
        [&](const std::pair<JsonStore, int> &) { EXPECT_FALSE(true) << "Should fail"; },
        [&](const caf::error &err) {
            EXPECT_TRUE(true) << "Should fail" << std::endl << to_string(err);
        });

    d.receive(
        [&](const std::pair<JsonStore, int> &) { EXPECT_FALSE(true) << "Should fail"; },
        [&](const caf::error &err) {
            EXPECT_TRUE(true) << "Should fail" << std::endl << to_string(err);
        });

    e.receive(
        [&](const std::pair<JsonStore, int> &_json) {
            EXPECT_FALSE(_json.first.empty()) << "Should return valid json";
        },
        [&](const caf::error &err) {
            EXPECT_TRUE(false) << "Should return valid json" << std::endl << to_string(err);
        });

    fix.self->send_exit(tmp, caf::exit_reason::user_shutdown);
    fix.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}
