// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/timeline/item.hpp"

namespace xstudio {
namespace timeline {
    class Stack : public utility::Container {
      public:
        Stack(
            const std::string &name   = "Stack",
            const utility::Uuid &uuid = utility::Uuid::generate(),
            const caf::actor &actor   = caf::actor());

        Stack(const utility::JsonStore &jsn);
        ~Stack() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] Stack duplicate() const;

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] const Items &children() const { return item_.children(); }
        [[nodiscard]] Items &children() { return item_.children(); }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

        utility::JsonStore refresh_item() { return item_.refresh(); }

      private:
        Item item_;
    };
} // namespace timeline
} // namespace xstudio