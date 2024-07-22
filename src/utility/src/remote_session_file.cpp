// SPDX-License-Identifier: Apache-2.0
#include <exception>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/remote_session_file.hpp"
#include "xstudio/utility/string_helpers.hpp"

using namespace xstudio::utility;


static std::regex parse_re(R"((.+)_(.+)_(.+)_(.+))");

RemoteSessionFile::RemoteSessionFile(const std::string &file_path) {
    // build entry..
    fs::path p(file_path);
    // parse path..
#ifdef _WIN32
    path_ = p.parent_path().string();
#else
    path_ = p.parent_path();
#endif

    auto file_name = p.filename().string();
    std::smatch match;
    if (std::regex_search(file_name, match, parse_re)) {
        session_name_ = match[1];
        sync_         = (match[2] == "sync" ? true : false);
        host_         = match[3];
        port_         = std::stoi(match[4], nullptr, 0);
    } else {
        throw std::runtime_error("Invalid remote session file. " + file_name);
    }

    // read pid from file.
    std::ifstream myfile;
    myfile.open(filepath().c_str());
    myfile >> pid_;
    myfile.close();

    // test pid if host is local.
    if (host_ == "localhost" and pid_) {
        if (not exists(fs::path(fmt::format("/proc/{}", pid_)))) {
            remove(filepath());
            throw std::runtime_error("Invalid remote session file, process dead. " + file_name);
        }
    }
    last_write_ = fs::last_write_time(filepath());
}

RemoteSessionFile::RemoteSessionFile(const std::string path, const int port, const bool sync)
    : path_(std::move(path)), port_(port), sync_(sync) {

    pid_ = get_pid();
    for (auto i = 0; i < 100; i++) {
        session_name_ = fmt::format("default{}", i);
        if (create_session_file()) {
            remove_on_delete_ = true;
            break;
        }
    }

    if (not remove_on_delete_)
        throw std::runtime_error("Failed to create remote session file.");
}

RemoteSessionFile::RemoteSessionFile(
    const std::string path,
    const int port,
    const bool sync,
    const std::string session_name,
    const std::string host,
    const bool force_cleanup)
    : path_(std::move(path)),
      session_name_(std::move(session_name)),
      host_(std::move(host)),
      port_(port),
      sync_(sync) {

    // we can't test if remote is still valid.
    // so we should only use this to create a new connection ?
    if (exists(filepath())) {
        remove(filepath());
    }
    // create new entry
    if (not create_session_file())
        throw std::runtime_error("Failed to create remote session file.");

    if (host == "localhost" or force_cleanup)
        remove_on_delete_ = true;
}

void RemoteSessionFile::cleanup() const {
    if (remove_on_delete_) {
        try {
            remove(filepath());
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
}

fs::path RemoteSessionFile::filepath() const {
    auto p = fs::path(path_);
    p /= filename();
    return p;
}

pid_t RemoteSessionFile::get_pid() const {

#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}


bool RemoteSessionFile::create_session_file() {
    bool result = false;
    // check it doesn't already exist..
    try {
        if (not exists(filepath())) {
            std::ofstream myfile;
            myfile.open(filepath());
            myfile << pid_ << std::endl;
            myfile.close();
            last_write_ = fs::last_write_time(filepath());
            result      = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

RemoteSessionManager::RemoteSessionManager(const std::string path) : path_(std::move(path)) {
    // create path is doesn't exist..
    if (not exists(fs::path(path_)))
        fs::create_directories(path_);

    scan();

    sessions_.sort([](const RemoteSessionFile &a, const RemoteSessionFile &b) {
        return a.last_write() > b.last_write();
    });
}

void RemoteSessionManager::scan() {
    // scan path.. build list..
    for (const auto &entry : fs::directory_iterator(path_)) {
        if (fs::is_regular_file(entry.status())) {
            try {
#ifdef _WIN32
                sessions_.emplace_back(RemoteSessionFile(entry.path().string()));
#else
                sessions_.emplace_back(RemoteSessionFile(entry.path()));
#endif
            } catch (const std::exception &err) {
                spdlog::debug("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }
    }
}

RemoteSessionManager::~RemoteSessionManager() {
    for (const auto &i : sessions_)
        i.cleanup();
}

std::optional<RemoteSessionFile> RemoteSessionManager::first_api() const {
    for (const auto &i : sessions_) {
        if (i.host() == "localhost" and not i.sync())
            return i;
    }

    return {};
}

std::optional<RemoteSessionFile> RemoteSessionManager::first_sync() const {
    for (const auto &i : sessions_) {
        if (i.host() == "localhost" and i.sync())
            return i;
    }

    return {};
}

std::optional<RemoteSessionFile>
RemoteSessionManager::find(const std::string &session_name) const {
    for (const auto &i : sessions_) {
        if (i.session_name() == session_name)
            return i;
    }

    return {};
}

void RemoteSessionManager::add_session_file(const RemoteSessionFile rsm) {
    sessions_.emplace_front(rsm);
}


std::string RemoteSessionManager::create_session_file(const int port, const bool sync) {
    auto i                   = RemoteSessionFile(path_, port, sync);
    std::string session_name = i.session_name();
    sessions_.emplace_front(i);
    return session_name;
}

void RemoteSessionManager::create_session_file(
    const int port,
    const bool sync,
    const std::string session_name,
    const std::string host,
    const bool force_cleanup) {
    sessions_.emplace_front(
        RemoteSessionFile(path_, port, sync, session_name, host, force_cleanup));
}

void RemoteSessionManager::remove_session(const std::string &session_name) {
    std::list<RemoteSessionFile>::iterator i;
    for (i = std::begin(sessions_); i != std::end(sessions_); ++i) {
        if (i->session_name() == session_name) {
            i->cleanup();
            sessions_.erase(i);
            break;
        }
    }
}
