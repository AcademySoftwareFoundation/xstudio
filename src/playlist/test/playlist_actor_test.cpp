// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <gtest/gtest.h>

#include "xstudio/atoms.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/playhead/sub_playhead.hpp"
#include "xstudio/playhead/playhead_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;
using namespace xstudio::media;
using namespace xstudio::global_store;
using namespace xstudio::global;
using namespace xstudio::media_cache;
using namespace xstudio::playhead;
using namespace xstudio::media_reader;
using namespace xstudio::playlist;
using namespace xstudio;
using namespace std::chrono_literals;


using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()


TEST(PlaylistActorTest, Test) {
    fixture f;
    auto tmp = f.self->spawn<PlaylistActor>("Test");

    f.self->request(tmp, std::chrono::seconds(10), name_atom_v)
        .receive(
            [&](const std::string &name) { EXPECT_EQ(name, "Test"); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->anon_send(tmp, name_atom_v, "Test2");
    f.self->request(tmp, std::chrono::seconds(10), name_atom_v)
        .receive(
            [&](const std::string &name) { EXPECT_EQ(name, "Test2"); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    auto su1 = Uuid::generate();
    f.self->anon_send(
        tmp,
        add_media_atom_v,
        f.self->spawn<MediaActor>(
            "test",
            Uuid(),
            UuidActorVector({UuidActor(
                su1,
                f.self->spawn<MediaSourceActor>(
                    "test",
                    posix_path_to_uri(TEST_RESOURCE "/media/test.{:04d}.ppm"),
                    FrameList(1, 10),
                    utility::FrameRate(timebase::k_flicks_24fps),
                    su1))})),

        Uuid());


    auto su2 = Uuid::generate();
    f.self->anon_send(
        tmp,
        add_media_atom_v,
        f.self->spawn<MediaActor>(
            "test",
            Uuid(),
            UuidActorVector({UuidActor(
                su1,
                f.self->spawn<MediaSourceActor>(
                    "test",
                    posix_path_to_uri(TEST_RESOURCE "/media/test.mov"),
                    utility::FrameRate(timebase::k_flicks_24fps),
                    su2))})),
        Uuid());

    JsonStore serial;
    f.self->request(tmp, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &jsn) { serial = jsn; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    auto tmp2 = f.self->spawn<PlaylistActor>(serial);
    auto tmp3 = f.self->spawn<PlaylistActor>(serial);

    f.self->request(tmp2, std::chrono::seconds(10), name_atom_v)
        .receive(
            [&](const std::string &name) { EXPECT_EQ(name, "Test2"); },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });
}
#pragma message "This needs fixing"

// TEST(PlaylistActorMediaTest, Test) {
// 	fixture f;
// 	auto gsa = f.self->spawn<GlobalActor>();

// 	auto tmp = f.self->spawn<PlaylistActor>("Test");
// 	f.self->send(tmp, add_media_atom_v,
// 		f.self->spawn<MediaActor>(
// 			"Media1",
// 			Uuid(),
// 			f.self->spawn<MediaSourceActor>("MediaSource1",
// posix_path_to_uri(TEST_RESOURCE
// "/media/test.{:04d}.ppm"), FrameList(1, 10 ))
// 		),
// 		Uuid()
// 	);

// 	f.self->send(tmp, add_media_atom_v,
// 		f.self->spawn<MediaActor>(
// 			"Media2",
// 			Uuid(),
// 			f.self->spawn<MediaSourceActor>("MediaSource2",
// posix_path_to_uri(TEST_RESOURCE
// "/media/test.{:04d}.ppm"), FrameList(1, 10 ))
// 		),
// 		Uuid()
// 	);

// 	f.self->anon_send(tmp, add_media_atom_v,
// 		f.self->spawn<MediaActor>(
// 			"Media3",
// 			Uuid(),
// 			f.self->spawn<MediaSourceActor>("MediaSource3",
// posix_path_to_uri(TEST_RESOURCE
// "/media/test.mov"))
// 		),
// 		Uuid()
// 	);

// 	// check our media refs work..
// 	f.self->request(tmp, std::chrono::seconds(10), get_media_pointer_atom_v, media::MT_IMAGE,
// 0).receive(
// 		[&](const media::AVFrameID &result) {
// 			EXPECT_EQ(result.frame_, 1);
// 			EXPECT_EQ(result.uri_, posix_path_to_uri(TEST_RESOURCE
// "/media/test.0001.ppm")
// )
// << to_string(result.uri_);
// 		},
// 		[&](const caf::error&err) {
// 			EXPECT_TRUE(false) << to_string(err);
// 		}
// 	);

// 	f.self->request(tmp, std::chrono::seconds(10), get_media_atom_v, true).receive(
// 		[&](std::vector<ContainerDetail> result) {
// 			EXPECT_EQ(result[0].name_, "Media1");
// 		},
// 		[&](const caf::error&err) {
// 			EXPECT_TRUE(false) << to_string(err);
// 		}
// 	);
// 	f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
// 	f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
// }

TEST(PlaylistPlayheadActorTest, Test) {
    fixture f;
    // need support actors..
    //  auto gsa = f.self->spawn<GlobalActor>();

    //  	auto tmp = f.self->spawn<PlaylistActor>("Test");
    // f.self->send(tmp, add_media_atom_v,
    // 	f.self->spawn<MediaActor>(
    // 		f.self->spawn<MediaSourceActor>("test", posix_path_to_uri(TEST_RESOURCE
    // "/media/test.{:04d}.ppm"), FrameList(1, 10) )
    // 	),
    // 	Uuid()
    // );
    // f.self->send(tmp, add_media_atom_v,
    // 	f.self->spawn<MediaActor>(
    // 		f.self->spawn<MediaSourceActor>("test", posix_path_to_uri(TEST_RESOURCE
    // "/media/test.mov"))
    // 	),
    // 	Uuid()
    // );

    //  caf::actor pa;
    //  f.self->request(tmp, infinite, create_playhead_atom_v).receive(
    //    [&](caf::actor a) {
    //        pa = a;
    //      },
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    //    );

    //  f.self->request(pa, infinite, buffer_atom_v).receive(
    //    [&](ImageBufPtr buf) {
    //        EXPECT_FALSE(buf);
    //      },
    // 	[&](const caf::error&err) {
    // 		EXPECT_TRUE(false) << to_string(err);
    // 	}
    //    );

    // auto r1 = f.self->request(
    // 	pa, infinite, precache_atom_v,
    // 	FrameRateDuration(1,24), utility::clock::now() + std::chrono::microseconds(100)
    // );
    // auto r2 = f.self->request(
    // 	pa, infinite, precache_atom_v,
    // 	FrameRateDuration(2,24), utility::clock::now() + std::chrono::microseconds(200)
    // );
    // auto r3 = f.self->request(
    // 	pa, infinite, precache_atom_v,
    // 	FrameRateDuration(3,24), utility::clock::now() + std::chrono::microseconds(300)
    // );
    // auto r4 = f.self->request(
    // 	pa, infinite, precache_atom_v,
    // 	FrameRateDuration(400,24), utility::clock::now() +
    // std::chrono::microseconds(400)
    // );


    // r1.receive(
    //   [&](ImageBufPtr buf) {
    //       EXPECT_TRUE(buf) << "Failed to create buffer";
    //     },
    // [&](const caf::error&err) {
    // 	EXPECT_TRUE(false) << to_string(err);
    // }
    //   );

    // r2.receive(
    //   [&](ImageBufPtr buf) {
    //       EXPECT_TRUE(buf) << "Failed to create buffer";
    //     },
    // [&](const caf::error&err) {
    // 	EXPECT_TRUE(false) << to_string(err);
    // }
    //   );
    // r3.receive(
    //   [&](ImageBufPtr buf) {
    //       EXPECT_TRUE(buf) << "Failed to create buffer";
    //     },
    // [&](const caf::error&err) {
    // 	EXPECT_TRUE(false) << to_string(err);
    // }
    //   );

    // r4.receive(
    //   [&](ImageBufPtr buf) {
    //       EXPECT_FALSE(buf) << "Failed to create buffer";
    //     },
    // [&](const caf::error&err) {
    // 	EXPECT_TRUE(true) << to_string(err);
    // }
    //   );

    // f.self->request(pa, infinite, move_atom_v, 0).receive(
    //   [&](ShowParams buf) {
    //       EXPECT_TRUE(std::get<0>(buf)) << "Failed to create buffer";
    //     },
    // [&](const caf::error&err) {
    // 	EXPECT_TRUE(false) << to_string(err);
    // }
    //   );
    // f.self->send_exit(pa, caf::exit_reason::user_shutdown);

    // f.self->send_exit(gsa, caf::exit_reason::user_shutdown);
}

TEST(PlaylistActorContainerTest, Test) {
    fixture f;
    auto tmp = f.self->spawn<PlaylistActor>("Test");
    Uuid gu1;
    Uuid gu2;

    f.self->request(tmp, infinite, create_group_atom_v, "Group", Uuid())
        .receive(
            [&](const Uuid &u) {
                gu1 = u;
                EXPECT_TRUE(true) << "Failed to create group";
            },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });
    f.self->request(tmp, infinite, create_group_atom_v, "Group2", gu1)
        .receive(
            [&](const Uuid &u) {
                gu2 = u;
                EXPECT_TRUE(true) << "Failed to create group";
            },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->request(tmp, infinite, create_subset_atom_v, "SubsetTest", gu1, false)
        .receive(
            [&](const UuidUuidActor &a) {
                EXPECT_TRUE(a.second.actor()) << "Failed to create subset";
            },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->request(tmp, infinite, create_subset_atom_v, "SubsetTest2", gu1, true)
        .receive(
            [&](const UuidUuidActor &a) {
                EXPECT_TRUE(a.second.actor()) << "Failed to create subset";
            },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    JsonStore js;
    f.self->request(tmp, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &j) { js = j; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    std::cout << js.dump() << std::endl;
    auto tmp2 = f.self->spawn<PlaylistActor>(js);

    f.self->request(tmp, infinite, get_container_atom_v)
        .receive(
            [&](const PlaylistTree &ct) { std::cout << ct << std::endl; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->request(tmp2, infinite, get_container_atom_v)
        .receive(
            [&](const PlaylistTree &ct) { std::cout << ct << std::endl; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });

    f.self->request(tmp2, infinite, serialise_atom_v)
        .receive(
            [&](const JsonStore &j) { js = j; },
            [&](const caf::error &err) { EXPECT_TRUE(false) << to_string(err); });
    std::cout << js.dump() << std::endl;

    f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
    f.self->send_exit(tmp2, caf::exit_reason::user_shutdown);
}
