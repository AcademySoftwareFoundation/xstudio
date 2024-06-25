// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef _WIN32
using uid_t = DWORD; // Use DWORD type for user ID
using gid_t = DWORD; // Use DWORD type for group ID

#include <Lmcons.h>

inline bool lstat(const std::string &path, struct stat *st) {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData)) {
        // Fill the 'struct stat' with information from 'WIN32_FILE_ATTRIBUTE_DATA'
        st->st_mode = fileData.dwFileAttributes;
        // Set other members of 'struct stat' as needed
        // ...
        return true;
    }
    return false;
}

inline uid_t getuid() { return static_cast<uid_t>(GetCurrentProcessId()); }

struct passwd {
    std::string pw_name;
    std::string pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    std::string pw_gecos;
    std::string pw_dir;
    std::string pw_shell;
};

inline struct passwd *getpwuid(uid_t uid) {
    static struct passwd pw;

    // Get the username associated with the UID
    wchar_t username[UNLEN + 1];
    DWORD usernameLen = UNLEN + 1;
    if (GetUserNameW(username, &usernameLen)) {
        pw.pw_name   = std::to_string(uid); // Set the username as needed
        pw.pw_passwd = "";                  // Set the password as needed
        pw.pw_uid    = uid;
        pw.pw_gid    = 0;  // Set the group ID as needed
        pw.pw_gecos  = ""; // Set the GECOS field as needed
        pw.pw_dir    = ""; // Set the home directory as needed
        pw.pw_shell  = ""; // Set the shell as needed

        return &pw;
    }

    return nullptr;
}
#else
// For Linux or non-Windows platforms
using uid_t = uid_t; 
using gid_t = gid_t;
#include <pwd.h>
#endif

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

#ifdef _WIN32
            std::filesystem::path lpath(uri_to_posix_path(source_));

            if (std::filesystem::exists(lpath) && std::filesystem::is_symlink(lpath))
                lpath = std::filesystem::canonical(lpath);

            std::string lpath_string = lpath.string();
            return posix_path_to_uri(lpath.concat(".lock").string());
#else
            // For other platforms, use the existing code
            auto lpath = uri_to_posix_path(source_);
            if (fs::exists(lpath) && fs::is_symlink(lpath))
                lpath = fs::canonical(lpath);

            return posix_path_to_uri(lpath + ".lock");
#endif       
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
#ifdef _WIN32
                _unlink(uri_to_posix_path(lock_file()).c_str());
#else
                unlink(uri_to_posix_path(lock_file()).c_str());
#endif
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
