// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <caf/actor_system.hpp>
#include <caf/type_id.hpp>

#include "xstudio/utility/container.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace tag {

    class Tag {
      public:
        Tag() : type_(to_string(id_)) {}
        Tag(const utility::JsonStore &);
        virtual ~Tag() = default;

        [[nodiscard]] utility::JsonStore serialise() const;

        [[nodiscard]] auto id() const { return id_; }
        [[nodiscard]] auto persistent() const { return persistent_; }
        [[nodiscard]] auto link() const { return link_; }
        [[nodiscard]] auto data() const { return data_; }
        [[nodiscard]] auto type() const { return type_; }
        [[nodiscard]] auto unique() const { return unique_; }

        void set_id(const utility::Uuid &value) { id_ = value; }
        void set_link(const utility::Uuid &value) { link_ = value; }
        void set_type(const std::string &value) { type_ = value; }
        void set_persistent(const bool value) { persistent_ = value; }
        void set_data(const std::string &value) { data_ = value; }
        void set_unique(const size_t value) { unique_ = value; }
        void set_unique(const std::string &value) { unique_ = std::hash<std::string>{}(value); }

        template <class Inspector> friend bool inspect(Inspector &f, Tag &x) {
            return f.object(x).fields(
                f.field("id", x.id_),
                f.field("link", x.link_),
                f.field("persist", x.persistent_),
                f.field("type", x.type_),
                f.field("uniq", x.unique_),
                f.field("data", x.data_));
        }

      private:
        utility::Uuid id_{utility::Uuid::generate()};
        utility::Uuid link_;
        std::string type_;
        bool persistent_{false};
        std::string data_;
        size_t unique_{0};
    };

    class TagBase : public utility::Container {
      public:
        TagBase(const std::string &name = "TagBase");
        TagBase(const utility::JsonStore &jsn);
        ~TagBase() override = default;

        [[nodiscard]] utility::JsonStore serialise() const override;
        [[nodiscard]] std::optional<Tag> get_tag(const utility::Uuid &id) const;
        [[nodiscard]] std::vector<Tag> get_tags(const utility::Uuid &link) const;
        [[nodiscard]] std::vector<Tag> get_tags() const;

        std::optional<Tag> add_tag(const Tag tag);
        bool remove_tag(const utility::Uuid &id);

      private:
        std::map<utility::Uuid, utility::UuidVector> link_map_;
        std::map<utility::Uuid, Tag> tag_map_;
    };

} // namespace tag
} // namespace xstudio
