// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <pwd.h>
#include <sys/stat.h>

#include <caf/uri.hpp>
#include <filesystem>


#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"

namespace xstudio {

namespace utility {

    namespace fs = std::filesystem;


    class LockFile {
      public:
        LockFile(const LockFile &src) : LockFile(src.source_) {}
        LockFile(const std::string &path = "") : LockFile(posix_path_to_uri(path)) {}
        LockFile(const caf::uri uri) {
            // sanitise.
            source_ = posix_path_to_uri(uri_to_posix_path(uri));
            check_externally_locked();
        }
        virtual ~LockFile() { static_cast<void>(unlock()); }


        LockFile &operator=(const LockFile &other) {
            if (this != &other) // not a self-assignment
            {
                // unlock our current
                static_cast<void>(unlock());
                source_ = other.source_;
                check_externally_locked();
            }
            return *this;
        }

        [[nodiscard]] caf::uri source() const { return source_; }
        [[nodiscard]] caf::uri lock_file() const {
            // resolve path to source..
            auto lpath = uri_to_posix_path(source_);
            if (fs::exists(lpath) && fs::is_symlink(lpath))
                lpath = fs::canonical(lpath);

            return posix_path_to_uri(lpath + ".lock");
        }
        [[nodiscard]] bool locked() const { return locked_; }
        [[nodiscard]] bool owned() const { return owned_; }
        [[nodiscard]] std::string owned_by() const { return owner_; }

        [[nodiscard]] bool lock() {
            if (locked_ and owned_)
                return true;

            // check no lock has happened..
            if (locked_ or not fs::exists(uri_to_posix_path(source_)))
                return false;

            try {
                std::ofstream myfile;
                myfile.open(uri_to_posix_path(lock_file()));
                myfile << "Locked.\n";
                myfile.close();
            } catch (const std::exception &err) {
                spdlog::warn("Failed to create lock file {}", err.what());
                return false;
            }

            locked_ = true;
            owned_  = true;
            set_owner_uid(get_uid(lock_file()));
            return true;
        }

        [[nodiscard]] bool unlock() {
            if (locked_ and owned_ and not borrowed_) {
                // unlock we no longer own file.
                unlink(uri_to_posix_path(lock_file()).c_str());
                reset();
                return true;
            }

            return false;
        }

        void set_owner_uid(const uid_t uid) {
            owner_uid_ = uid;
            owner_     = get_owner(owner_uid_);
        }

      private:
        void reset() {
            locked_    = false;
            owned_     = false;
            owner_     = "Unlocked";
            owner_uid_ = 0;
            borrowed_  = false;
        }

        [[nodiscard]] uid_t get_uid(const caf::uri path) const {
            struct stat st;
            if (lstat(uri_to_posix_path(path).c_str(), &st) == 0)
                return st.st_uid;
            return 0;
        }

        [[nodiscard]] std::string get_owner(const uid_t uid) const {
            auto owner        = std::to_string(uid);
            struct passwd *pw = getpwuid(uid);
            if (pw) {
                owner = std::string(pw->pw_gecos);
                if (owner.empty())
                    owner = std::string(pw->pw_name);
            }
            return owner;
        }

        void check_externally_locked() {
            // only check if we don't own it already
            if (not owned_) {
                if (fs::exists(uri_to_posix_path(lock_file()))) {
                    // check it's own by ourself
                    set_owner_uid(get_uid(lock_file()));
                    locked_   = true;
                    owned_    = (getuid() == owner_uid_);
                    borrowed_ = owned_;
                } else {
                    reset();
                }
            }
        }

        caf::uri source_;
        bool locked_{false};
        bool owned_{false};
        std::string owner_;
        uid_t owner_uid_{0};
        bool borrowed_{false};
    };

} // namespace utility
} // namespace xstudio
