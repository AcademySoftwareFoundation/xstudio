// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <caf/policy/select_all.hpp>
#include <caf/actor_registry.hpp>
#include <filesystem>
#include <openssl/md5.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>

#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/scanner/scanner_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/sequence.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::scanner;
using namespace caf;
using namespace xstudio::global_store;
using namespace xstudio::json_store;

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

    // spdlog::warn("check_media_status {} {}", to_string(mr.uri()), static_cast<int>(ms));
    return ms;
}


uintmax_t get_file_size(const fs::path &path) {
    uintmax_t result = 0;

    try {
        result = fs::file_size(path);
    } catch (...) {
    }
    return result;
}

fs::file_time_type get_modified_time(const fs::path &path) {
    try {
        return fs::last_write_time(path);
    } catch (...) {
        return fs::file_time_type::min();
    }
}

std::string get_checksum(const fs::path &path) {
    std::array<unsigned char, MD5_DIGEST_LENGTH> hash;

    // read first and last 1k..
    std::string buf(2048, ' ');
    // open file..
    std::ifstream myfile;
    try {
        myfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        myfile.open(path, std::ios::in | std::ios::binary);
        auto short_file = false;

        try {
            myfile.read((char *)buf.data(), 1024);
        } catch (...) {
            // shortfile..
            short_file = true;
        }
        if (not short_file) {
            myfile.seekg(-1024, std::ios::end);
            myfile.read((char *)buf.data() + 1024, 1024);
        }

        myfile.close();

    } catch (const std::exception &err) {
        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, path.string(), err.what());
        return std::string();
    }

    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, buf.c_str(), buf.size());
    MD5_Final(hash.data(), &md5);

    std::stringstream ss;

    for (unsigned char i : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
    }

    return ss.str();
}

MediaReference rescan_media_reference(MediaReference mr) {
    // only deal with frames..
    if (not mr.container()) {
        // get dir..
        auto fl = FrameList(mr.uri());
        if (fl != mr.frame_list()) {
            mr.set_frame_list(fl);
            mr.set_timecode_from_frames();
        }
    }

    return mr;
}

} // namespace

std::pair<std::string, uintmax_t>
ScanHelperActor::checksum_for_path(const std::filesystem::path &path) {
    const auto size = get_file_size(path);
    if (!size)
        return std::make_pair(std::string(), 0);

    const auto modified_at = get_modified_time(path);
    const auto cache_key   = path.string();
    const auto cached      = cache_.find(cache_key);
    if (cached != cache_.end() && cached->second.size_ == size &&
        cached->second.modified_at_ == modified_at) {
        return std::make_pair(cached->second.checksum_, cached->second.size_);
    }

    auto checksum = get_checksum(path);
    cache_[cache_key] = ChecksumCacheEntry{checksum, size, modified_at};
    return std::make_pair(std::move(checksum), size);
}

ScanHelperActor::ScanHelperActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    behavior_.assign(
        [=](media::checksum_atom,
            const MediaReference &mr) -> result<std::pair<std::string, uintmax_t>> {
            int frame;
            auto urlpath = mr.uri(0, frame);
            if (not urlpath)
                return make_error(xstudio_error::error, "Invalid url");

            return checksum_for_path(uri_to_posix_path(*urlpath));
        },

        [=](media::checksum_atom,
            const caf::uri &uri) -> result<std::pair<std::string, uintmax_t>> {
            return checksum_for_path(uri_to_posix_path(uri));
        },

        [=](media::rescan_atom, const MediaReference &mr) -> result<MediaReference> {
            auto rp = make_response_promise<MediaReference>();
            try {
                rp.deliver(rescan_media_reference(mr));
            } catch (const std::exception &err) {
                rp.deliver(make_error(xstudio_error::error, err.what()));
            }
            return rp;
        },

        [=](media::relink_atom,
            const media::MediaSourceChecksum &pin,
            const caf::uri &uri,
            const bool loose_match) -> result<caf::uri> {
            // recursively scan directory
            // look for size match.
            // then checksum
            // cache any checksums we create..
            auto path = uri_to_posix_path(uri);
            auto cpin = std::make_pair(std::get<1>(pin), std::get<2>(pin));
            const auto iter_options = fs::directory_options::skip_permission_denied;

            try {
                for (const auto &entry : fs::recursive_directory_iterator(path, iter_options)) {
                    try {
                        if (fs::is_regular_file(entry.status())) {
                            // check we've not alredy got it in cache..
#ifdef _WIN32
                            const auto puri = posix_path_to_uri(entry.path().string());
#else
                            const auto puri = posix_path_to_uri(entry.path());
#endif

                            if (get_file_size(entry.path()) != cpin.second)
                                continue;

                            const auto [checksum, size] = checksum_for_path(entry.path());
                            if (size == cpin.second && checksum == cpin.first)
                                return puri;
                        }
                    } catch (...) {
                    }
                }
                if (loose_match) {
                    for (const auto &entry : fs::recursive_directory_iterator(path, iter_options)) {
                        try {
                            if (fs::is_regular_file(entry.status())) {
                                // check we've not alredy got it in cache..
#ifdef _WIN32
                                const auto puri = posix_path_to_uri(entry.path().string());
#else
                                const auto puri = posix_path_to_uri(entry.path());
#endif

                                if (entry.path().filename() == std::get<0>(pin))
                                    return puri;
                            }
                        } catch (...) {
                        }
                    }
                }
            } catch (...) {
            }
            return caf::uri();
        });
}


ScannerActor::ScannerActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    auto helper_count = std::max<size_t>(1, std::min<size_t>(std::thread::hardware_concurrency(), 4));
    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        prefs.get_group(js);
        helper_count =
            std::clamp<size_t>(preference_value<size_t>(js, "/core/scanner/max_worker_count"), 1, 16);
    } catch (...) {
    }

    helpers_.reserve(helper_count);
    for (size_t helper_index = 0; helper_index < helper_count; ++helper_index) {
        auto helper = spawn<ScanHelperActor>();
        link_to(helper);
        helpers_.emplace_back(std::move(helper));
    }

    system().registry().put(scanner_registry, this);

    behavior_.assign(
        [=](media::media_status_atom, const MediaReference &mr, caf::actor dest) {
            anon_mail(media::media_status_atom_v, check_media_status(mr)).send(dest);
        },

        [=](media::checksum_atom atom, const caf::actor &media_source) {
            mail(media::media_reference_atom_v)
                .request(media_source, infinite)
                .then(
                    [=](const MediaReference &result) mutable {
                        anon_mail(atom, media_source, result)
                            .send(caf::actor_cast<caf::actor>(this));
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::checksum_atom atom,
            const caf::actor &media_source,
            const MediaReference &mr) {
            auto helper = next_helper();
            if (!helper)
                return;
            mail(atom, mr)
                .request(helper, infinite)
                .then(
                    [=](const std::pair<std::string, uintmax_t> &result) mutable {
                        anon_mail(atom, result).send(media_source);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::rescan_atom atom, const MediaReference &mr) {
            auto helper = next_helper();
            if (!helper)
                return make_error(sec::runtime_error, "No scanner workers");
            return mail(atom, mr).delegate(helper);
        },

        [=](media::checksum_atom atom, const MediaReference &mr) {
            auto helper = next_helper();
            if (!helper)
                return make_error(sec::runtime_error, "No scanner workers");
            return mail(atom, mr).delegate(helper);
        },

        [=](media::relink_atom atom,
            const media::MediaSourceChecksum &pin,
            const caf::uri &path,
            const bool loose_match) {
            auto helper = next_helper();
            if (!helper)
                return make_error(sec::runtime_error, "No scanner workers");
            return mail(atom, pin, path, loose_match).delegate(helper);
        },

        [=](media::relink_atom atom,
            const caf::actor &media_source,
            const caf::uri &path,
            const bool loose_match) {
            mail(media::checksum_atom_v)
                .request(media_source, infinite)
                .then(
                    [=](const media::MediaSourceChecksum &result) mutable {
                        anon_mail(atom, media_source, result, path, loose_match)
                            .send(caf::actor_cast<caf::actor>(this));
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::relink_atom atom,
            const caf::actor &media_source,
            const media::MediaSourceChecksum &pin,
            const caf::uri &path,
            const bool loose_match) {
            auto helper = next_helper();
            if (!helper)
                return;
            mail(atom, pin, path, loose_match)
                .request(helper, infinite)
                .then(
                    [=](const caf::uri &result) mutable {
                        if (not result.empty()) {
                            // get mr, and then over write..
                            mail(media::media_reference_atom_v)
                                .request(media_source, infinite)
                                .then(
                                    [=](MediaReference mr) mutable {
                                        if (not mr.container()) {
                                            // string frames...
                                            // special handing required as frame number /
                                            // padding might have changed.
                                            auto tmp = uri_from_file(uri_to_posix_path(result));
                                            if (not tmp.empty())
                                                mr.set_uri(tmp[0].first);
                                        } else
                                            mr.set_uri(result);
                                        anon_mail(media::media_reference_atom_v, mr)
                                            .send(media_source);
                                    },
                                    [=](const caf::error &err) {
                                        spdlog::warn(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    });
                        }
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        });
}

caf::actor ScannerActor::next_helper() {
    if (helpers_.empty())
        return caf::actor{};

    const auto helper_index = next_helper_index_ % helpers_.size();
    next_helper_index_      = (helper_index + 1) % helpers_.size();
    return helpers_[helper_index];
}

void ScannerActor::on_exit() { system().registry().erase(scanner_registry); }


// CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, checksum_atom)
// CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, relink_atom)
