// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdio>
#include <filesystem>

// handle directories where we want to manage content..
namespace xstudio::utility {

namespace fs = std::filesystem;

class ManagedDir {
  public:
    ManagedDir(
        const fs::path target                      = "",
        const size_t max_size                      = 0,
        const size_t max_count                     = 0,
        const fs::file_time_type::duration max_age = std::chrono::seconds(0))
        : target_(std::move(target)),
          max_size_(max_size),
          max_count_(max_count),
          max_age_(std::move(max_age)) {}

    virtual ~ManagedDir() {
        if (prune_on_exit_)
            prune();
    }

    void max_size(const size_t value) { max_size_ = value; }
    [[nodiscard]] size_t max_size() const { return max_size_; }
    [[nodiscard]] size_t current_size() const { return current_size_; }

    void max_count(const size_t value) { max_count_ = value; }
    [[nodiscard]] size_t max_count() const { return max_count_; }
    [[nodiscard]] size_t current_count() const { return current_count_; }

    void max_age(const fs::file_time_type::duration value) { max_age_ = value; }
    [[nodiscard]] fs::file_time_type::duration max_age() const { return max_age_; }
    [[nodiscard]] fs::file_time_type current_max_age() const { return current_max_age_; }

    void target(const fs::path &value, const bool create = false) {
        target_ = value;
        if (create)
            fs::create_directories(target_);
    }
    void target(const std::string &value, const bool create = false) {
        target(fs::path(value), create);
    }

    [[nodiscard]] fs::path target() const { return target_; }
    [[nodiscard]] std::string target_string() const { return target_.string(); }

    void prune_on_exit(const bool value) { prune_on_exit_ = value; }
    [[nodiscard]] bool prune_on_exit() { return prune_on_exit_; }

    void scan() {
        content_.clear();
        current_count_   = 0;
        current_size_    = 0;
        current_max_age_ = fs::file_time_type::max();

        if (target_.string() == "")
            return;

        try {
            for (const auto &entry : fs::directory_iterator(target_)) {
                if (fs::is_regular_file(entry.status())) {
                    auto mtime = fs::last_write_time(entry.path());
                    auto size  = fs::file_size(entry.path());
                    auto name  = entry.path();

                    current_count_++;
                    current_size_ += size;
                    current_max_age_ = std::min(current_max_age_, mtime);
                    content_.insert(std::make_pair(mtime, std::make_pair(name, size)));
                }
            }
        } catch (...) {
        }
    }

    bool prune(bool force = false) {
        auto done   = false;
        auto pruned = false;
        scan();

        while (not done and not content_.empty()) {
            done = true;

            if (max_count_ and current_count_ > max_count_)
                done = false;
            else if (max_size_ and current_size_ > max_size_)
                done = false;
            else if (
                max_age_ != std::chrono::seconds(0) and
                current_max_age_ < fs::file_time_type::clock::now() - max_age_)
                done = false;
            else if (force)
                done = false;

            force = false;

            if (not done) {
                pruned     = true;
                auto it    = content_.begin();
                auto mtime = it->first;
                auto item  = it->second;
                content_.erase(it);

                if (content_.empty()) {
                    current_count_   = 0;
                    current_size_    = 0;
                    current_max_age_ = fs::file_time_type::max();
                } else {
                    current_count_--;
                    current_size_ -= item.second;
                    current_max_age_ = content_.begin()->first;
                }
                // prune file from list..
                try {
                    fs::remove(item.first);
                } catch (...) {
                    // give up..
                    done = true;
                }
            }
        }
        return pruned;
    }

    void purge() {
        while (prune(true)) {
        }
    }

  private:
    fs::path target_;

    size_t max_size_{0};
    size_t max_count_{0};
    fs::file_time_type::duration max_age_{std::chrono::seconds(0)};

    size_t current_count_{0};
    size_t current_size_{0};
    fs::file_time_type current_max_age_{fs::file_time_type::max()};

    bool prune_on_exit_{false};

    std::multimap<fs::file_time_type, std::pair<fs::path, size_t>> content_;
};

} // namespace xstudio::utility