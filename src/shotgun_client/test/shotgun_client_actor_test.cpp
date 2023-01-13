// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/shotgun_client/shotgun_client_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::shotgun_client;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

TEST(ShotgunClientActorTest, Test) {
    // fixture f;
    // auto tmp = f.self->spawn<ShotgunClientActor>();

    // f.self->send(tmp, shotgun_host_atom_v, "http://shotgun");

    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_info_atom_v);
    // 	EXPECT_EQ(body["data"]["api_version"], "v1");
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }

    // try {
    // 	auto body = request_receive<std::string>(*(f.self), tmp, shotgun_authenticate_atom_v,
    // "username", "password", AM_LOGIN); 	std::cerr << body << std::endl; } catch(const
    // std::exception &err) { 	std::cerr << err.what() << std::endl;
    // }

    // try {
    // 	auto body = request_receive<std::string>(*(f.self), tmp, shotgun_acquire_token_atom_v);
    // 	std::cerr << body << std::endl;
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }

    // auto query = R"({"sg_status_list": "ip"})"_json;

    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp,
    // 		shotgun_entity_filter_atom_v, "Projects",
    // 		JsonStore(query)
    // 		);
    // 	std::cerr << body.dump(2) << std::endl;
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }


    // try {
    // 	std::vector<std::string> fields;
    // 	// fields.push_back("name")
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_entity_atom_v,
    // "Projects", 329, fields);
    // 	// auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_entity_atom_v,
    // "Projects", 329,std::vector<std::string>()); 	std::cerr << body.dump(2) << std::endl;
    // } catch(const std::exception &err) { 	std::cerr << err.what() << std::endl;
    // }

    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_preferences_atom_v);
    // 	std::cerr << body.dump(2) << std::endl;
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }
    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_projects_atom_v);
    // 	std::cerr << body.dump(2) << std::endl;
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }

    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_schema_atom_v, 329);
    // 	std::cerr << body.dump(2) << std::endl;
    // } catch(const std::exception &err) {
    // 	std::cerr << err.what() << std::endl;
    // }
    // try {
    // 	auto query = R"({})"_json;
    // 	query["Project"] = static_cast<nlohmann::json>(FilterBy());
    // 	// query["Project"] =
    // static_cast<nlohmann::json>(FilterBy().And(StatusList("sg_status").is("Active")));

    // 	auto body = request_receive<JsonStore>(*(f.self), tmp,
    // shotgun_text_search_atom_v,"NS",JsonStore(query)); 	std::cerr << body.dump(2) <<
    // std::endl; } catch(const std::exception &err) { 	std::cerr << err.what() << std::endl;
    // }
    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp, shotgun_entity_atom_v,
    // "Projects", 329); 	std::cerr << body.dump(2) << std::endl; } catch(const
    // std::exception &err) { 	std::cerr << err.what() << std::endl;
    // }
    // try {
    // 	auto body = request_receive<JsonStore>(*(f.self), tmp,
    // shotgun_schema_entity_fields_atom_v, "Projects", 329); 	std::cerr << body.dump(2) <<
    // std::endl; } catch(const std::exception &err) { 	std::cerr << err.what() << std::endl;
    // }
}
