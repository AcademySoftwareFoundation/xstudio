// SPDX-License-Identifier: Apache-2.0

#include "xstudio/atoms.hpp"
#include "xstudio/utility/path_remapper.hpp"

namespace xstudio::utility {

std::string PathRemapper::forwards(const std::string &path) { return remap(path, true); }

std::string PathRemapper::backwards(const std::string &path) { return remap(path, false); }

std::string PathRemapper::remap(const std::string &path, const bool forward) {

    std::scoped_lock lock(mutex_);

    const auto &remap = (forward ? forward_regex_ : backward_regex_);
    auto p            = path;

    // are we in danger of flip / flopping forever ?
    if (forward) {
        auto it = forward_map_.find(p);
        while (it != std::end(forward_map_) and p != it->second) {
            p  = it->second;
            it = forward_map_.find(p);
        }
    }

    for (const auto &r : remap) {
        p = std::regex_replace(p, r.first, r.second);
    }

    // are we in danger of flip / flopping forever ?
    if (not forward) {
        auto it = backward_map_.find(p);
        while (it != std::end(backward_map_) and p != it->second) {
            p  = it->second;
            it = backward_map_.find(p);
        }
    }

    // if(p != path) {
    // 	spdlog::warn("remap forward {} {} -> {}", forward, path, p);
    // }

    return p;
}

void PathRemapper::add_path_mapping(const std::string &from, const std::string &to) {
    // spdlog::warn("add_path_mapping {} -> {}", from, to);
    std::scoped_lock lock(mutex_);
    forward_map_.emplace(std::make_pair(from, to));
    backward_map_.emplace(std::make_pair(to, from));
}


void PathRemapper::configure(const utility::JsonStore &jsn) {
    std::scoped_lock lock(mutex_);
    try {
        if (!jsn.is_object())
            throw std::runtime_error("Json should be a dict");

        for (auto p = jsn.begin(); p != jsn.end(); ++p) {
            auto f = p.value();
            if (!f.is_array())
                continue;
            for (const auto &e : f) {
                if (e.size() != 3 || !e.at(0).is_string() || !e.at(1).is_string() ||
                    !e.at(2).is_boolean())
                    throw std::runtime_error(
                        "Elements of Json should be an array with 3 elemens [str, str, bool]");
            }
        }

        forward_regex_.clear();
        backward_regex_.clear();

        for (auto p = jsn.begin(); p != jsn.end(); ++p) {
            auto f = p.value();
            if (!f.is_array())
                continue;
            for (const auto &e : f) {
                if (e.at(2).get<bool>())
                    forward_regex_.emplace_back(
                        e.at(0).get<std::string>(), e.at(1).get<std::string>());
                else {
                    backward_regex_.emplace_back(
                        e.at(0).get<std::string>(), e.at(1).get<std::string>());
                }
            }
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {} -- \n\n{}\n\n", __PRETTY_FUNCTION__, e.what(), jsn.dump(2));
    }
}

} // namespace xstudio::utility
