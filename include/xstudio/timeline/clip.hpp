// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <memory>
#include <string>

#include "xstudio/media/media.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/edit_list.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {

    class Clip : public utility::Container {
      public:
        Clip(
            const std::string name         = "Clip",
            const utility::Uuid &uuid      = utility::Uuid::generate(),
            const caf::actor &actor        = caf::actor(),
            const utility::Uuid media_uuid = utility::Uuid());
        Clip(const utility::JsonStore &jsn);

        ~Clip() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] Clip duplicate() const;

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

        [[nodiscard]] const utility::Uuid &media_uuid() const { return media_uuid_; }
        void set_media_uuid(const utility::Uuid &media_uuid) {
            auto jsn          = item_.prop();
            jsn["media_uuid"] = media_uuid;
            item_.set_prop(jsn);
            media_uuid_ = media_uuid;
        }

      private:
        Item item_;
        utility::Uuid media_uuid_;
    };
} // namespace timeline
} // namespace xstudio