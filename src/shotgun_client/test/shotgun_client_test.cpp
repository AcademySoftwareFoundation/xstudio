// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/uuid.hpp"

using namespace xstudio::utility;
using namespace xstudio::shotgun_client;

TEST(ShotgunClientTest, TestFilters) {
    // ShotgunClient sc;

    EXPECT_EQ(
        static_cast<nlohmann::json>(FilterBy()).dump(),
        "{\"conditions\":[],\"logical_operator\":\"and\"}");
    EXPECT_EQ(
        static_cast<nlohmann::json>(Checkbox("DateTime").is_null()).dump(),
        "[\"DateTime\",\"is\",null]");

    EXPECT_EQ(
        static_cast<nlohmann::json>(Image("TEST").is()).dump(), "[\"TEST\",\"is_not\",null]");

    auto t1 = FilterBy().And(Checkbox("checkbox").is_null());
    EXPECT_EQ(
        static_cast<nlohmann::json>(t1).dump(),
        "{\"conditions\":[[\"checkbox\",\"is\",null]],\"logical_operator\":\"and\"}");
    t1.push_back(Checkbox("another").is_null());
    EXPECT_EQ(
        static_cast<nlohmann::json>(t1).dump(),
        "{\"conditions\":[[\"checkbox\",\"is\",null],[\"another\",\"is\",null]],\"logical_"
        "operator\":\"and\"}");

    EXPECT_EQ(static_cast<nlohmann::json>(Number("TEST").is(1)).dump(), "[\"TEST\",\"is\",1]");

    auto t2 = FilterBy().And(Checkbox("checkbox").is(false));
    EXPECT_EQ(std::get<bool>(std::get<Checkbox>(t2.front()).value()), false);
    EXPECT_EQ(
        static_cast<nlohmann::json>(t2).dump(),
        static_cast<nlohmann::json>(FilterBy(t2.serialise())).dump());

    auto t3 = FilterBy().And(Checkbox("checkbox").is(true));
    EXPECT_EQ(std::get<bool>(std::get<Checkbox>(t3.front()).value()), true);

    auto t4 = FilterBy().And(FilterBy());
    EXPECT_EQ(
        static_cast<nlohmann::json>(t4).dump(),
        "{\"conditions\":[{\"conditions\":[],\"logical_operator\":\"and\"}],\"logical_"
        "operator\":\"and\"}");
    EXPECT_EQ(
        static_cast<nlohmann::json>(FilterBy(t4.serialise())).dump(),
        static_cast<nlohmann::json>(t4).dump());

    auto t5 = Checkbox("checkbox").is_null();
    EXPECT_EQ(
        static_cast<nlohmann::json>(t5).dump(),
        static_cast<nlohmann::json>(Checkbox(t5.serialise())).dump());

    // auto t6 = Number("number").in({1,2,3});
    // EXPECT_EQ(static_cast<nlohmann::json>(t6).dump(),
    // static_cast<nlohmann::json>(Number(t6.serialise())).dump());

    // auto t7 = Number("number").between(1,2);
    // EXPECT_EQ(static_cast<nlohmann::json>(t7).dump(),
    // static_cast<nlohmann::json>(Number(t7.serialise())).dump());

    // auto t8 = DateTime("DateTime").in_last(1,Period::DAY);
    // EXPECT_EQ(static_cast<nlohmann::json>(t8).dump(),
    // static_cast<nlohmann::json>(DateTime(t8.serialise())).dump());

    // auto t9 =FilterBy().And(Number("number").is(10));
    // EXPECT_EQ(static_cast<nlohmann::json>(t9).dump(),
    // static_cast<nlohmann::json>(Number(t9.serialise())).dump());

    auto t6 = FilterBy().And(Checkbox("checkbox").is(false), Image("TEST").is());
    auto t7 = FilterBy().And(Checkbox("checkbox").is(false));
    t7.push_back(Image("TEST").is());
    EXPECT_EQ(static_cast<nlohmann::json>(t6).dump(), static_cast<nlohmann::json>(t7).dump());

    auto t8 = FilterBy().And(
        RelationType("playlist").is(R"({"id":1311301, "type":"Playlist"})"_json));
    EXPECT_EQ(
        static_cast<nlohmann::json>(t8).dump(),
        "{\"conditions\":[[\"playlist\",\"is\",{\"id\":1311301,\"type\":\"Playlist\"}]],"
        "\"logical_operator\":\"and\"}");


    // auto t8 = FilterBy().And(EntityType)
    // EXPECT_EQ(static_cast<nlohmann::json>(t8).dump(),
    // "{\"conditions\":[[\"playlist\",\"is\",{\"type\":\"Playlist\",\"id\":1311301}]],\"logical_operator\":\"and\"}");

    // {
    // "filters":{
    //   "logical_operator": "and",
    //   "conditions": [
    //     ["playlist", "is", {"type":"Playlist","id":1311301}]
    //   ]
    // }
    // }

    // auto filt = Filter("Latest", "Version", "GIRONA", 1350);
    // filt.push_back(Number("project.Project.id").is(filt.project_id()));
    // filt.push_back(Text("sg_on_disk_lon").in(std::vector<std::string>({"Full","Partial"})));
    // filt.push_back(Text("sg_path_to_movie").is_not_null());
    // filt.push_back(Checkbox("sg_deleted").is_null());
    // filt.push_back(Number("frame_count").is_not_null());
    // filt.push_back(Text("sg_category").is("Render"));
    // filt.push_back(DateTime("created_at").in_last(1, Period::WEEK));

    // EXPECT_EQ(filt.serialise().dump(2), "");
}


// std::cerr << FilterBy()
// std::cerr << Is() << std::endl;
// FilterBy f();.or(Checkbox("name").is(true)).or().and();
// std::cerr << Checkbox("name").is(true) << std::endl << std::endl;
// std::cerr << Text("name").in({"hello","goodbye"}) << std::endl << std::endl;
// std::cerr << Number("name").is(32) << std::endl << std::endl;
// std::cerr << Number("name").between(32,35) << std::endl << std::endl;
// std::cerr << Image("image").is() << std::endl << std::endl;
// std::cerr << List("image").in({"one", "toe"}) << std::endl << std::endl;
// std::cerr << StatusList("image").is("one") << std::endl << std::endl;
// std::cerr << DateTime("DateTime").in_calendar_day(1) << std::endl << std::endl;
// std::cerr << DateTime("DateTime").in_last(1, Period::HOUR) << std::endl << std::endl;

// std::cerr << Float("DateTime").is(1.0) << std::endl << std::endl;
// std::cerr << Number("DateTime").is(1) << std::endl << std::endl;

//                  .Or(Checkbox("name").is(true))
//                  .Or(Checkbox("wibble").is(true))
//                  .Or(FilterBy().And(Text("name").in({"hello", "goodbye"})))
//           << std::endl;
// std::cerr << EntityType().With("Project", FilterBy()) << std::endl;
