// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef _WIN32
using pid_t = int; // Use Int type as pyconfig.h
#endif

#include <filesystem>

#include <list>
#include <optional>
#include <string>
#ifdef __linux__
#include <sys/types.h>
#include <unistd.h>
#endif

namespace xstudio {
namespace utility {

    namespace fs = std::filesystem;


    class RemoteSessionFile {
      public:
        RemoteSessionFile(const std::string &file_path);
        RemoteSessionFile(const std::string path, const int port, const bool sync = false);
        RemoteSessionFile(
            const std::string path,
            const int port,
            const bool sync,
            const std::string session_name,
            const std::string host   = "localhost",
            const bool force_cleanup = false);
        virtual ~RemoteSessionFile() = default;

        [[nodiscard]] std::string path() const { return path_; }
        [[nodiscard]] std::string session_name() const { return session_name_; }
        [[nodiscard]] std::string filename() const {
            return session_name_ + "_" + (sync_ ? "sync" : "api") + "_" + host_ + "_" +
                   std::to_string(port_);
        }
        [[nodiscard]] fs::path filepath() const;
        [[nodiscard]] std::string host() const { return host_; }
        [[nodiscard]] bool sync() const { return sync_; }
        [[nodiscard]] int port() const { return port_; }
        [[nodiscard]] bool remove_on_delete() const { return remove_on_delete_; }
        void set_remove_on_delete(const bool remove = true) { remove_on_delete_ = remove; }
        [[nodiscard]] pid_t pid() const { return pid_; }
        [[nodiscard]] bool check_expired() const;
        [[nodiscard]] fs::file_time_type last_write() const { return last_write_; }
        void cleanup() const;

      private:
        [[nodiscard]] pid_t get_pid() const;
        bool create_session_file();

      private:
        std::string path_;
        std::string session_name_;
        std::string host_      = {"localhost"};
        int port_              = {0};
        bool remove_on_delete_ = {false};
        pid_t pid_             = {0};
        bool sync_             = {false};
        fs::file_time_type last_write_;
    };

    class RemoteSessionManager {
      public:
        RemoteSessionManager(const std::string path);
        virtual ~RemoteSessionManager();

        void scan();

        std::string create_session_file(const int port, const bool sync = false);
        void create_session_file(
            const int port,
            const bool sync,
            const std::string session_name,
            const std::string host   = "localhost",
            const bool force_cleanup = false);
        void add_session_file(const RemoteSessionFile rsm);
        [[nodiscard]] const std::list<RemoteSessionFile> &sessions() const { return sessions_; }

        void remove_session(const std::string &session_name);
        [[nodiscard]] size_t size() const { return sessions_.size(); }
        [[nodiscard]] bool empty() const { return sessions_.empty(); }


        [[nodiscard]] std::optional<RemoteSessionFile> first_api() const;
        [[nodiscard]] std::optional<RemoteSessionFile> first_sync() const;
        [[nodiscard]] std::optional<RemoteSessionFile>
        find(const std::string &session_name) const;

      private:
        std::string path_;
        std::list<RemoteSessionFile> sessions_;
    };
} // namespace utility
} // namespace xstudio