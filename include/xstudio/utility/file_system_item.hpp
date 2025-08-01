// SPDX-License-Identifier: Apache-2.0
// container to handle sequences/mov files etc..
#pragma once

// #include <caf/type_id.hpp>
#ifdef _WIN32
#else
#include <pwd.h>
#endif
#include <sys/stat.h>
#include <filesystem>
#include <list>

#include <caf/uri.hpp>

#include "xstudio/utility/json_store.hpp"

namespace xstudio::utility {

// typedef enum {
//     FSA_INSERT = 0x1L,
//     FSA_REMOVE = 0x2L,
//     FSA_MOVE   = 0x3L,
//     FSA_UPDATE = 0x4L,

// } FileSystemAction;

typedef enum {
    FSIT_NONE      = 0x0L,
    FSIT_ROOT      = 0x1L,
    FSIT_DIRECTORY = 0x2L,
    FSIT_FILE      = 0x3L,

} FileSystemItemType;

class FileSystemItem;
using FileSystemItems = std::list<FileSystemItem>;

// typedef std::function<void(const utility::JsonStore &event, FileSystemItem &item)>
//     FileSystemItemEventFunc;
typedef std::function<bool(const fs::directory_entry &entry)> FileSystemItemIgnoreFunc;

class FileSystemItem : private FileSystemItems {
  public:
    FileSystemItem() : FileSystemItems() {}
    FileSystemItem(const fs::directory_entry &entry);
    FileSystemItem(
        const std::string name, const caf::uri path, FileSystemItemType type = FSIT_DIRECTORY)
        : FileSystemItems(),
          name_(std::move(name)),
          path_(std::move(path)),
          type_(std::move(type)) {}

    virtual ~FileSystemItem() = default;

    using FileSystemItems::empty;
    using FileSystemItems::size;

    using FileSystemItems::begin;
    using FileSystemItems::cbegin;
    using FileSystemItems::cend;
    using FileSystemItems::end;

    using FileSystemItems::crbegin;
    using FileSystemItems::crend;
    using FileSystemItems::rbegin;
    using FileSystemItems::rend;

    using FileSystemItems::back;
    using FileSystemItems::front;

    // these circumvent the handler..
    using FileSystemItems::clear;
    using FileSystemItems::emplace_back;
    using FileSystemItems::emplace_front;
    using FileSystemItems::pop_back;
    using FileSystemItems::pop_front;
    using FileSystemItems::push_back;
    using FileSystemItems::push_front;
    using FileSystemItems::splice;

    FileSystemItems::iterator
    insert(FileSystemItems::iterator position, const FileSystemItem &val);

    FileSystemItems::iterator erase(FileSystemItems::iterator position);

    [[nodiscard]] const FileSystemItems &children() const { return *this; }
    [[nodiscard]] FileSystemItems &children() { return *this; }

    [[nodiscard]] std::string name() const { return name_; }
    [[nodiscard]] caf::uri path() const { return path_; }
    [[nodiscard]] fs::file_time_type last_write() const { return last_write_; }
    [[nodiscard]] FileSystemItemType type() const { return type_; }

    void set_last_write(const fs::file_time_type &value = fs::file_time_type()) {
        last_write_ = value;
    }

    [[nodiscard]] std::optional<FileSystemItems::const_iterator>
    item_at_index(const int index) const;

    bool scan(const int depth = -1, const bool ignore_last_write = false);

    FileSystemItem *find_by_path(const caf::uri &path);

    utility::JsonStore dump() const;

    // void bind_event_func(FileSystemItemEventFunc fn);
    void bind_ignore_entry_func(FileSystemItemIgnoreFunc fn);


  private:
    FileSystemItemType type_{FSIT_ROOT};
    std::string name_{};
    caf::uri path_{};

    fs::file_time_type last_write_{};

    // FileSystemItemEventFunc event_callback_{nullptr};
    FileSystemItemIgnoreFunc ignore_entry_callback_{nullptr};
};

inline std::string to_string(const FileSystemItemType it) {
    std::string str;
    switch (it) {
    case FSIT_NONE:
        str = "None";
        break;
    case FSIT_ROOT:
        str = "ROOT";
        break;
    case FSIT_DIRECTORY:
        str = "DIRECTORY";
        break;
    case FSIT_FILE:
        str = "FILE";
        break;
    }
    return str;
}

bool ignore_not_session(const fs::directory_entry &entry);

} // namespace xstudio::utility