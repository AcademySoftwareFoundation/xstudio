// SPDX-License-Identifier: Apache-2.0
#ifdef _WIN32
// Windows specific implementation
#include <sys/stat.h>
#include <ctime>

time_t get_mtim(const struct stat &st) { return st.st_mtime; }

time_t get_ctim(const struct stat &st) { return st.st_ctime; }

#else
// Linux specific implementation
#define get_mtim(st) (st).st_mtim.tv_sec
#define get_ctim(st) (st).st_ctim.tv_sec

#endif

#include <caf/all.hpp>
#include <limits>
#include <regex>

#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <list>
#include <map>
#include <regex>
#include <set>

#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/sequence.hpp"
#include "xstudio/utility/string_helpers.hpp"

// namespace fs = std::
namespace fs = std::filesystem;


using namespace xstudio::utility;
namespace xstudio::utility {

// parse file list and derive items.
std::vector<std::pair<caf::uri, FrameList>>
uri_from_file_list(const std::vector<std::string> &paths) {
    static const std::regex percent_match(R"(%0(\d+)d)", std::regex::optimize);
    std::vector<std::pair<caf::uri, FrameList>> result;

    std::vector<Entry> entries;
    entries.reserve(paths.size());
    for (const auto &i : paths) {
        entries.emplace_back(Entry(i));
    }

    auto sequences = sequences_from_entries(entries);
    for (const auto &i : sequences) {
        // convert sequence into uri
        if (i.is_sequence()) {
            result.emplace_back(std::make_pair(
                posix_path_to_uri(std::regex_replace(i.name_, percent_match, "{:0$1d}"), true),
                FrameList(i.frames_)));
        } else {
            result.emplace_back(std::make_pair(posix_path_to_uri(i.name_, true), FrameList()));
        }
    }
    return result;
}

std::vector<UriSequence> uri_from_file(const std::string &path) {
    return uri_from_file_list(std::vector<std::string>({path}));
}


Entry::Entry(const std::string path) : name_(std::move(path)) {
    std::memset(&stat_, 0, sizeof stat_);
}

#ifdef _WIN32
uint64_t get_block_size_windows(const xstudio::utility::Entry &entry) {
    const std::string &path = entry.name_; // Assuming 'name_' contains the path
    ULARGE_INTEGER blockSize;
    DWORD blockSizeLow, blockSizeHigh;

    if (!GetDiskFreeSpaceExA(path.c_str(), nullptr, nullptr, &blockSize)) {
        // Error occurred, handle it accordingly
        // ...
    }

    blockSizeLow  = blockSize.LowPart;
    blockSizeHigh = blockSize.HighPart;

    return static_cast<uint64_t>(blockSizeLow) | (static_cast<uint64_t>(blockSizeHigh) << 32);
}
#endif

Sequence::Sequence(const Entry &entry)
    : count_(1),
      uid_(entry.stat_.st_uid),
      gid_(entry.stat_.st_gid),
#ifdef _WIN32
      size_(get_block_size_windows(entry) * 512),
#else
      size_(entry.stat_.st_blocks * 512),
#endif
      apparent_size_(entry.stat_.st_size),
#ifdef _WIN32
      mtim_(get_mtim(entry.stat_)),
      ctim_(get_ctim(entry.stat_)),
#else
      mtim_(entry.stat_.st_mtim.tv_sec),
      ctim_(entry.stat_.st_ctim.tv_sec),
#endif
      name_(entry.name_),
      frames_() {
}
Sequence::Sequence(const std::string name) : name_(std::move(name)), frames_() {}

std::string make_frame_sequence(
    const long start, const long end, const long offset, const bool single = false) {
    // check for single entry
    if (start == end) {
        if (single)
            return std::to_string(start) + "-" + std::to_string(end);

        return std::to_string(start);
    }

    if (offset == 1) {
        return std::to_string(start) + "-" + std::to_string(end);
    }
    if (end == start + offset) {
        return std::to_string(start) + "," + std::to_string(end);
    }

    return std::to_string(start) + "-" + std::to_string(end) + "x" + std::to_string(offset);
}

struct DefaultSequenceHelper {
    uint8_t pad_;
    std::string head_;
    std::string tail_;
    std::string name_;
    std::string frame_str_;
    std::string pad_str_;

    std::set<int> frames_;
    std::vector<Entry> entries_;

    [[nodiscard]] Sequence make_sequence() const {
        Sequence seq;
        time_t max_t = 0;

        seq.name_ = head_ + "%0" + std::to_string(pad_) + "d" + tail_;

        seq.count_         = frames_.size();
        seq.uid_           = 0;
        seq.gid_           = 0;
        seq.mtim_          = 0;
        seq.ctim_          = 0;
        seq.size_          = 0;
        seq.apparent_size_ = 0;

        for (const auto &entry : entries_) {
#ifdef __linux__
            if (entry.stat_.st_mtim.tv_sec > seq.mtim_) {
                seq.mtim_ = entry.stat_.st_mtim.tv_sec;
                if (seq.mtim_ > max_t) {
                    max_t    = seq.mtim_;
                    seq.uid_ = entry.stat_.st_uid;
                    seq.gid_ = entry.stat_.st_gid;
                }
            }
            if (entry.stat_.st_ctim.tv_sec > seq.ctim_) {
                seq.ctim_ = entry.stat_.st_ctim.tv_sec;
                if (seq.ctim_ > max_t) {
                    max_t    = seq.ctim_;
                    seq.uid_ = entry.stat_.st_uid;
                    seq.gid_ = entry.stat_.st_gid;
                }
            }
#endif
#ifdef _WIN32
            seq.size_ += get_block_size_windows(entry) * 512;
#else
            seq.size_ += entry.stat_.st_blocks * 512;
#endif
            seq.apparent_size_ += entry.stat_.st_size;
        }
        if (frames_.empty())
            seq.frames_ = "";
        else if (frames_.size() == 1)
            seq.frames_ = make_frame_sequence(*(frames_.begin()), *(frames_.begin()), 0, true);
        else {
            long start = 0, end = 0, offset = 0;
            seq.frames_ = "";

            for (const auto &frame : frames_) {
                // new range
                if (!offset) {
                    start  = frame;
                    end    = frame;
                    offset = 1;
                } else {
                    // simple, one one element in current frag.
                    if (start == end) {
                        end    = frame;
                        offset = end - start;
                        // easy we`re part of the current sequence
                    } else if ((end + offset) == frame) {
                        end = frame;
                    } else {
                        // new frag..
                        if (seq.frames_.empty()) {
                            seq.frames_ = make_frame_sequence(start, end, offset);
                        } else {
                            seq.frames_ +=
                                std::string(",") + make_frame_sequence(start, end, offset);
                        }
                        start  = frame;
                        end    = frame;
                        offset = 1;
                    }
                }
            }

            if (seq.frames_.empty()) {
                seq.frames_ = make_frame_sequence(start, end, offset);
            } else {
                seq.frames_ += std::string(",") + make_frame_sequence(start, end, offset);
            }
        }

        return seq;
    };
};


std::optional<DefaultSequenceHelper> create_default_seq(const Entry &entry) {
    std::cmatch m;
    try {
        static const std::regex bare_number(R"(()([-]?\d+)())", std::regex::optimize);
        static const std::regex number_and_ext(R"(()([-]?\d+)(\.[^.]+))", std::regex::optimize);
        static const std::regex body_dot_number_and_double_ext(
            R"((.+\.)([-]?\d+)(\.[^.\d]{1,4}\.[^.]{1,3}))", std::regex::optimize);
        static const std::regex body_dot_number_and_ext(
            R"((.+\.)([-]?\d+)(\.[^.]+))", std::regex::optimize);
        // static const std::regex body_number_and_ext(
        //     R"((.+?)([-]?\d+)(\.[^.]+))", std::regex::optimize);
        // static const std::regex body_number_body("(.+)([-]?\\d{3,})(.+)$");
        // if (std::regex_match(entry.name_.c_str(), m, bare_number) or
        //     std::regex_match(entry.name_.c_str(), m, number_and_ext) or
        //     std::regex_match(entry.name_.c_str(), m, body_dot_number_and_double_ext) or
        //     std::regex_match(entry.name_.c_str(), m, body_dot_number_and_ext) or
        //     std::regex_match(entry.name_.c_str(), m, body_number_and_ext)) {

        if (std::regex_match(entry.name_.c_str(), m, bare_number) or
            std::regex_match(entry.name_.c_str(), m, number_and_ext) or
            std::regex_match(entry.name_.c_str(), m, body_dot_number_and_double_ext) or
            std::regex_match(entry.name_.c_str(), m, body_dot_number_and_ext)
            // std::regex_match(entry.name_.c_str(), m, body_number_and_ext)
        ) {
            // skip versions..
            if (ends_with(m[1].str(), "_v"))
                return {};
            DefaultSequenceHelper seq;
            seq.head_ = escape_percentage(m[1].str());
            seq.tail_ = escape_percentage(m[3].str());
            seq.name_ = seq.head_ + "#" + seq.tail_;
            seq.frames_.insert(std::atoi(m[2].str().c_str()));
            seq.frame_str_ = m[2].str();
            seq.pad_       = pad_size(m[2].str());
            seq.pad_str_   = pad_spec(seq.pad_);
            seq.entries_.push_back(entry);
            return seq;
        }

        return {};
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return {};
}

std::vector<Sequence> default_collapse_sequences(const std::vector<Entry> &entries) {
    // collapse sequences that are part of a collection.
    // really hard...
    std::vector<Sequence> sequences;
    std::map<std::string, std::list<DefaultSequenceHelper>> seqs;

    // change entry into something useful..
    for (const auto &entry : entries) {
        auto seq = create_default_seq(entry);
        if (seq)
            seqs[(*seq).name_].push_back(*seq);
        else
            sequences.emplace_back(Sequence(entry));
    }

    // collapse seqs..
    for (auto &[key, val] : seqs) {
        // sort val..
        val.sort([](const DefaultSequenceHelper &a, const DefaultSequenceHelper &b) {
            return a.pad_ > b.pad_;
        });

        for (auto it = val.begin(); it != val.end(); ++it) {
            auto next = std::next(it);
            while (true) {
                auto current = next;
                if (current == val.end())
                    break;
                next = std::next(next);

                if (it->pad_ == current->pad_ or
                    (current->pad_ == 0 and current->frame_str_.size() == it->pad_)) {
                    (*it).frames_.merge((*current).frames_);
                    (*it).entries_.insert(
                        (*it).entries_.end(),
                        (*current).entries_.begin(),
                        (*current).entries_.end());
                    val.erase(current);
                }
            }
        }
    }

    // have populated seqs.
    // iterate over map..
    for (auto const &[key, val] : seqs) {
        // val is a collection of similar DefaultSequenceHelper's
        for (const auto &seq : val) {
            sequences.push_back(seq.make_sequence());
            // std::cout << sequences.back().name << " " << sequences.back().frames <<
            // std::endl;
        }
    }

    return sequences;
}

int pad_size(const std::string &frame) {
    // -01 == pad 3
    // 0 pad means unknown padding
    return (std::to_string(std::atoi(frame.c_str())).size() == frame.size() ? 0 : frame.size());
}

std::string pad_spec(const int pad) {
    // -01 == pad 3
    // 0 pad means unknown padding
    return "%0" + std::to_string(pad) + "d";
}

std::string escape_percentage(const std::string &str) {
    const std::regex replace_regex("%");
    return std::regex_replace(str, replace_regex, "%%");
}

static const std::set<std::string> not_sequence_ext_set{
    ".BZ2", ".bz2", ".MOV", ".mov", ".AVI", ".avi",  ".CINE", ".cine", ".R3D", ".r3d", ".AAF",
    ".aaf", ".MXF", ".mxf", ".WAV", ".wav", ".AIFF", ".aiff", ".HIP",  ".hip", ".MB",  ".mb",
    ".MA",  ".ma",  ".NK",  ".nk",  ".mv4", ".MP4",  ".mp4",  ".mp3",  ".MP3", ".WEBM"};

bool default_is_sequence(const Entry &entry) {
    // things that are never sequences..
#ifdef _WIN32
    std::string ext = std::filesystem::path(entry.name_).extension().string();
#else
    std::string ext = std::filesystem::path(entry.name_).extension();
#endif
    // we don't try and handle case, as that get's trick when utf-8 is in use..
    // we assume that it'll not be mixed..
    if (not_sequence_ext_set.count(to_lower(ext)))
        return false;

    return true;
}

bool default_ignore_sequence(const Entry &) { return false; }

bool always_ignore_sequence(const Entry &) { return true; }

bool default_ignore_entry(const std::string &, const Entry &) { return false; }

std::vector<Sequence> sequences_from_entries(
    const std::vector<Entry> &entries,
    CollapseSequencesFunc collapse_sequences,
    IsSequenceFunc is_sequence,
    IgnoreSequenceFunc ignore_sequence) {
    std::vector<Entry> sequence_entries;
    std::vector<Sequence> not_sequences;

    for (const auto &entry : entries) {
        // only files
        if (S_ISDIR(entry.stat_.st_mode))
            continue;

        if (ignore_sequence(entry))
            continue;

        // no numbers, never a sequence
        if (not std::any_of(entry.name_.begin(), entry.name_.end(), ::isdigit) or
            not is_sequence(entry)) {
            not_sequences.emplace_back(Sequence(entry));
            continue;
        }

        // what kind of sequence..
        // the assumption is at this point we always make it a sequence..
        // need to populate a lookup table so we can reduce entries to collections..
        // a sequence may have multiple candidate numbers..
        // which makes this rather hellish.
        // turn path into a printf format %02d etc. (handle escaping.. ?)
        // turn in to look ups..
        // expose the code that converts into sequences..
        sequence_entries.push_back(entry);
    }
    std::vector<Sequence> sequences = collapse_sequences(sequence_entries);

    sequences.reserve(sequences.size() + distance(not_sequences.begin(), not_sequences.end()));
    sequences.insert(sequences.end(), not_sequences.begin(), not_sequences.end());

    return sequences;
}
} // namespace xstudio::utility