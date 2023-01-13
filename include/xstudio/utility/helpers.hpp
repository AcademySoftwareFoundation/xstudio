// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdio>
#include <filesystem>

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <regex>
#include <iterator>
#include <cstdlib>

#include <caf/all.hpp>
#include <caf/uri_builder.hpp>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/caf_error.hpp"

namespace xstudio {
namespace utility {
    const std::array supported_extensions{".AAF",  ".AIFF", ".AVI", ".CIN", ".DPX", ".EXR",
                                          ".GIF",  ".JPEG", ".JPG", ".MKV", ".MOV", ".MPG",
                                          ".MPEG", ".MP3",  ".MP4", ".MXF", ".PNG", ".PPM",
                                          ".TIF",  ".TIFF", ".WAV"};

    void join_event_group(caf::event_based_actor *source, caf::actor actor);
    void leave_event_group(caf::event_based_actor *source, caf::actor actor);
    void join_broadcast(caf::event_based_actor *source, caf::actor actor);
    void leave_broadcast(caf::event_based_actor *source, caf::actor actor);

    void join_broadcast(caf::blocking_actor *source, caf::actor actor);
    void leave_broadcast(caf::blocking_actor *source, caf::actor actor);

    std::vector<std::byte> read_file(const std::string &path);

    inline auto make_get_event_group_handler(caf::actor grp) {
        return [=](get_event_group_atom) -> caf::actor { return grp; };
    }

    namespace fs = std::filesystem;


    inline bool check_create_path(const std::string &path) {
        bool create_path = true;
        bool success     = true;

        try {
            if (fs::exists(path))
                create_path = false;
        } catch (...) {
        }

        if (create_path) {
            try {
                if (not fs::create_directories(path))
                    success = false;
            } catch (const std::exception &err) {
                spdlog::warn("Failed to create {} {}", path, err.what());
                success = false;
            }
        }

        return success;
    }


    inline std::vector<caf::byte> hex_to_bytes(const std::string &hex) {
        std::vector<caf::byte> bytes;

        for (unsigned int i = 0; i < hex.length(); i += 2) {
            bytes.push_back(
                static_cast<caf::byte>(strtol(hex.substr(i, 2).c_str(), nullptr, 16)));
        }

        return bytes;
    }

    template <typename R, typename... Ts>
    R request_receive_wait(
        caf::blocking_actor &src,
        caf::actor dest,
        const caf::timespan &wait_for,
        Ts const &...args) {
        R result{};
        src.request(dest, wait_for, args...)
            .receive(
                [&result](const R &res) mutable { result = std::move(res); },
                [=](const caf::error &e) { throw XStudioError(e); });

        return result;
    }

    template <typename R, typename... Ts>
    R request_receive(caf::blocking_actor &src, caf::actor dest, Ts const &...args) {
        // spdlog::warn("REQUEST");
        R result{};
        src.request(dest, caf::infinite, args...)
            .receive(
                [&result](const R &res) mutable {
                    // spdlog::error("RECEIVE");
                    result = std::move(res);
                },
                [=](const caf::error &e) {
                    // spdlog::error("RECEIVE");
                    throw XStudioError(e);
                });

        return result;
    }

    template <typename R, typename... Ts>
    R request_receive_high_priority(
        caf::blocking_actor &src, caf::actor dest, Ts const &...args) {
        R result{};
        src.request<caf::message_priority::high>(dest, caf::infinite, args...)
            .receive(
                [&result](const R &res) mutable { result = std::move(res); },
                [=](const caf::error &e) { throw XStudioError(e); });

        return result;
    }

    inline void print_on_create(const caf::actor &hdl, const Container &cont) {
        spdlog::debug(
            "{} {} {} created {}",
            cont.type(),
            cont.name(),
            to_string(cont.uuid()),
            to_string(hdl));
    }

    inline void print_on_create(const caf::actor &hdl, const std::string &name) {
        spdlog::debug("{} created {}", name, to_string(hdl));
    }

    inline void print_on_exit(const caf::actor &hdl, const Container &cont) {
        hdl->attach_functor([=](const caf::error &reason) {
            spdlog::debug(
                "{} {} {} exited: {}",
                cont.type(),
                cont.name(),
                to_string(cont.uuid()),
                to_string(reason));
        });
    }

    inline void print_on_exit(
        const caf::actor &hdl, const std::string &name, const Uuid &uuid = utility::Uuid()) {
        hdl->attach_functor([=](const caf::error &reason) {
            spdlog::debug(
                "{} {} exited: {}",
                name,
                uuid.is_null() ? "" : to_string(uuid),
                to_string(reason));
        });
    }

    std::string exec(const std::vector<std::string> &cmd, int &exit_code);

    std::string filemanager_show_uris(const std::vector<caf::uri> &uris);

    caf::uri posix_path_to_uri(const std::string &path, const bool abspath = false);

    caf::uri parse_cli_posix_path(
        const std::string &path, FrameList &frame_list, const bool scan = false);
    void validate_media(const caf::uri &uri, const FrameList &frame_list);
    std::vector<caf::uri>
    uri_framelist_as_sequence(const caf::uri &uri, const FrameList &frame_list);


    bool check_plugin_uri_request(const std::string &request);

    // used due to bug in caf, which should be fixed in the next release..
    inline caf::uri url_to_uri(const std::string &url) {
        auto uri = caf::make_uri(url);
        if (uri)
            return *uri;

        // }
        // std::string _url = url;
        // if(url.find("file:///") == 0){
        //  _url = "file:/" + url.substr(8);
        // }
        // caf::uri uri;
        // caf::parse(_url, uri);
        return caf::uri();
    }

    //  DIRTY REPLACE (does caf support this?)
    inline std::string uri_decode(const std::string eString) {
        std::string ret;
        char ch;
        unsigned int i, j;
        for (i = 0; i < eString.length(); i++) {
            if (int(eString[i]) == 37) {
                sscanf(eString.substr(i + 1, 2).c_str(), "%x", &j);
                ch = static_cast<char>(j);
                ret += ch;
                i = i + 2;
            } else {
                ret += eString[i];
            }
        }
        return (ret);
    }

    // this is WRONG on purpose, as caf::uri are buggy.
    // the path component needs to be escaped, even when it's a file::
    inline std::string uri_encode(const std::string &s) {
        std::string result;
        result.reserve(s.size());
        auto params = false;
        std::array<char, 4> hex;

        for (size_t i = 0; s[i]; i++) {
            switch (s[i]) {
            case ' ':
                result += "%20";
                break;
            case '+':
                result += "%2B";
                break;
            case '\r':
                result += "%0D";
                break;
            case '\n':
                result += "%0A";
                break;
            case '\'':
                result += "%27";
                break;
            case ',':
                result += "%2C";
                break;
            // case ':': result += "%3A"; break; // ok? probably...
            case ';':
                result += "%3B";
                break;
            default:
                auto c = static_cast<uint8_t>(s[i]);
                if (c == '?')
                    params = true;

                if (not params and c == '&') {
                    result += "%26";
                } else if (c >= 0x80) {
                    result += '%';
                    auto len = snprintf(hex.data(), hex.size() - 1, "%02X", c);
                    assert(len == 2);
                    result.append(hex.data(), static_cast<size_t>(len));
                } else {
                    result += s[i];
                }
                break;
            }
        }

        return result;
    }

    inline std::string uri_to_posix_path(const caf::uri &uri) {
        if (uri.path().data()) {
            std::string path = uri_decode(uri.path().data());
            if (not path.empty() and path[0] != '/' and not uri.authority().empty()) {
                path = "/" + path;
            }
            return path;
        }
        return "";
    }

    // can only get signature for posix urls..
    inline std::array<uint8_t, 16> get_signature(const caf::uri &uri) {
        std::array<uint8_t, 16> sig{};
        // read header.. caller myse use try block to catch errors
        if (to_string(uri.scheme()) != "file")
            return sig;

        std::ifstream myfile;
        try {
            myfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            myfile.open(uri_to_posix_path(uri), std::ios::in | std::ios::binary);
            myfile.read((char *)sig.data(), sig.size());
            myfile.close();
        } catch (...) {
            // seems like ifstream exceptions are next to useless for diagnosing
            // why a file couldn't be read, so just using generic error for now
            std::stringstream ss;
            ss << "Unable to read " << to_string(uri);
            throw std::runtime_error(ss.str().c_str());
        }
        return sig;
    }

    inline std::string xstudio_root(const std::string &append_path) {
        auto root = get_env("XSTUDIO_ROOT");
        std::string path =
            (root ? (*root) + append_path : std::string(BINARY_DIR) + append_path);
        return path;
    }

    inline std::string remote_session_path() {
        auto root        = get_env("HOME");
        std::string path = (root ? (*root) + "/.config/DNEG/xstudio/sessions" : "");
        return path;
    }

    inline std::string preference_path(const std::string &append_path = "") {
        auto root = get_env("HOME");
        std::string path =
            (root ? (*root) + "/.config/DNEG/xstudio/preferences/" + append_path : "");
        return path;
    }

    inline std::string snippets_path(const std::string &append_path = "") {
        auto root = get_env("HOME");
        std::string path =
            (root ? (*root) + "/.config/DNEG/xstudio/snippets/" + append_path : "");
        return path;
    }

    inline std::string preference_path_context(const std::string &context) {
        return preference_path(to_lower(context) + ".json");
    }

    inline bool are_same(float a, float b, int decimals) {
        return std::abs(a - b) < (1.0 / std::pow(10, decimals));
    }

    std::vector<std::pair<caf::uri, FrameList>>
    scan_posix_path(const std::string &path, const int depth = -1);

    std::string get_host_name();
    std::string get_user_name();
    std::string get_login_name();

    inline fs::file_time_type get_file_mtime(const std::string path) {
        fs::file_time_type mtim = fs::file_time_type::min();
        try {
            mtim = fs::last_write_time(path);
        } catch (...) {
        }
        return mtim;
    }

    inline fs::file_time_type get_file_mtime(const caf::uri &path) {
        return get_file_mtime(uri_to_posix_path(path));
    }


    inline bool is_file_supported(const caf::uri &uri) {
        fs::path p(uri_to_posix_path(uri));
        std::string ext = to_upper(p.extension());
        for (const auto &i : supported_extensions)
            if (i == ext)
                return true;
        return false;
    }

    std::string expand_envvars(
        const std::string &src, const std::map<std::string, std::string> &additional = {});


    template <typename M> std::vector<typename M::key_type> map_key_to_vec(const M &m) {
        std::vector<typename M::key_type> result;
        result.reserve(m.size());
        for (auto it = m.begin(); it != m.end(); ++it) {
            result.push_back(it->first);
        }
        return result;
    }

    template <typename M> std::vector<typename M::mapped_type> map_value_to_vec(const M &m) {
        std::vector<typename M::mapped_type> result;
        result.reserve(m.size());
        for (auto it = m.begin(); it != m.end(); ++it) {
            result.push_back(it->second);
        }
        return result;
    }

    template <typename V>
    std::vector<typename V::value_type::first_type> vpair_first_to_v(const V &v) {
        std::vector<typename V::value_type::first_type> result;
        result.reserve(v.size());
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.push_back(it->first);
        }
        return result;
    }

    template <typename V>
    std::vector<typename V::value_type::second_type> vpair_second_to_v(const V &v) {
        std::vector<typename V::value_type::second_type> result;
        result.reserve(v.size());
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.push_back(it->second);
        }
        return result;
    }

    template <typename V> std::vector<caf::actor> vector_to_caf_actor_vector(const V &v) {
        std::vector<caf::actor> result;
        result.reserve(v.size());
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.push_back(static_cast<caf::actor>(*it));
        }
        return result;
    }

    template <typename V>
    std::set<typename V::value_type::first_type> vpair_first_to_s(const V &v) {
        std::set<typename V::value_type::first_type> result;
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.insert(it->first);
        }
        return result;
    }

    template <typename V>
    std::set<typename V::value_type::second_type> vpair_second_to_s(const V &v) {
        std::set<typename V::value_type::second_type> result;
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.insert(it->second);
        }
        return result;
    }

    template <typename V>
    std::map<typename V::value_type::first_type, typename V::value_type::second_type>
    vpair_to_map(const V &v) {
        std::map<typename V::value_type::first_type, typename V::value_type::second_type>
            result;
        for (auto it = v.begin(); it != v.end(); ++it) {
            result[it->first] = it->second;
        }
        return result;
    }

    // for vector<class> that can be cast actor or uuid (e.g. UuidActor !)
    template <typename V>
    std::map<utility::Uuid, caf::actor> uuidactor_vect_to_map(const V &v) {
        std::map<utility::Uuid, caf::actor> result;
        for (auto it = v.begin(); it != v.end(); ++it) {
            result[static_cast<utility::Uuid>(*it)] = static_cast<caf::actor>(*it);
        }
        return result;
    }

    // for vector<class> that can be cast actor or uuid (e.g. UuidActor !)
    template <typename V> std::set<utility::Uuid> uuidactor_vect_to_uuid_set(const V &v) {
        std::set<utility::Uuid> result;
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.insert(static_cast<utility::Uuid>(*it));
        }
        return result;
    }

} // namespace utility
} // namespace xstudio