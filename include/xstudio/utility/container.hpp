// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <list>
#include <optional>
#include <ostream>
#include <string>

#include <fmt/format.h>

#include <caf/actor_companion.hpp>
#include <caf/event_based_actor.hpp>
#include <semver.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {

namespace utility {

    struct ContainerDetail {
        ContainerDetail(
            const std::string name = "",
            const std::string type = "",
            const Uuid uuid        = Uuid(),
            caf::actor actor       = caf::actor(),
            caf::actor group       = caf::actor())
            : name_(std::move(name)),
              type_(std::move(type)),
              uuid_(std::move(uuid)),
              actor_(std::move(actor)),
              group_(std::move(group)) {}

        template <class Inspector> friend bool inspect(Inspector &f, ContainerDetail &x) {
            return f.object(x).fields(
                f.field("name", x.name_),
                f.field("type", x.type_),
                f.field("uuid", x.uuid_),
                f.field("actor", x.actor_),
                f.field("group", x.group_));
        }

        std::string name_;
        std::string type_;
        Uuid uuid_;
        caf::actor actor_;
        caf::actor group_;
    };

    class Container {
      public:
        Container(
            const std::string name   = "",
            const std::string type   = "",
            const utility::Uuid uuid = utility::Uuid::generate())
            : name_(std::move(name)),
              type_(std::move(type)),
              uuid_(std::move(uuid)),
              version_(PROJECT_VERSION) {}
        explicit Container(const utility::JsonStore &jsn);
        virtual ~Container() = default;

        [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
        [[nodiscard]] std::string name() const { return name_; }
        [[nodiscard]] std::string type() const { return type_; }
        [[nodiscard]] semver::version file_version() const { return file_version_; }
        [[nodiscard]] semver::version version() const { return version_; }
        [[nodiscard]] time_point last_changed() const { return last_changed_; }
        void set_last_changed(const time_point last_changed = utility::clock::now()) {
            last_changed_ = std::move(last_changed);
        }
        void set_name(const std::string &name) { name_ = name; }
        void set_uuid(const utility::Uuid &uuid) { uuid_ = uuid; }
        void set_version(const std::string &version) { version_.from_string(version); }
        void set_file_version(const std::string &version, bool warn = false);

        [[nodiscard]] ContainerDetail detail(caf::actor act, caf::actor group) const {
            return ContainerDetail(name_, type_, uuid_, std::move(act), std::move(group));
        }

        [[nodiscard]] virtual utility::JsonStore serialise() const;
        virtual void deserialise(const utility::JsonStore &);

        [[nodiscard]] Container duplicate() const;

        void send_changed(
            caf::actor grp,
            caf::event_based_actor *act,
            const time_point last_changed = utility::clock::now()) {
            if (last_changed > last_changed_) {
                last_changed_ = std::move(last_changed);
                act->send(grp, event_atom_v, last_changed_atom_v, last_changed_);
            }
        }

        void send_changed(
            caf::actor grp,
            caf::blocking_actor *act,
            const time_point last_changed = utility::clock::now()) {
            if (last_changed > last_changed_) {
                last_changed_ = std::move(last_changed);
                act->send(grp, event_atom_v, last_changed_atom_v, last_changed_);
            }
        }

        auto make_get_name_handler() {
            return [=](name_atom) -> std::string { return name_; };
        }
        auto make_set_name_handler() {
            return [=](name_atom, const std::string &name) { name_ = name; };
        }

        auto make_set_name_handler(caf::actor grp, caf::event_based_actor *act) {
            return [=](name_atom, const std::string &name) {
                name_ = name;
                act->send(grp, event_atom_v, name_atom_v, name);
                send_changed(grp, act);
            };
        }
        auto make_set_name_handler(caf::actor grp, caf::blocking_actor *act) {
            return [=](name_atom, const std::string &name) {
                name_ = name;
                act->send(grp, event_atom_v, name_atom_v, name);
                send_changed(grp, act);
            };
        }

        auto make_last_changed_getter() {
            return [=](last_changed_atom) -> time_point { return last_changed_; };
        }
        auto make_last_changed_setter(caf::actor grp, caf::event_based_actor *act) {
            return [=](last_changed_atom, const time_point &last_changed) {
                if (last_changed > last_changed_ or last_changed == time_point()) {
                    last_changed_ = last_changed;
                    act->send(grp, utility::event_atom_v, last_changed_atom_v, last_changed_);
                }
            };
        }
        auto make_last_changed_event_handler(caf::actor grp, caf::event_based_actor *act) {
            return [=](utility::event_atom, last_changed_atom, const time_point &last_changed) {
                if (last_changed > last_changed_) {
                    last_changed_ = last_changed;
                    act->send(grp, utility::event_atom_v, last_changed_atom_v, last_changed_);
                }
            };
        }

        auto make_last_changed_setter(caf::actor grp, caf::blocking_actor *act) {
            return [=](last_changed_atom, const time_point &last_changed) {
                if (last_changed > last_changed_ or last_changed == time_point()) {
                    last_changed_ = last_changed;
                    act->send(grp, utility::event_atom_v, last_changed_atom_v, last_changed_);
                }
            };
        }
        auto make_last_changed_event_handler(caf::actor grp, caf::blocking_actor *act) {
            return [=](utility::event_atom, last_changed_atom, const time_point &last_changed) {
                if (last_changed > last_changed_) {
                    last_changed_ = last_changed;
                    act->send(grp, utility::event_atom_v, last_changed_atom_v, last_changed_);
                }
            };
        }

        auto make_ignore_error_handler() {
            return [=](const caf::error) {};
        }

        auto make_get_uuid_handler() {
            return [=](uuid_atom) -> Uuid { return uuid_; };
        }
        auto make_get_type_handler() {
            return [=](type_atom) -> std::string { return type_; };
        }

        auto
        make_get_detail_handler(caf::event_based_actor *act, caf::actor group = caf::actor()) {
            return [=](detail_atom) -> ContainerDetail { return detail(act, group); };
        }
        auto
        make_get_detail_handler(caf::blocking_actor *act, caf::actor group = caf::actor()) {
            return [=](detail_atom) -> ContainerDetail { return detail(act, group); };
        }

        bool operator==(const Container &other) const {
            return name_ == other.name_ and type_ == other.type_ and uuid_ == other.uuid_;
        }

        bool operator!=(const Container &other) const {
            return not(name_ == other.name_ and type_ == other.type_ and uuid_ == other.uuid_);
        }

        inline friend std::ostream &operator<<(std::ostream &os, const Container &ct);

        template <class Inspector> friend bool inspect(Inspector &f, Container &x) {
            return f.object(x).fields(
                f.field("name", x.name_), f.field("type", x.type_), f.field("uuid", x.uuid_));
        }

      private:
        std::string name_;
        std::string type_;
        utility::Uuid uuid_;
        semver::version version_;
        semver::version file_version_;
        time_point last_changed_ = {utility::clock::now()};
    };

    std::ostream &operator<<(std::ostream &out, const Container &rhs) {
        return out << to_string(rhs.uuid_) << " " << rhs.name_ << " " << rhs.type_;
    }


    class PlaylistItem {
      public:
        PlaylistItem(
            const std::string name   = "",
            const std::string type   = "",
            const utility::Uuid uuid = utility::Uuid::generate(),
            const std::string flag   = "#00000000")
            : name_(std::move(name)),
              type_(std::move(type)),
              flag_(std::move(flag)),
              uuid_(std::move(uuid)) {}

        explicit PlaylistItem(const utility::JsonStore &jsn);
        virtual ~PlaylistItem() = default;

        [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
        [[nodiscard]] std::string name() const { return name_; }
        [[nodiscard]] std::string type() const { return type_; }
        [[nodiscard]] std::string flag() const { return flag_; }
        void set_name(const std::string &name) { name_ = name; }
        void set_flag(const std::string &flag) { flag_ = flag; }
        void set_uuid(const utility::Uuid &uuid) { uuid_ = uuid; }

        [[nodiscard]] virtual utility::JsonStore serialise() const;

        bool operator==(const PlaylistItem &other) const {
            return name_ == other.name_ and type_ == other.type_ and flag_ == other.flag_ and
                   uuid_ == other.uuid_;
        }

        bool operator!=(const PlaylistItem &other) const {
            return not(
                name_ == other.name_ and type_ == other.type_ and flag_ == other.flag_ and
                uuid_ == other.uuid_);
        }

        inline friend std::ostream &operator<<(std::ostream &os, const PlaylistItem &ct);

        template <class Inspector> friend bool inspect(Inspector &f, PlaylistItem &x) {
            return f.object(x).fields(
                f.field("name", x.name_),
                f.field("type", x.type_),
                f.field("flag", x.flag_),
                f.field("uuid", x.uuid_));
        }

      private:
        std::string name_;
        std::string type_;
        std::string flag_;
        utility::Uuid uuid_;
    };

    inline std::string to_string(const PlaylistItem &pli) {
        return std::string(fmt::format(
            "uuid: {} name: {} type: {} flag: {}",
            to_string(pli.uuid()),
            pli.name(),
            pli.type(),
            pli.flag()));
    }


    std::ostream &operator<<(std::ostream &out, const PlaylistItem &rhs) {
        return out << to_string(rhs.uuid_) << " " << rhs.name_ << " " << rhs.type_ << " "
                   << rhs.flag_;
    }

    template <typename V> class UuidTree {
      public:
        UuidTree(const V value = V(), const utility::Uuid uuid = utility::Uuid::generate());
        UuidTree(const JsonStore &jsn);
        virtual ~UuidTree() = default;

        [[nodiscard]] utility::Uuid uuid() const { return uuid_; }
        [[nodiscard]] V value() const { return value_; }
        V &value() { return value_; }

        [[nodiscard]] bool empty() const { return children_.empty(); }

        void reset_uuid(const bool recursive = false);

        void set_uuid(const utility::Uuid &uuid) { uuid_ = uuid; }
        void set_value(const V &value) { value_ = value; }

        [[nodiscard]] size_t count(const utility::Uuid &uuid) const;
        [[nodiscard]] size_t size() const { return children_.size(); }

        [[nodiscard]] std::list<UuidTree<V>> children() const { return children_; }
        std::list<UuidTree<V>> &children_ref() { return children_; }
        [[nodiscard]] const std::list<UuidTree<V>> &children_ref() const { return children_; }

        bool remove(const utility::Uuid &uuid, const bool recursive = false);
        bool move(
            const utility::Uuid &uuid,
            const utility::Uuid &uuid_before = utility::Uuid(),
            const bool into                  = false);
        [[nodiscard]] std::optional<UuidTree<V>> copy(const utility::Uuid &uuid) const;

        std::optional<utility::Uuid> insert(
            const V &value, const utility::Uuid &uuid_before = Uuid(), const bool into = false);
        bool insert(
            const UuidTree<V> &ut,
            const utility::Uuid &uuid_before = Uuid(),
            const bool into                  = false);

        [[nodiscard]] JsonStore serialise() const;

        [[nodiscard]] utility::UuidVector
        uuids(const UuidTree<V> &child, const bool recursive = false) const;
        [[nodiscard]] utility::UuidVector uuids(const bool recursive = false) const {
            return uuids(*this, recursive);
        }

        [[nodiscard]] std::optional<UuidTree<V>>
        intersect(const std::vector<Uuid> &uuids) const;

        std::optional<UuidTree<V> *> find(const utility::Uuid &uuid);
        [[nodiscard]] std::optional<const UuidTree<V> *> cfind(const utility::Uuid &uuid) const;

        [[nodiscard]] std::optional<Uuid>
        find_next_at_same_depth(const utility::Uuid &uuid) const;

        template <typename C>
        inline friend std::ostream &operator<<(std::ostream &os, const UuidTree<C> &ct);

        bool operator==(const UuidTree<V> &other) const {
            return value_ == other.value_ and children_ == other.children_;
        }

        bool operator!=(const UuidTree<V> &other) const {
            return not(value_ == other.value_ and children_ == other.children_);
        }

        std::ostream &dump(std::ostream &os, const size_t depth = 0) const {
            os << std::setw(depth * 4) << "" << to_string(uuid()) << " " << value_ << std::endl;
            for (const auto &i : children_)
                i.dump(os, depth + 1);
            return os;
        }

        template <class Inspector> friend bool inspect(Inspector &f, UuidTree<V> &x) {
            return f.object(x).fields(
                f.field("uuid", x.uuid_),
                f.field("value", x.value_),
                f.field("children", x.children_));
        }

      public:
        utility::Uuid uuid_;
        V value_;
        std::list<UuidTree<V>> children_;
    };

    class PlaylistTree : public UuidTree<PlaylistItem> {
      public:
        PlaylistTree(const utility::JsonStore &jsn) : UuidTree<PlaylistItem>(jsn) {}

        PlaylistTree(
            const std::string name   = "",
            const std::string type   = "",
            const utility::Uuid uuid = utility::Uuid())
            : UuidTree<PlaylistItem>(
                  PlaylistItem(std::move(name), std::move(type), std::move(uuid))) {}
        PlaylistTree(const UuidTree<PlaylistItem> &t) : UuidTree<PlaylistItem>(t) {}

        ~PlaylistTree() override = default;

        [[nodiscard]] std::string name() const { return value_.name(); }
        [[nodiscard]] utility::Uuid value_uuid() const { return value_.uuid(); }
        [[nodiscard]] std::string type() const { return value_.type(); }
        [[nodiscard]] std::string flag() const { return value_.flag(); }

        void set_flag(const std::string &flag) { value_.set_flag(flag); }
        void set_name(const std::string &name) { value_.set_name(name); }

        bool rename(
            const std::string &name, const utility::Uuid &uuid, UuidTree<PlaylistItem> &child);
        bool rename(const std::string &name, const utility::Uuid &uuid) {
            return rename(name, uuid, *this);
        }

        bool reflag(
            const std::string &flag, const utility::Uuid &uuid, UuidTree<PlaylistItem> &child);
        bool reflag(const std::string &flag, const utility::Uuid &uuid) {
            return reflag(flag, uuid, *this);
        }

        template <class Inspector> friend bool inspect(Inspector &f, PlaylistTree &x) {
            return f.object(x).fields(
                f.field("uuid", x.uuid_),
                f.field("value", x.value_),
                f.field("children", x.children_));
        }
        [[nodiscard]] utility::UuidVector
        children_uuid(const UuidTree<PlaylistItem> &child, const bool recursive = false) const;
        [[nodiscard]] utility::UuidVector children_uuid(const bool recursive = false) const {
            return children_uuid(*this, recursive);
        }

        [[nodiscard]] std::optional<UuidTree<PlaylistItem> *>
        find_value(const utility::Uuid &uuid);
        [[nodiscard]] std::optional<const UuidTree<PlaylistItem> *>
        cfind_value(const utility::Uuid &uuid) const;

        [[nodiscard]] std::optional<UuidTree<PlaylistItem> *>
        find_any(const utility::Uuid &uuid);
        [[nodiscard]] std::optional<const UuidTree<PlaylistItem> *>
        cfind_any(const utility::Uuid &uuid) const;

        [[nodiscard]] std::optional<Uuid>
        find_next_value_at_same_depth(const utility::Uuid &uuid) const;
    };

    inline std::string to_string(const PlaylistTree &plt) {
        std::string children = "";
        for (const auto &i : plt.children_ref()) {
            children += "{" + to_string(i) + "}";
        }
        return std::string(fmt::format(
            "uuid: {} value: {} children: {}",
            to_string(plt.uuid()),
            to_string(plt.value()),
            children));
    }

    template <typename U> std::ostream &operator<<(std::ostream &out, const UuidTree<U> &rhs) {
        return rhs.dump(out);
    }

    struct CopyResult {
        CopyResult()  = default;
        ~CopyResult() = default;
        CopyResult(
            utility::PlaylistTree tree,
            utility::UuidActorVector containers,
            utility::UuidActorVector media,
            utility::UuidUuidMap media_map)
            : tree_(std::move(tree)),
              containers_(std::move(containers)),
              media_(std::move(media)),
              media_map_(std::move(media_map)) {}

        utility::PlaylistTree tree_;
        utility::UuidActorVector containers_;
        utility::UuidActorVector media_;
        utility::UuidUuidMap media_map_;

        template <class Inspector> friend bool inspect(Inspector &f, CopyResult &x) {
            return f.object(x).fields(
                f.field("t", x.tree_),
                f.field("c", x.containers_),
                f.field("m", x.media_),
                f.field("mm", x.media_map_));
        }

        void push_back(struct CopyResult cr) {
            tree_.insert(cr.tree_);
            containers_.insert(containers_.end(), cr.containers_.begin(), cr.containers_.end());
            media_.insert(media_.end(), cr.media_.begin(), cr.media_.end());
            media_map_.merge(cr.media_map_);
        }
    };


    template <typename V>
    UuidTree<V>::UuidTree(const V value, const utility::Uuid uuid)
        : uuid_(std::move(uuid)), value_(std::move(value)) {}

    template <typename V>
    UuidTree<V>::UuidTree(const utility::JsonStore &jsn)
        : value_(static_cast<JsonStore>(jsn["value"])) {
        uuid_ = jsn["uuid"];
        for (const auto &i : jsn["children"]) {
            children_.emplace_back(UuidTree(utility::JsonStore(i)));
        }
    }

    template <typename V> size_t UuidTree<V>::count(const utility::Uuid &uuid) const {
        size_t _count = 0;
        if (uuid_ == uuid) {
            _count++;
        } else {
            for (const auto &i : children_) {
                _count += i.count(uuid);
                if (_count)
                    break;
            }
        }

        return _count;
    }

    template <typename V>
    std::optional<UuidTree<V> *> UuidTree<V>::find(const utility::Uuid &uuid) {
        if (uuid == uuid_)
            return this;

        auto i = std::begin(children_);
        for (; i != std::end(children_); ++i) {
            if (i->uuid_ == uuid) {
                return &(*i);
            }
            auto tmp = i->find(uuid_);
            if (tmp)
                return *tmp;
        }
        return {};
    }

    template <typename V>
    std::optional<const UuidTree<V> *> UuidTree<V>::cfind(const utility::Uuid &uuid) const {
        if (uuid == uuid_)
            return this;

        auto i = std::cbegin(children_);
        for (; i != std::cend(children_); ++i) {
            if (i->uuid_ == uuid) {
                return &(*i);
            }
            auto tmp = i->cfind(uuid_);
            if (tmp)
                return *tmp;
        }
        return {};
    }

    template <typename V>
    std::optional<Uuid> UuidTree<V>::find_next_at_same_depth(const utility::Uuid &uuid) const {
        // given a uuid, find the uuid of the next item in the tree at the
        // same depth
        auto i = std::cbegin(children_);
        for (; i != std::cend(children_); ++i) {
            if (i->uuid_ == uuid) {
                i++;
                if (i != std::cend(children_)) {
                    return i->uuid_;
                }
                // found item is at end of children_ list, so try
                // using the previous item in children
                i--;
                if (i != std::cbegin(children_)) {
                    return i->uuid_;
                }
                // fail, as there is only one chid
            }
            auto tmp = i->find_next_at_same_depth(uuid);
            if (tmp)
                return *tmp;
        }
        return {};
    }

    template <typename V>
    utility::UuidVector
    UuidTree<V>::uuids(const UuidTree<V> &child, const bool recursive) const {
        utility::UuidVector result;

        for (const auto &i : child.children_ref()) {
            if (recursive) {
                auto tmp = uuids(i, recursive);
                result.insert(result.end(), tmp.begin(), tmp.end());
            }
            result.push_back(i.uuid());
        }

        return result;
    }

    template <typename V> void UuidTree<V>::reset_uuid(const bool recursive) {
        uuid_.generate_in_place();
        if (recursive) {
            for (auto i = std::begin(children_); i != std::end(children_); ++i) {
                i->reset_uuid(recursive);
            }
        }
    }


    template <typename V>
    bool UuidTree<V>::remove(const utility::Uuid &uuid, const bool recursive) {
        bool deleted = false;

        for (auto i = std::begin(children_); i != std::end(children_); ++i) {
            if (i->uuid_ == uuid) {
                if (recursive) {
                    children_.erase(i);
                } else {
                    children_.splice(i, i->children_ref());
                    children_.erase(i);
                }
                deleted = true;
                break;
            } else {
                deleted = i->remove(uuid, recursive);
                if (deleted)
                    break;
            }
        }

        return deleted;
    }

    template <typename V>
    std::optional<utility::Uuid>
    UuidTree<V>::insert(const V &value, const utility::Uuid &uuid_before, const bool into) {
        UuidTree<V> utv(value);
        if (insert(utv, uuid_before, into))
            return utv.uuid_;

        return {};
    }

    template <typename V>
    bool UuidTree<V>::insert(
        const UuidTree<V> &ut, const utility::Uuid &uuid_before, const bool into) {
        bool result = false;

        if (uuid_before.is_null()) {
            children_.push_back(ut);
            result = true;
        } else {
            // scan each child..
            for (auto i = std::begin(children_); i != std::end(children_); ++i) {
                if (i->uuid_ == uuid_before) {
                    if (into) {
                        i->insert(ut);
                        result = true;
                    } else {
                        children_.insert(i, ut);
                        result = true;
                    }
                    break;
                } else {
                    result = i->insert(ut, uuid_before, into);
                    if (result)
                        break;
                }
            }
        }

        return result;
    }
    template <typename V>
    bool UuidTree<V>::move(
        const utility::Uuid &uuid, const utility::Uuid &uuid_before, const bool into) {
        bool moved = false;
        auto c     = copy(uuid);
        if (c) {
            // make sure we're not copying into ourself..
            if (uuid_before.is_null() or not c->count(uuid_before)) {
                // remove old copy..
                remove(uuid, true);
                insert(*c, uuid_before, into);
                moved = true;
            }
        }

        return moved;
    }

    template <typename V>
    std::optional<UuidTree<V>> UuidTree<V>::copy(const utility::Uuid &uuid) const {
        if (uuid_ == uuid)
            return *this;

        for (const auto &i : children_) {
            auto c = i.copy(uuid);
            if (c)
                return *c;
        }

        return {};
    }

    template <typename V> JsonStore UuidTree<V>::serialise() const {
        JsonStore jsn;
        jsn["uuid"]     = uuid_;
        jsn["value"]    = value_.serialise();
        jsn["children"] = nlohmann::json::array();

        for (const auto &i : children_)
            jsn["children"].push_back(i.serialise());

        return jsn;
    }

    template <typename V>
    std::optional<UuidTree<V>> UuidTree<V>::intersect(const std::vector<Uuid> &uuids) const {
        // build new tree from selection

        std::list<UuidTree<V>> children;

        for (const auto &i : children_) {
            auto ii = i.intersect(uuids);
            if (ii)
                children.emplace_back(*ii);
        }

        if (not children.empty() or
            std::find(uuids.begin(), uuids.end(), uuid_) != uuids.end()) {
            // preserve..
            auto result      = UuidTree<V>();
            result.uuid_     = uuid_;
            result.value_    = value_;
            result.children_ = children;
            return result;
        }

        return {};
    }

} // namespace utility
} // namespace xstudio
