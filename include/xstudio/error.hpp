// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <exception>

namespace xstudio {

class xstudio_err : public std::runtime_error {
  public:
    xstudio_err(const std::string &what_arg) : std::runtime_error(what_arg) {}
    xstudio_err(const char *what_arg) : std::runtime_error(what_arg) {}
};

} // namespace xstudio