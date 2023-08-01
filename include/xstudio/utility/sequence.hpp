// SPDX-License-Identifier: Apache-2.0
// container to handle sequences/mov files etc..
#pragma once

#ifdef _WIN32
using uid_t = DWORD; // Use DWORD type for user ID
using gid_t = DWORD; // Use DWORD type for group ID
#else
// For Linux or non-Windows platforms
using uid_t = uid_t; 
using gid_t = gid_t;
#endif

// #include <limits>
// #include <set>
#include <ctime>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <caf/uri.hpp>
// #include <caf/type_id.hpp>

#include "xstudio/utility/frame_list.hpp"

namespace xstudio {
namespace utility {
    using UriSequence = std::pair<caf::uri, FrameList>;

    std::vector<UriSequence> uri_from_file_list(const std::vector<std::string> &paths);
    std::vector<UriSequence> uri_from_file(const std::string &path);

    struct Entry {
        struct stat stat_;
        std::string name_;

        Entry(const std::string path);
    };

    struct Sequence {
        size_t count_{0};
        uid_t uid_{0};
        gid_t gid_{0};
        off_t size_{0};
        off_t apparent_size_{0};
        time_t mtim_{0};
        time_t ctim_{0};
        std::string name_;
        std::string frames_;

        [[nodiscard]] inline bool is_sequence() const { return not frames_.empty(); }

        Sequence(const std::string name = "");
        Sequence(const Entry &entry);
        // Sequence(const DFile &file);
    };
    int pad_size(const std::string &frame);
    std::string pad_spec(const int pad);
    std::string escape_percentage(const std::string &str);

    // defaults to simple sequences.. only wibble.####.ext
    std::vector<Sequence> default_collapse_sequences(const std::vector<Entry> &entries);

    bool default_is_sequence(const Entry &entry);
    bool default_ignore_sequence(const Entry &entry);
    bool default_ignore_entry(const std::string &path, const Entry &entry);

    // this will cause only directories to be seen
    bool always_ignore_sequence(const Entry &entry);

    using CollapseSequencesFunc =
        std::function<std::vector<Sequence>(const std::vector<Entry> &entries)>;
    using IsSequenceFunc     = std::function<bool(const Entry &entry)>;
    using IgnoreSequenceFunc = std::function<bool(const Entry &entry)>;
    using IgnoreEntryFunc    = std::function<bool(const std::string &path, const Entry &entry)>;

    std::vector<Sequence> sequences_from_entries(
        const std::vector<Entry> &entries,
        CollapseSequencesFunc collapse_sequences = default_collapse_sequences,
        IsSequenceFunc is_sequence               = default_is_sequence,
        IgnoreSequenceFunc ignore_sequence       = default_ignore_sequence);

} // namespace utility
} // namespace xstudio
