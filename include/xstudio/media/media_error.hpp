// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/error.hpp"

namespace xstudio {

class media_err : public xstudio_err {
  public:
    media_err(const std::string &what_arg) : xstudio_err(what_arg) {}
    media_err(const char *what_arg) : xstudio_err(what_arg) {}
};

class media_missing_error : public media_err {
  public:
    media_missing_error(const std::string &what_arg) : media_err(what_arg) {}
    media_missing_error(const char *what_arg) : media_err(what_arg) {}
};

class media_corrupt_error : public media_err {
  public:
    media_corrupt_error(const std::string &what_arg) : media_err(what_arg) {}
    media_corrupt_error(const char *what_arg) : media_err(what_arg) {}
};

class media_unsupported_error : public media_err {
  public:
    media_unsupported_error(const std::string &what_arg) : media_err(what_arg) {}
    media_unsupported_error(const char *what_arg) : media_err(what_arg) {}
};

class media_unreadable_error : public media_err {
  public:
    media_unreadable_error(const std::string &what_arg) : media_err(what_arg) {}
    media_unreadable_error(const char *what_arg) : media_err(what_arg) {}
};

} // namespace xstudio