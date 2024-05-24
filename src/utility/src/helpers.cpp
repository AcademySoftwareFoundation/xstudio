// SPDX-License-Identifier: Apache-2.0
#ifdef __linux__
#define __USE_POSIX
#include <unistd.h>
#include <sys/param.h>
#include <pwd.h>
#include <sys/types.h>
#endif
#include <filesystem>
#include <limits>
#include <regex>
#include <set>
#include <climits>

#include <fmt/format.h>

//#include <reproc++/drain.hpp>
//#include <reproc++/reproc.hpp>

#include "xstudio/utility/frame_list.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/sequence.hpp"
#include "xstudio/utility/string_helpers.hpp"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

using namespace xstudio::utility;
using namespace caf;
namespace fs = std::filesystem;

// namespace caf {
// 	error_code<sec> inspect(caf::binary_deserializer& src, caf::uri& x) {
// 	  auto impl = make_counted<detail::uri_impl>();
// 	  auto err = inspect(src, *impl);
// 	  if (err == none)
// 	    x = uri{std::move(impl)};
// 	  return err;
// 	}
// }

static std::shared_ptr<ActorSystemSingleton> s_actor_system_singleton;

caf::actor_system &ActorSystemSingleton::actor_system_ref() {
    assert(s_actor_system_singleton);
    return s_actor_system_singleton->get_system();
}

caf::actor_system &ActorSystemSingleton::actor_system_ref(caf::actor_system &sys) {

    // Note that this function is called when instancing the xstudio python module and
    // allows it to grab a reference to the application actor system (if there is one -
    // which will be the case if we're in the embedded interpreter in xstudio). If the
    // python module is instanced outside of an xstudio application, there will be no
    // existing application actor system so essentially 'sys' is returned. In that case
    // 'sys' will be a reference to an actor system that is local to the python module
    // itself.

    if (!s_actor_system_singleton) {
        // no instance of ActorSystemSingleton exists yet, so create one and
        // grab a rederence to 'sys'
        s_actor_system_singleton.reset(new ActorSystemSingleton(sys));
    } else {
        // in this case, 'sys' is ignored/unused and we return the ref to previously
        // created actor_system. We do this to ensure that the python module, when
        // running in the embedded interpreter in the xstudio application, will spawn
        // actors within the application system.
    }
    return s_actor_system_singleton->get_system();
}

std::string xstudio::utility::actor_to_string(caf::actor_system &sys, const caf::actor &actor) {
    std::string result;

    try {
        caf::binary_serializer::container_type buf;
        caf::binary_serializer bs{sys, buf};

        auto e = bs.apply(caf::actor_cast<caf::actor_addr>(actor));
        if (not e)
            throw std::runtime_error(to_string(bs.get_error()));

        result = utility::make_hex_string(std::begin(buf), std::end(buf));
    } catch (const std::exception &err) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

caf::actor
xstudio::utility::actor_from_string(caf::actor_system &sys, const std::string &str_addr) {
    caf::actor actor;

    caf::binary_serializer::container_type buf = utility::hex_to_bytes(str_addr);
    caf::binary_deserializer bd{sys, buf};

    caf::actor_addr addr;
    auto e = bd.apply(addr);

    if (not e) {
        spdlog::debug("{} {}", __PRETTY_FUNCTION__, to_string(bd.get_error()));
    } else {
        actor = caf::actor_cast<caf::actor>(addr);
    }

    return actor;
}

void xstudio::utility::join_broadcast(caf::event_based_actor *source, caf::actor actor) {

    source->request(actor, caf::infinite, broadcast::join_broadcast_atom_v)
        .then(
            [=](const bool) mutable {},
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void xstudio::utility::join_broadcast(caf::blocking_actor *source, caf::actor actor) {
    source->request(actor, caf::infinite, broadcast::join_broadcast_atom_v)
        .receive(
            [=](const bool) mutable {},
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void xstudio::utility::leave_broadcast(caf::blocking_actor *source, caf::actor actor) {
    source->request(actor, caf::infinite, broadcast::leave_broadcast_atom_v)
        .receive(
            [=](const bool) mutable {},
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void xstudio::utility::leave_broadcast(caf::event_based_actor *source, caf::actor actor) {
    source->request(actor, caf::infinite, broadcast::leave_broadcast_atom_v)
        .then(
            [=](const bool) mutable {},
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void xstudio::utility::join_event_group(caf::event_based_actor *source, caf::actor actor) {
    source->request(actor, caf::infinite, utility::get_event_group_atom_v)
        .then(
            [=](caf::actor grp) mutable {
                source->request(grp, caf::infinite, broadcast::join_broadcast_atom_v)
                    .then(
                        [=](const bool) mutable {},
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

void xstudio::utility::leave_event_group(caf::event_based_actor *source, caf::actor actor) {
    source->request(actor, caf::infinite, utility::get_event_group_atom_v)
        .then(
            [=](caf::actor grp) mutable {
                source->request(grp, caf::infinite, broadcast::leave_broadcast_atom_v)
                    .then(
                        [=](const bool) mutable {},
                        [=](const error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](const error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}


void xstudio::utility::print_on_exit(const caf::actor &hdl, const Container &cont) {
    hdl->attach_functor([=](const caf::error &reason) {
        spdlog::debug(
            "{} {} {} exited: {}",
            cont.type(),
            cont.name(),
            to_string(cont.uuid()),
            to_string(reason));
    });
}

void xstudio::utility::print_on_exit(
    const caf::actor &hdl, const std::string &name, const Uuid &uuid) {

    hdl->attach_functor([=](const caf::error &reason) {
        spdlog::debug(
            "{} {} exited: {}", name, uuid.is_null() ? "" : to_string(uuid), to_string(reason));
    });
}

std::string xstudio::utility::exec(const std::vector<std::string> &cmd, int &exit_code) {
    /*reproc::process process;
    std::error_code ec = process.start(cmd);

    if (ec == std::errc::no_such_file_or_directory) {
        exit_code = ec.value();
        return "Program not found. Make sure it's available from the PATH.";
    } else if (ec) {
        exit_code = ec.value();
        return ec.message();
    }

    std::string output;
    reproc::sink::string sink(output);

    ec = reproc::drain(process, sink, reproc::sink::null);
    if (ec) {
        exit_code = ec.value();
        return ec.message();
    }

    // int status = 0;
    std::tie(exit_code, ec) = process.wait(reproc::infinite);
    if (ec) {
        exit_code = ec.value();
        return ec.message();
    }

    return output;*/
    return std::string();
}

std::string xstudio::utility::uri_to_posix_path(const caf::uri &uri) {
    if (uri.path().data()) {
        // spdlog::warn("{} {}",uri.path().data(), uri_decode(uri.path().data()));
        std::string path = uri_decode(uri.path().data());
#ifdef __linux__
        if (not path.empty() and path[0] != '/' and not uri.authority().empty()) {
            path = "/" + path;
        }
#endif
#ifdef _WIN32

        std::size_t pos = path.find("/");
        if (pos == 0) {
            // Remove the leading /
            path.erase(0, 1);
        }
        /*
        // Remove the leading '[protocol]:' part
        std::size_t pos = path.find(":");
        if (pos != std::string::npos) {
            path.erase(0, pos + 1); // +1 to erase the colon
        }
        

        // Now, replace forward slashes with backslashes
        std::replace(path.begin(), path.end(), '/', '\\');
        */
#endif
        return path;
    }
    return "";
}

std::string xstudio::utility::uri_encode(const std::string &s) {
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


//  DIRTY REPLACE (does caf support this?)
std::string xstudio::utility::uri_decode(const std::string &eString) {
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


void xstudio::utility::validate_media(const caf::uri &uri, const FrameList &frame_list) {
    for (const auto &i : xstudio::utility::uri_framelist_as_sequence(uri, frame_list)) {
        if (not fs::exists(uri_to_posix_path(i)))
            throw std::runtime_error(
                fmt::format("File does not exist {}", uri_to_posix_path(i)));
    }
}

std::vector<caf::uri>
xstudio::utility::uri_framelist_as_sequence(const caf::uri &uri, const FrameList &frame_list) {
    std::vector<caf::uri> uris;

    if (frame_list.empty())
        uris.push_back(uri);
    else {
        auto fl = frame_list.frames();
        uris.reserve(fl.size());
        for (const auto i : fl) {
            auto new_uri =
                caf::make_uri(uri_encode(fmt::format(uri_decode(to_string(uri)), i)));
            if (not new_uri) {
                spdlog::warn(
                    "{} {} {}",
                    to_string(uri),
                    uri_decode(to_string(uri)),
                    uri_encode(fmt::format(uri_decode(to_string(uri)), i)));
                throw std::runtime_error(
                    "Invalid uri " + uri_encode(fmt::format(uri_decode(to_string(uri)), i)));
            }

            uris.push_back(*new_uri);
        }
    }

    return uris;
}


caf::uri xstudio::utility::parse_cli_posix_path(
    const std::string &path, FrameList &frame_list, const bool scan) {
    caf::uri uri;
    std::cmatch m;
    const std::regex xstudio_movie_spec(R"(^(.*)=([-0-9x,]+)$)", std::regex::optimize);
    const std::regex xstudio_spec(R"(^(.*\{.+\}.*?)(=([-0-9x,]+))?$)", std::regex::optimize);
    const std::regex xstudio_shake(
        R"(^(.+\.)([#@]+)(\..+?)(=([-0-9x,]+))?$)", std::regex::optimize);
    const std::regex xstudio_prefix_spec(
        R"(^(.*\.)([-0-9x,]+)(\{.+\}.*)$)", std::regex::optimize);
    const std::regex xstudio_prefix_shake(
        R"(^(.+\.)([-0-9x,]+)([#@]+)(\..+)$)", std::regex::optimize);

#ifdef _WIN32
    std::string abspath = path;
    if (abspath[0] == '\\') {
        abspath.erase(abspath.begin());
    }
#else
    const std::string abspath = fs::absolute(path);
#endif

    if (std::regex_match(abspath.c_str(), m, xstudio_prefix_spec)) {
        uri        = posix_path_to_uri(m[1].str() + m[3].str());
        frame_list = FrameList(m[2].str());
    } else if (std::regex_match(abspath.c_str(), m, xstudio_prefix_shake)) {
        size_t pad_c = 0;
        if (m[3].str() == "#") {
            pad_c = 4;
        } else {
            pad_c = m[3].str().size();
        }

        uri = posix_path_to_uri(m[1].str() + "{:0" + std::to_string(pad_c) + "d}" + m[4].str());
        frame_list = FrameList(m[2].str());
    } else if (std::regex_match(abspath.c_str(), m, xstudio_spec)) {
        uri = posix_path_to_uri(m[1].str());

        if (not m[3].str().empty()) {
            frame_list = FrameList(m[3].str());
        } else if (scan) {
            frame_list = FrameList(uri);
            if (frame_list.empty())
                throw std::runtime_error("No frames found.");
        } else {
            throw std::runtime_error("No frames specified.");
        }

    } else if (std::regex_match(abspath.c_str(), m, xstudio_shake)) {
        size_t pad_c = 0;
        if (m[2].str() == "#") {
            pad_c = 4;
        } else {
            pad_c = m[2].str().size();
        }

        uri = posix_path_to_uri(m[1].str() + "{:0" + std::to_string(pad_c) + "d}" + m[3].str());
        // spdlog::error("posix_path_to_uri {}", to_string(uri));

        if (not m[5].str().empty()) {
            frame_list = FrameList(m[5].str());
        } else if (scan) {
            frame_list = FrameList(uri);
        }

        if (frame_list.empty()) {
            throw std::runtime_error("No frames specified.");
        }
    } else if (std::regex_match(abspath.c_str(), m, xstudio_movie_spec)) {
        uri        = posix_path_to_uri(m[1].str());
        frame_list = FrameList(m[2].str());
    } else {
        frame_list.clear();
        uri = posix_path_to_uri(abspath);
    }

    return uri;
}

caf::uri xstudio::utility::posix_path_to_uri(const std::string &path, const bool abspath) {
    auto p = path;

    if (abspath) {
        auto pwd = get_env("PWD");
        if (pwd and not pwd->empty())
#ifdef _WIN32
            p = (fs::path(*pwd) / path).lexically_normal().string();
        else
	    p = (std::filesystem::current_path() / path).lexically_normal().string();
#else
            p = fs::path(fs::path(*pwd) / path).lexically_normal();
        else
            p = fs::path(std::filesystem::current_path() / path).lexically_normal();
#endif
    }

    // spdlog::warn("posix_path_to_uri: {} -> {}", path, p);

    // A valid file URI must therefore begin with either file:/path (no hostname), file:///path
    // (empty hostname), or file://hostname/path.

    // relative
    if (not p.empty() && p[0] != '/')
        return caf::uri_builder().scheme("file").path(p).make();

    return caf::uri_builder().scheme("file").host("localhost").path(p).make();
}

std::vector<std::pair<caf::uri, FrameList>>
xstudio::utility::scan_posix_path(const std::string &path, const int depth) {
    std::vector<std::pair<caf::uri, FrameList>> items;

    // spdlog::error("xstudio::utility::scan_posix_path {}", path);

    fs::path p(path);

    try {
        if (fs::is_directory(p)) {
            // read content.
            try {
                std::vector<std::string> files;
                for (const auto &entry : fs::directory_iterator(path)) {
                    if (!entry.path().filename().empty() &&
                        entry.path().filename().string()[0] == '.')
                        continue;
                    if (fs::is_directory(entry) && (depth > 0 || depth < 0)) {
#ifdef _WIN32
                        auto more = scan_posix_path(entry.path().string(), depth - 1);
#else
                        auto more = scan_posix_path(entry.path(), depth - 1);
#endif
                        items.insert(items.end(), more.begin(), more.end());
                    } else if (fs::is_regular_file(entry))
#ifdef _WIN32
                        files.push_back(entry.path().string());
#else
                        files.push_back(entry.path());
#endif
                }
                auto file_items = uri_from_file_list(files);
                items.insert(items.end(), file_items.begin(), file_items.end());
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        } else if (fs::is_regular_file(p)) {
            items.emplace_back(std::make_pair(posix_path_to_uri(path), FrameList()));
        } else {
            FrameList fl;
            auto uri = xstudio::utility::parse_cli_posix_path(path, fl, true);
            if (not uri.empty()) {
                items.emplace_back(std::make_pair(uri, fl));
            } else {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, path);
            }
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return items;
}

std::string xstudio::utility::filemanager_show_uris(const std::vector<caf::uri> &uris) {
    int exit_code;

    std::vector<std::string> cmd = {
        "dbus-send",
        "--session",
        "--print-reply",
        "--dest=org.freedesktop.FileManager1",
        "--type=method_call",
        "/org/freedesktop/FileManager1",
        "org.freedesktop.FileManager1.ShowItems",
        std::string("array:string:") + join_as_string(uris, ","),
        "string:"};
    // dbus-send --session --print-reply --dest=org.freedesktop.FileManager1 --type=method_call
    // /org/freedesktop/FileManager1 org.freedesktop.FileManager1.ShowItems
    // array:string:"file://localhost//u/al/Desktop/itsdeadjim.jpg" string:""

    return xstudio::utility::exec(cmd, exit_code);
}

std::string xstudio::utility::get_host_name() {
    std::array<char, MAXHOSTNAMELEN> hostname{0};
    gethostname(hostname.data(), (int)hostname.size());
    return hostname.data();
}

std::string xstudio::utility::get_user_name() {
    std::string result;

#ifdef _WIN32
    TCHAR username[MAX_PATH];
    DWORD size = MAX_PATH;
    if (GetUserName(username, &size)) {
        result = std::string(username);
    }
#else
    long strsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    std::vector<char> buf;

    if (strsize > 0) {
        struct passwd pwbuf;
        struct passwd *pw = nullptr;
        buf.resize(strsize);

        if (getpwuid_r(getuid(), &pwbuf, &(buf[0]), strsize, &pw) == 0 && pw != nullptr) {
            if (pw->pw_gecos)
                result = pw->pw_gecos;
            else
                result = pw->pw_name;
        }
    }
#endif

    return result;
}


std::string xstudio::utility::expand_envvars(
    const std::string &src, const std::map<std::string, std::string> &additional) {
    // use regex to capture envs and replace.
    std::regex words_regex(R"(\$\{[^\}]+\})");
    auto env_begin = std::sregex_iterator(src.begin(), src.end(), words_regex);
    auto env_end   = std::sregex_iterator();


    // build env map
    std::map<std::string, std::string> envs;
    for (std::sregex_iterator i = env_begin; i != env_end; ++i) {
        std::smatch match     = *i;
        std::string match_str = match.str().substr(2, match.str().size() - 3);

        if (not envs.count(match_str)) {
            auto env = get_env(match_str.c_str());
            if (not env) {
                if (additional.count(match_str)) {
                    env = additional.at(match_str);
                } else if (match_str == "USERFULLNAME") {
                    env = utility::get_user_name();
                } else {
                    spdlog::warn("Undefined envvar ${{{}}}", match_str);
                    env = "";
                }
            }
            envs[match_str] = std::string(*env);
        }
    }

    std::string expanded = src;
    for (const auto &i : envs)
        replace_string_in_place(expanded, std::string("${") + i.first + "}", i.second);

    return expanded;
}


std::string xstudio::utility::get_login_name() {
    std::string result;

#ifdef _WIN32
    TCHAR username[MAX_PATH];
    DWORD size = MAX_PATH;
    if (GetUserName(username, &size)) {
        result = std::string(username);
    }
#else
    long strsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    std::vector<char> buf;

    if (strsize > 0) {
        struct passwd pwbuf;
        struct passwd *pw = nullptr;
        buf.resize(strsize);

        if (getpwuid_r(getuid(), &pwbuf, &(buf[0]), strsize, &pw) == 0 && pw != nullptr) {
            result = pw->pw_name;
        }
    }
#endif

    return result;
}

std::vector<std::byte> xstudio::utility::read_file(const std::string &path) {
    std::ifstream is(path.c_str(), std::ios::binary);
    if (is.is_open()) {
        is.seekg(0, std::ios::end);
        auto size = is.tellg();
        is.seekg(0, std::ios::beg);

        // reserve capacity
        std::vector<std::byte> result(size);
        is.read(reinterpret_cast<char *>(result.data()), size);

        if (not is)
            throw std::runtime_error("only " + std::to_string(is.gcount()) + " could be read");

        is.close();
        return result;
    } else {
        throw std::runtime_error("File open failed");
    }
    return std::vector<std::byte>();
}

bool xstudio::utility::check_plugin_uri_request(const std::string &request) {
    const static std::regex re(R"((\w+)://.+)");
    std::cmatch m;
    if (std::regex_match(request.c_str(), m, re)) {
        if (m[1] != "xstudio" and m[1] != "file")
            return true;
    }
    return false;
}
