// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_cache/media_cache_actor.hpp"
#include "xstudio/media_reader/media_reader_actor.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::media_reader;
using namespace xstudio::media_cache;

using namespace caf;

#include "xstudio/utility/serialise_headers.hpp"


ACTOR_TEST_SETUP()

#pragma message "This needs fixing"

/*

TEST(MediaReaderActorTest, Test) {
   fixture f;

   std::shared_ptr<MediaReaderManager> mrm = std::make_shared<MediaReaderManager>();

   mrm->add_path(xstudio_root("/plugin"));
   mrm->load_plugins();


   auto tmp = f.self->spawn<MediaReaderWorkerActor>(mrm->construct("PPM"));
   auto c = make_function_view(tmp);
   caf::uri path = posix_path_to_uri(TEST_RESOURCE "/media/test.0001.ppm");
   caf::uri badpath = posix_path_to_uri(TEST_RESOURCE "/media/test..ppm");

        f.self->request(tmp, std::chrono::seconds(10), get_image_atom_v,
media::AVFrameID(path)).receive(
          [&](ImageBufPtrbuf) {
                EXPECT_TRUE(buf);
          },
          [&](const caf::error&) {
           EXPECT_TRUE(false) << "Should return valid buf";
          }
        );

        f.self->request(tmp, std::chrono::seconds(10), get_image_atom_v,
media::AVFrameID(badpath)).receive(
          [&](ImageBufPtrbuf) {
                EXPECT_FALSE(buf);
          },
                [&](const caf::error&err) {
                        EXPECT_TRUE(true) << to_string(err);
                }
        );
}*/


// TEST(CachingMediaReaderActorTest, Test) {
// 	fixture f;
// 	caf::uri path = posix_path_to_uri(TEST_RESOURCE "/media/test.0001.ppm");

// 	std::shared_ptr<MediaReaderManager> mrm = std::make_shared<MediaReaderManager>();

// 	mrm->add_path(xstudio_root("/plugin"));
// 	mrm->load_plugins();

// 	auto gmca = f.self->spawn<GlobalImageCacheActor>();

// 	auto tmp = f.self->spawn<CachingMediaReaderActor>(1, gmca, mrm);

// 	EXPECT_TRUE(tmp);

// 	auto gmra = f.self->spawn<GlobalMediaReaderActor>();
// 	auto tmp2 = f.self->spawn<CachingMediaReaderActor>(1);

// 	EXPECT_TRUE(tmp2);

// 	f.self->request(tmp, std::chrono::seconds(10), get_image_atom_v,
// media::AVFrameID(path)).receive(
// 	  [&](ImageBufPtrbuf) {
// 		EXPECT_TRUE(buf);
// 	  },
// 	  [&](const caf::error&) {
// 		EXPECT_TRUE(false) << "Should return valid buf";
// 	  }
// 	);

// 	f.self->request(tmp2, std::chrono::seconds(10), get_image_atom_v,
// media::AVFrameID(path)).receive(
// 	  [&](ImageBufPtrbuf) {
// 		EXPECT_TRUE(buf);
// 	  },
// 	  [&](const caf::error&) {
// 		EXPECT_TRUE(false) << "Should return valid buf";
// 	  }
// 	);

// 	f.self->send_exit(gmca, caf::exit_reason::user_shutdown);
// 	f.self->send_exit(gmra, caf::exit_reason::user_shutdown);
// }

// TEST(GlobalMediaReaderActorTest, Test) {
//   	fixture f;

//  	auto tmp = f.self->spawn<GlobalMediaReaderActor>();

//  	auto c = make_function_view(tmp);
//  	caf::uri path = posix_path_to_uri("/hosts/hawley/user_data/QT_PIX_FORMATS/Samsung
//  Landscape 4K Demo.mp4"); 	caf::uri badpath = posix_path_to_uri(TEST_RESOURCE
//  "/media/test..ppm");
//   media::AVFrameID mptr(path);

//   for (int i = 1; i < 2000; ++i) {
//     mptr.frame() = i;
//  	  f.self->request(tmp, std::chrono::seconds(10), get_image_atom_v, mptr).receive(
//       [&](ImageBufPtrbuf) {
//       EXPECT_TRUE(buf);
//       },
//       [&](const caf::error& e) {
//         std::cerr << "OIOI " << to_string(e) << "\n";
//       EXPECT_TRUE(false) << "Should return valid buf";
//  	    }
//  	  );
//      std::this_thread::sleep_for(std::chrono::milliseconds(30));
//   }

//  	f.self->request(tmp, std::chrono::seconds(10), get_image_atom_v,
//  media::AVFrameID(badpath)).receive(
//  	  [&](ImageBufPtrbuf) {
//  		EXPECT_FALSE(buf);
//  	  },
//  	  [&](const caf::error&) {
//  		EXPECT_TRUE(true);
//  	  }
//  	);

//     f.self->send_exit(tmp, caf::exit_reason::user_shutdown);
// }
