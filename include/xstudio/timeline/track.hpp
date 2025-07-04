// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor.hpp>
#include <string>

#include "xstudio/media/enums.hpp"
#include "xstudio/timeline/item.hpp"
#include "xstudio/utility/container.hpp"
#include "xstudio/utility/frame_rate.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace timeline {

    class Track : public utility::Container {
      public:
        explicit Track(
            const std::string &name,
            const utility::FrameRate &rate,
            const media::MediaType media_type = media::MediaType::MT_IMAGE,
            const utility::Uuid &uuid         = utility::Uuid::generate(),
            const caf::actor &actor           = caf::actor());
        Track(const utility::JsonStore &jsn);
        Track(const Item &item, const caf::actor &actor);

        ~Track() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;

        [[nodiscard]] Track duplicate() const;

        void set_media_type(const media::MediaType media_type);
        [[nodiscard]] media::MediaType media_type() const { return media_type_; }

        [[nodiscard]] const Item &item() const { return item_; }
        [[nodiscard]] Item &item() { return item_; }

        [[nodiscard]] const Items &children() const { return item_.children(); }
        [[nodiscard]] Items &children() { return item_.children(); }

        [[nodiscard]] bool valid_child(const Item &child) const {
            return item_.valid_child(child);
        }

        utility::JsonStore refresh_item() { return item_.refresh(); }

        utility::FrameRate rate() const { return item_.rate(); }

      private:
        media::MediaType media_type_;
        Item item_;
    };
} // namespace timeline
} // namespace xstudio