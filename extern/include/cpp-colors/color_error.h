#pragma once

#include <exception>
#include <string>

namespace colors {

  class color_error
    : public std::runtime_error {
  public:
    explicit color_error(const std::string& message)
      : std::runtime_error(message) {
      /* no-op */
    }

    virtual ~color_error() {
      /* no-op */
    }
  };

} // namespace colors
