// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/frame_range.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/timeline/item.hpp"

namespace xstudio {
namespace timeline {
    class Gap : public utility::Container {
      public:
        Gap(const std::string &name                    = "Gap",
            const utility::FrameRateDuration &duration = utility::FrameRateDuration(),
            const utility::Uuid &uuid                  = utility::Uuid::generate(),
            const caf::actor &actor                    = caf::actor());
        Gap(const utility::JsonStore &jsn);
        Gap(const Item &item, const caf::actor &actor);

        ~Gap() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] Gap duplicate() const;

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

      private:
        Item item_;
    };
} // namespace timeline
} // namespace xstudio