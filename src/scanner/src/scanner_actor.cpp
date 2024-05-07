// SPDX-License-Identifier: Apache-2.0
#include <caf/policy/select_all.hpp>
#include <filesystem>
#include <openssl/md5.h>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/scanner/scanner_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/sequence.hpp"

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

    } catch ([[maybe_unused]] std::exception &e) {
        ms = media::MediaStatus::MS_UNREADABLE;
    }

    // spdlog::warn("check_media_status {} {}", to_string(mr.uri()), static_cast<int>(ms));
    return ms;
}


uintmax_t get_file_size(const std::string &path) {
    uintmax_t result = 0;

    try {
        result = fs::file_size(path);
    } catch (...) {
    }
    return result;
}

std::string get_checksum(const std::string &path) {
    std::array<unsigned char, MD5_DIGEST_LENGTH> hash;

    // read first and last 1k..
    std::string buf(2048, ' ');
    // open file..
    std::ifstream myfile;
    try {
        myfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        myfile.open(path, std::ios::in | std::ios::binary);
        myfile.read((char *)buf.data(), 1024);
        myfile.seekg(-1024, std::ios::end);
        myfile.read((char *)buf.data() + 1024, 1024);
        myfile.close();
    } catch (const std::exception &err) {
        spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, path, err.what());
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

ScanHelperActor::ScanHelperActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    behavior_.assign(
        [=](media::checksum_atom,
            const MediaReference &mr) -> result<std::pair<std::string, uintmax_t>> {
            int frame;
            auto urlpath = mr.uri(0, frame);
            if (not urlpath)
                return make_error(xstudio_error::error, "Invalid url");

            auto path = uri_to_posix_path(*urlpath);

            auto size     = get_file_size(path);
            auto checksum = std::string();

            if (size)
                checksum = get_checksum(path);

            return std::make_pair(checksum, size);
        },

        [=](media::checksum_atom,
            const caf::uri &uri) -> result<std::pair<std::string, uintmax_t>> {
            auto path     = uri_to_posix_path(uri);
            auto size     = get_file_size(path);
            auto checksum = std::string();

            if (size)
                checksum = get_checksum(path);
            return std::make_pair(checksum, size);
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
            const std::pair<std::string, uintmax_t> &pin,
            const caf::uri &uri) -> result<caf::uri> {
            // recursively scan directory
            // look for size match.
            // then checksum
            // cache any checksums we create..
            auto path = uri_to_posix_path(uri);

            try {
                for (const auto &entry : fs::recursive_directory_iterator(path)) {
                    try {
                        if (fs::is_regular_file(entry.status())) {
                            // check we've not alredy got it in cache..
#ifdef _WIN32
                            const auto puri = posix_path_to_uri(entry.path().string());
#else
                            const auto puri = posix_path_to_uri(entry.path());
#endif

                            if (cache_.count(puri)) {
                                const auto &c = cache_.at(puri);
                                if (c == pin)
                                    return puri;
                            } else {
#ifdef _WIN32
                                auto size = get_file_size(entry.path().string());
#else
                                auto size = get_file_size(entry.path());
#endif
                                if (size == pin.second) {
#ifdef _WIN32
                                    auto checksum = get_checksum(entry.path().string());
#else
                                    auto checksum = get_checksum(entry.path());
#endif
                                    cache_[puri]  = std::make_pair(checksum, size);
                                    if (checksum == pin.first)
                                        return puri;
                                }
                            }
                        }
                    } catch (...) {
                    }
                }
            } catch (...) {
            }

            return caf::uri();
        });
}


ScannerActor::ScannerActor(caf::actor_config &cfg) : caf::event_based_actor(cfg) {
    auto helper = spawn<ScanHelperActor>();
    link_to(helper);

    system().registry().put(scanner_registry, this);

    behavior_.assign(
        [=](media::media_status_atom, const MediaReference &mr, caf::actor dest) {
            anon_send(dest, media::media_status_atom_v, check_media_status(mr));
        },

        [=](media::checksum_atom atom, const caf::actor &media_source) {
            request(media_source, infinite, media::media_reference_atom_v)
                .then(
                    [=](const MediaReference &result) mutable {
                        anon_send(
                            caf::actor_cast<caf::actor>(this), atom, media_source, result);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::checksum_atom atom,
            const caf::actor &media_source,
            const MediaReference &mr) {
            request(helper, infinite, atom, mr)
                .then(
                    [=](const std::pair<std::string, uintmax_t> &result) mutable {
                        anon_send(media_source, atom, result);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::rescan_atom atom, const MediaReference &mr) { delegate(helper, atom, mr); },

        [=](media::checksum_atom atom, const MediaReference &mr) {
            delegate(helper, atom, mr);
        },

        [=](media::relink_atom atom,
            const std::pair<std::string, uintmax_t> &pin,
            const caf::uri &path) { delegate(helper, atom, pin, path); },

        [=](media::relink_atom atom, const caf::actor &media_source, const caf::uri &path) {
            request(media_source, infinite, media::checksum_atom_v)
                .then(
                    [=](const std::pair<std::string, uintmax_t> &result) mutable {
                        anon_send(
                            caf::actor_cast<caf::actor>(this),
                            atom,
                            media_source,
                            result,
                            path);
                    },
                    [=](const caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](media::relink_atom atom,
            const caf::actor &media_source,
            const std::pair<std::string, uintmax_t> &pin,
            const caf::uri &path) {
            request(helper, infinite, atom, pin, path)
                .then(
                    [=](const caf::uri &result) mutable {
                        if (not result.empty()) {
                            // get mr, and then over write..
                            request(media_source, infinite, media::media_reference_atom_v)
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
                                        anon_send(
                                            media_source, media::media_reference_atom_v, mr);
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

void ScannerActor::on_exit() { system().registry().erase(scanner_registry); }


// CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, checksum_atom)
// CAF_ADD_ATOM(xstudio_session_atoms, xstudio::media, relink_atom)
