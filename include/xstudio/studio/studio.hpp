// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio::studio {
class Studio : public utility::Container {
  public:
    Studio(const std::string &name = "Studio");
    Studio(const utility::JsonStore &jsn);

    ~Studio() override = default;

    [[nodiscard]] utility::JsonStore serialise() const override;
};
} // namespace xstudio::studio
