// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace xstudio::scanner {
class ScanHelperActor : public caf::event_based_actor {
  public:
    ScanHelperActor(caf::actor_config &cfg);

    ~ScanHelperActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    const char *name() const override { return NAME.c_str(); }

  private:
    std::pair<std::string, uintmax_t> checksum_for_path(const std::filesystem::path &path);

    inline static const std::string NAME = "ScanHelperActor";
    caf::behavior behavior_;

    struct ChecksumCacheEntry {
        std::string checksum_;
        uintmax_t size_{0};
        std::filesystem::file_time_type modified_at_{};
    };

    std::unordered_map<std::string, ChecksumCacheEntry> cache_;
};

class ScannerActor : public caf::event_based_actor {
  public:
    ScannerActor(caf::actor_config &cfg);

    ~ScannerActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }

    void on_exit() override;
    const char *name() const override { return NAME.c_str(); }

  private:
    caf::actor next_helper();

    inline static const std::string NAME = "ScannerActor";
    caf::behavior behavior_;
    std::vector<caf::actor> helpers_;
    size_t next_helper_index_{0};
};

} // namespace xstudio::scanner
