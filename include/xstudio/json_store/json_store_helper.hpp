// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/utility/json_store.hpp"
#include <caf/actor_system.hpp>
#include <caf/blocking_actor.hpp>
#include <caf/scoped_actor.hpp>

// not optimal as it's using the system actor..

namespace xstudio {
namespace json_store {
    class JsonStoreHelper {
      public:
        JsonStoreHelper(caf::actor_system &, caf::actor store_actor);
        virtual ~JsonStoreHelper() = default;

        void init(caf::actor_system &, caf::actor store_actor);

        template <typename result_type>
        [[nodiscard]] inline result_type get(const std::string &path = "") const {
            return get(path);
        }
        [[nodiscard]] utility::JsonStore get(const std::string &path = "") const;

        template <typename value_type>
        inline void
        set(const value_type &value,
            const std::string &path     = "",
            const bool async            = true,
            const bool broadcast_change = true) {
            set(utility::JsonStore(value), path, async, broadcast_change);
        }

        void
        set(const utility::JsonStore &V,
            const std::string &path     = "",
            const bool async            = true,
            const bool broadcast_change = true);

        caf::actor get_group(utility::JsonStore &V) const;
        caf::actor get_jsonactor() const;

        [[nodiscard]] caf::actor get_actor() const {
            return caf::actor_cast<caf::actor>(store_actor_);
        }

      protected:
        caf::scoped_actor system_;
        caf::actor_addr store_actor_;
    };
} // namespace json_store
} // namespace xstudio