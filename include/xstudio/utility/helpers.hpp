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
#include "xstudio/caf_utility/caf_setup.hpp"
#ifdef _WIN32
#include <windows.h>
#include <fmt/format.h>
#endif

namespace xstudio {
namespace utility {

    /* This class provides a static method for getting a reference to the
    actor system. The same method takes an actor system ref as an argument.
    If its the first time the method is called, it copies the ref and stores
    it. On subsequent calls it ignores the argument and returns the ref that
    was stored on the first call.

    It's a bit clumsy but so far the only solution for making the xSTUDIO
    application actor system visible to the python module when the python
    module is instanced. If the python module is instanced *outside* of an
    xSTUDIO application (i.e. in an external python interpreter) then it
    will use the fallback system local to the python module.

    We need this so that actors created by the python module are in the same
    system as the application (for the embedded interpreter case) - this is
    the only way to trigger python callbacks in message handler of the
    embedded python actor.
    */
    class ActorSystemSingleton {

      public:
        static caf::actor_system &actor_system_ref();
        static caf::actor_system &actor_system_ref(caf::actor_system &sys);

      private:
        ActorSystemSingleton(caf::actor_system &provided_sys) : system_ref_(provided_sys) {}

        caf::actor_system &get_system() { return system_ref_.get(); }

        std::reference_wrapper<caf::actor_system> system_ref_;
    };

    const std::array supported_extensions{".AAF",  ".AIFF", ".AVI", ".CIN", ".DPX", ".EXR",
                                          ".GIF",  ".JPEG", ".JPG", ".MKV", ".MOV", ".MPG",
                                          ".MPEG", ".MP3",  ".MP4", ".MXF", ".PNG", ".PPM",
                                          ".TIF",  ".TIFF", ".WAV", ".WEBM"};

    const std::array supported_timeline_extensions{".OTIO", ".XML", ".EDL"};

    const std::array session_extensions{".XST", ".XSZ"};


    std::string actor_to_string(caf::actor_system &sys, const caf::actor &actor);
    caf::actor actor_from_string(caf::actor_system &sys, const std::string &str_addr);

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

    // Centralizing the Path to String conversions in case we run into encoding problems down the line.
    inline std::string path_to_string(fs::path path) {
#ifdef _WIN32
        return path.string();
#else
        // Implicit cast works fine on Linux
        return path;
#endif
    }


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
        const caf::actor &dest,
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
    R request_receive(caf::blocking_actor &src, const caf::actor &dest, Ts const &...args) {
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
        caf::blocking_actor &src, const caf::actor &dest, Ts const &...args) {
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

    void print_on_exit(const caf::actor &hdl, const Container &cont);

    void print_on_exit(
        const caf::actor &hdl, const std::string &name, const Uuid &uuid = utility::Uuid());

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
    inline std::string url_clean(const std::string &url) {
        const std::regex xstudio_shake(
            R"(^(.+\.)([#@]+)(\..+?)(=([-0-9x,]+))?$)", std::regex::optimize);

        auto clean = url;
        std::cmatch m;

        if (std::regex_match(clean.c_str(), m, xstudio_shake)) {
            size_t pad_c = 0;
            if (m[2].str() == "#") {
                pad_c = 4;
            } else {
                pad_c = m[2].str().size();
            }

            clean = m[1].str() + "{:0" + std::to_string(pad_c) + "d}" + m[3].str();
        }

        return clean;
    }

    //  DIRTY REPLACE (does caf support this?)
    std::string uri_decode(const std::string &eString);
    // this is WRONG on purpose, as caf::uri are buggy.
    // the path component needs to be escaped, even when it's a file::
    std::string uri_encode(const std::string &s);
    std::string uri_to_posix_path(const caf::uri &uri);

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

        std::string fallback_root;
        #ifdef _WIN32
        char filename[MAX_PATH];
        DWORD nSize  = _countof(filename);
        DWORD result = GetModuleFileNameA(NULL, filename, nSize);
        if (result == 0) {
            spdlog::debug("Unable to determine executable path from Windows API, falling back "
                          "to standard methods");
        } else {
            auto exePath = fs::path(filename);

            // The first parent path gets us to the bin directory, the second gets us to the level above bin.
            auto xstudio_root = exePath.parent_path().parent_path();
            fallback_root     = xstudio_root.string();
        }
        #else
        //TODO: This could inspect the current running process and look one directory up.
        fallback_root   = std::string(BINARY_DIR);
        #endif


        std::string path =
            (root ? (*root) + append_path : fallback_root + append_path);

        return path;
    }

inline std::string remote_session_path() {
    const char* root;
#ifdef _WIN32
    root = std::getenv("USERPROFILE");
#else
    root = std::getenv("HOME");
#endif
    std::filesystem::path path;
    if (root)
    {
        path = std::filesystem::path(root) / ".config" / "DNEG" / "xstudio" / "sessions";
    }

    return path.string();
}

inline std::string preference_path(const std::string &append_path = "") {
    const char *root;
#ifdef _WIN32
    root = std::getenv("USERPROFILE");
#else
    root          = std::getenv("HOME");
#endif
    std::filesystem::path path;
    if (root) {
        path = std::filesystem::path(root) / ".config" / "DNEG" / "xstudio" / "preferences";
        if (!append_path.empty()) {
            path /= append_path;
        }
    }

    return path.string();
}

inline std::string snippets_path(const std::string &append_path = "") {
    const char *root;
#ifdef _WIN32
    root = std::getenv("USERPROFILE");
#else
    root          = std::getenv("HOME");
#endif
    std::filesystem::path path;
    if (root) {
        path = std::filesystem::path(root) / ".config" / "DNEG" / "xstudio" / "snippets";
        if (!append_path.empty()) {
            path /= append_path;
        }
    }

    return path.string();
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
        } catch (const std::exception &err) {
            spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return mtim;
    }

    inline fs::file_time_type get_file_mtime(const caf::uri &path) {
        return get_file_mtime(uri_to_posix_path(path));
    }

    inline std::string get_path_extension(const fs::path p)
    {
        const std::string sp = p.string();
#ifdef _WIN32
        std::string sanitized;

        try {
            sanitized = fmt::format(sp, 0);

        } catch (...) {
            // If we are here, the path likely doesn't have a format string.
            sanitized = sp;
        }
        fs::path pth(sanitized);
        std::string ext = pth.extension().string(); // Convert path extension to string
        return ext;
#else
        return p.extension().string();
#endif
    }


    inline bool is_file_supported(const caf::uri &uri) {
        const std::string sp = uri_to_posix_path(uri);
        std::string ext      = to_upper(get_path_extension(fs::path(sp)));

        for (const auto &i : supported_extensions)
            if (i == ext)
                return true;
        return false;
    }

    inline bool is_session(const std::string &path) {
        fs::path p(path);
        std::string ext = to_upper(path_to_string(get_path_extension(p)));
        for (const auto &i : session_extensions)
            if (i == ext)
                return true;
        return false;
    }

    inline bool is_session(const caf::uri &path) { return is_session(uri_to_posix_path(path)); }

    inline bool is_timeline_supported(const caf::uri &uri) {
        fs::path p(uri_to_posix_path(uri));
        spdlog::error(p.string());
        std::string ext = to_upper(get_path_extension(p));

        for (const auto &i : supported_timeline_extensions)
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

    //  this is annoying.. we now have to create all these silly UuidActor functions..


    // for vector<class> that can be cast actor or uuid (e.g. UuidActor !)
    template <typename V> std::vector<caf::actor> vector_to_caf_actor_vector(const V &v) {
        std::vector<caf::actor> result;
        result.reserve(v.size());
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.push_back(static_cast<caf::actor>(*it));
        }
        return result;
    }
    // for vector<class> that can be cast actor or uuid (e.g. UuidActor !)
    template <typename V> std::vector<utility::Uuid> vector_to_uuid_vector(const V &v) {
        std::vector<utility::Uuid> result;
        result.reserve(v.size());
        for (auto it = v.begin(); it != v.end(); ++it) {
            result.push_back(static_cast<utility::Uuid>(*it));
        }
        return result;
    }

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
