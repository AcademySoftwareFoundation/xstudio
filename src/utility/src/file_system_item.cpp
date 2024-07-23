// SPDX-License-Identifier: Apache-2.0

#include "xstudio/utility/file_system_item.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::utility;

FileSystemItem::FileSystemItem(const fs::directory_entry &entry) : FileSystemItems() {
    if (fs::is_regular_file(entry.status()))
        type_ = FSIT_FILE;
    else
        type_ = FSIT_DIRECTORY;

    path_ = posix_path_to_uri(path_to_string(entry.path()));
    // last_write_ = fs::last_write_time(entry.path());
    name_ = path_to_string(entry.path().filename());
}

bool FileSystemItem::scan(const int depth, const bool ignore_last_write) {
    auto changed = false;
    try {
        switch (type_) {
        case FSIT_NONE:
        case FSIT_FILE:
            break;

        case FSIT_ROOT:
            if (depth)
                for (auto &i : *this)
                    changed |= i.scan(depth - 1, ignore_last_write);
            break;

        case FSIT_DIRECTORY: {
            // check path exists.
            auto path   = uri_to_posix_path(path_);
            auto update = false;
            std::map<caf::uri, FileSystemItems::iterator> child_paths;

            for (auto it = begin(); it != end(); ++it)
                child_paths.insert(
                    std::pair<caf::uri, FileSystemItems::iterator>(it->path(), it));

            if (not fs::exists(path)) {
                // we only deal with our own children..
                // erase children
                FileSystemItems::clear();
                set_last_write();
                changed = true;
            } else {
                auto scan = ignore_last_write;

                if (not ignore_last_write) {
                    auto last_write = fs::last_write_time(path);
                    if (last_write != last_write_) {
                        scan        = true;
                        update      = true;
                        last_write_ = last_write;
                    }
                }

                if (scan) {
                    // remove children when we see themm.
                    for (const auto &entry : fs::directory_iterator(path)) {
                        try {
                            if (ignore_entry_callback_ == nullptr or
                                not ignore_entry_callback_(entry)) {
                                auto cpath = posix_path_to_uri(path_to_string(entry.path()));

                                // is new entry ?
                                FileSystemItems::iterator it = end();

                                if (child_paths.count(cpath)) {
                                    it = child_paths.at(cpath);
                                    child_paths.erase(cpath);
                                } else {
                                    it      = insert(end(), FileSystemItem(entry));
                                    changed = true;
                                }

                                if (it->type() == FSIT_DIRECTORY) {
                                    if (depth) {
                                        changed |= it->scan(depth - 1, ignore_last_write);
                                    }
                                } else {
                                    it->set_last_write(fs::last_write_time(path));
                                }
                            }
                        } catch (const std::exception &err) {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }

                    for (auto &i : child_paths) {
                        erase(i.second);
                        changed = true;
                    }

                } else {
                    if (depth) {
                        for (auto it = begin(); it != end(); ++it) {
                            if (it->type() == FSIT_DIRECTORY) {
                                changed |= it->scan(depth - 1, ignore_last_write);
                            }
                        }
                    }
                }
            }
        } break;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return changed;
}

JsonStore FileSystemItem::dump() const {
    auto result = JsonStore(
        R"({"type_name": null, "type": null, "last_write": null, "name": null, "path": null})"_json);

    result["type_name"]  = to_string(type_);
    result["type"]       = type_;
    result["last_write"] = last_write_;
    result["name"]       = name_;
    result["path"]       = to_string(path_);

    if (type_ == FSIT_ROOT or type_ == FSIT_DIRECTORY) {
        auto children = nlohmann::json::array();

        for (const auto &i : *this) {
            children.emplace_back(i.dump());
        }

        result["children"] = children;
    }

    return result;
}

// void FileSystemItem::bind_event_func(FileSystemItemEventFunc fn) {
//     event_callback_ = [fn](auto &&PH1, auto &&PH2) {
//         return fn(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
//     };

//     for (auto &i : *this)
//         i.bind_event_func(fn);
// }

void FileSystemItem::bind_ignore_entry_func(FileSystemItemIgnoreFunc fn) {
    ignore_entry_callback_ = [fn](auto &&PH1) { return fn(std::forward<decltype(PH1)>(PH1)); };

    for (auto &i : *this)
        i.bind_ignore_entry_func(fn);
}

FileSystemItems::iterator
FileSystemItem::insert(FileSystemItems::iterator position, const FileSystemItem &val) {
    auto it = FileSystemItems::insert(position, val);
    it->bind_ignore_entry_func(ignore_entry_callback_);
    return it;
}

FileSystemItems::iterator FileSystemItem::erase(FileSystemItems::iterator position) {
    return FileSystemItems::erase(position);
}


bool xstudio::utility::ignore_not_session(const fs::directory_entry &entry) {
    auto result = false;

    if (fs::is_regular_file(entry.status())) {
        auto ext = to_lower(path_to_string(entry.path().extension()));
        if (ext != ".xst" and ext != ".xsz")
            result = true;
    }

    return result;
}


FileSystemItem *FileSystemItem::find_by_path(const caf::uri &path) {
    FileSystemItem *result = nullptr;

    if (path == path_)
        result = this;
    else {
        for (auto &i : *this) {
            result = i.find_by_path(path);
            if (result)
                break;
        }
    }

    return result;
}
