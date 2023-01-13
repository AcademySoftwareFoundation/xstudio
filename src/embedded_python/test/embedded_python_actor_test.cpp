// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/embedded_python/embedded_python_actor.hpp"
#include "xstudio/playlist/playlist.hpp"


#include "xstudio/utility/serialise_headers.hpp"

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

using namespace xstudio::utility;
using namespace xstudio::embedded_python;
using namespace xstudio;

using namespace caf;


ACTOR_TEST_SETUP()


TEST(EmbeddedPythonActorTest, Test) {
    fixture f;
    auto tmp = f.self->spawn<EmbeddedPythonActor>("Test");

    // auto pyactor = py::cast(actor_cast<actor_addr>(tmp));
    // spdlog::warn("{}", to_string(tmp));
    // spdlog::warn("{}", to_string(actor_cast<actor_addr>(tmp)));
    // py::print(pyactor);

    try {
        auto result = request_receive<std::string>(*(f.self), tmp, name_atom_v);
        EXPECT_EQ(result, "Test");
    } catch (const std::exception &) {
    }

    try {
        auto result = request_receive<JsonStore>(*(f.self), tmp, python_eval_atom_v, "1+1");
        EXPECT_EQ(result, 2);
    } catch (const std::exception &) {
    }

    try {
        f.self->send(tmp, python_exec_atom_v, "a=3");
    } catch (const std::exception &) {
    }

    try {
        auto result = request_receive<JsonStore>(*(f.self), tmp, python_eval_atom_v, "a");
        EXPECT_EQ(result, 3);
    } catch (const std::exception &) {
    }

    try {
        auto result = request_receive<JsonStore>(
            *(f.self),
            tmp,
            python_eval_atom_v,
            "a+b",
            JsonStore(nlohmann::json({{"a", 1}, {"b", 2}})));
        EXPECT_EQ(result, 3);
    } catch (const std::exception &) {
    }

    try {
        auto result = request_receive<JsonStore>(
            *(f.self),
            tmp,
            python_eval_locals_atom_v,
            "c=locals()['a']+locals()['b']",
            JsonStore(nlohmann::json({{"a", 1}, {"b", 2}})));
        EXPECT_EQ(result["c"], 3);
    } catch (const std::exception &) {
    }

    f.self->send(tmp, python_exec_atom_v, "print ('hello')", utility::Uuid());

    try {
        auto result = request_receive<Uuid>(*(f.self), tmp, python_create_session_atom_v, true);
        EXPECT_FALSE(result.is_null());


        try {
            auto existsin = request_receive<JsonStore>(
                *(f.self),
                tmp,
                python_eval_atom_v,
                "'" + to_string(result) + "' in xstudio_sessions",
                utility::Uuid());
            EXPECT_EQ(existsin, true);
        } catch (const std::exception &) {
        }

        auto result2 =
            request_receive<Uuid>(*(f.self), tmp, python_remove_session_atom_v, result);
        EXPECT_TRUE(result2);

        try {
            auto existsin = request_receive<JsonStore>(
                *(f.self),
                tmp,
                python_eval_atom_v,
                "'" + to_string(result) + "' in xstudio_sessions",
                utility::Uuid());
            EXPECT_EQ(existsin, false);
        } catch (const std::exception &) {
        }

    } catch (const std::exception &) {
    }


    f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
}

// TEST(SessionSerializerTest, Test) {
// 	fixture f;

// 	binary_serializer::container_type buf;
// 	binary_serializer bs{f.system, buf};

// 	std::vector<UuidActor> dt1{};
// 	std::vector<UuidActor> dt2{};

// 	auto e = bs.apply(dt1);
// 	EXPECT_FALSE(e) << "unable to serialize" << to_string(e) << std::endl;

// 	binary_deserializer bd{f.system, buf};
// 	e = bd.apply(dt2);
// 	EXPECT_FALSE(e) << "unable to deserialize" << to_string(e) << std::endl;

// 	EXPECT_EQ(dt1, dt2) << "Creation from string should be equal";
// }
