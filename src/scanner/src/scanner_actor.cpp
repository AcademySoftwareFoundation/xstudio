// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <filesystem>


#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/scanner/scanner_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::scanner;
using namespace caf;

namespace {
namespace fs = std::filesystem;


media::MediaStatus check_media_status(const MediaReference &mr) {
    media::MediaStatus ms = media::MediaStatus::MS_ONLINE;

    // determine actual state..
    // we can't quickly check for
    // MS_CORRUPT, MS_UNSUPPORTED, MS_UNREADABLE
    // So leave for readers..

    // test exists and is readable..
    try {

        if (mr.container()) {
            if (not fs::exists(uri_to_posix_path(mr.uri())))
                ms = media::MediaStatus::MS_MISSING;
        } else {
            // check first and last frame.
            if (mr.frame_count()) {
                int tmp;
                auto first_uri = mr.uri(0, tmp);
                auto last_uri  = mr.uri(mr.frame_count() - 1, tmp);

                if (not first_uri or not fs::exists(uri_to_posix_path(*first_uri))) {
                    ms = media::MediaStatus::MS_MISSING;
                } else if (not last_uri or not fs::exists(uri_to_posix_path(*last_uri))) {
                    ms = media::MediaStatus::MS_MISSING;
                }
            } else {
                ms = media::MediaStatus::MS_MISSING;
            }
        }

    } catch (std::exception &e) {
        ms = media::MediaStatus::MS_UNREADABLE;
    }
    return ms;
}

} // namespace


ScannerActor::ScannerActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {

    system().registry().put(scanner_registry, this);

    behavior_.assign([=](media::media_status_atom, const MediaReference &mr, caf::actor dest) {
        anon_send(dest, media::media_status_atom_v, check_media_status(mr));
    });
}

void ScannerActor::on_exit() { system().registry().erase(scanner_registry); }
