// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <regex>
#include <vector>
#include <string>
#include <mutex>

// std::string forward_remap_file_path(const std::string path);

// std::string reverse_remap_file_path(const std::string path);
namespace xstudio::utility {

class PathRemapper {

  public:
    PathRemapper() = default;
    virtual ~PathRemapper() = default;

    std::string forwards(const std::string &path);
    std::string backwards(const std::string &path);

    void configure(const utility::JsonStore &jsn);

    void add_path_mapping(const std::string &from, const std::string &to);

  private:
  	std::string remap(const std::string &path, const bool forwards);

  	std::mutex mutex_;

  	std::vector<std::pair<std::regex, std::string>> forward_regex_;
  	std::vector<std::pair<std::regex, std::string>> backward_regex_;

  	std::map<std::string, std::string> forward_map_;
  	std::map<std::string, std::string> backward_map_;
};

}