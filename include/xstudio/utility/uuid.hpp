// SPDX-License-Identifier: Apache-2.0
#pragma once

/*
    \file uuid.h
    Class for dealing with UUID's
*/

#include <cstring>
#include <list>
#include <map>
#include <string>

#include <caf/actor.hpp>
#include <caf/actor_addr.hpp>
#include <caf/actor_cast.hpp>
#include <nlohmann/json.hpp>

#include "stduuid/uuid.h"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace utility {
    //! Uuid container class.
    /*! Handles Uuid operations, wraps C class.
     */
    struct Uuid {

        //! Underlying c struct
        uuids::uuid uuid_;

        //! Create NULL uuid
        Uuid() : uuid_() {}
        // ! Copy uuid
        //    \param other Clone uuid.

        // Uuid(const Uuid &other) { uuid_ = other.uuid_; }

        /*! Copy uuid
           \param other Clone uuid.
         */
        Uuid(const uuids::uuid other_uuid) { uuid_ = other_uuid; }

        /*! Copy uuid
           \param other Clone uuid.
         */
        Uuid(const std::string &other) {
            auto tmp = uuids::uuid::from_string(other);
            if (tmp)
                uuid_ = tmp.value();
        }

        //! \return Is uuid set
        [[nodiscard]] bool is_null() const { return uuid_.is_nil(); }

        //! clear uuid
        void clear() { uuid_ = uuids::uuid(); }

        //! Regenerate uuid
        void generate_in_place() { uuid_ = uuids::uuid_system_generator{}(); }

        //! Regenerate uuid
        void generate_in_place_from_name(const char *name) {
            uuid_ = uuids::uuid_name_generator{uuids::uuid()}(name);
        }

        //! Generate uuid
        static utility::Uuid generate() {
            utility::Uuid wrapped;
            wrapped.generate_in_place();
            return wrapped;
        }

        //! Generate uuid from name
        static utility::Uuid generate_from_name(const char *name) {
            utility::Uuid wrapped;
            wrapped.generate_in_place_from_name(name);
            return wrapped;
        }

        /*!
          \return Less than
        */
        bool operator<(const utility::Uuid &other) const { return uuid_ < other.uuid_; }
        //! \return  Equality
        bool operator==(const utility::Uuid &other) const { return uuid_ == other.uuid_; }
        //! \return  Inequality
        bool operator!=(const utility::Uuid &other) const { return uuid_ != other.uuid_; }

        //! \return Is uuid set
        explicit operator bool() const { return not uuid_.is_nil(); }

        /*! Regenerate from string.
           \param other Uuid string.
         */
        inline void from_string(const std::string &other) {
            auto tmp = uuids::uuid::from_string(other);

            if (tmp)
                uuid_ = tmp.value();
        }
    };


    //! Type instection for caf::serialise
    template <class Inspector> bool inspect(Inspector &f, utility::Uuid &x) {
        auto get_uuid = [&x]() -> decltype(auto) { return to_string(x.uuid_); };
        auto set_uuid = [&x](std::string value) {
            auto tmp = uuids::uuid::from_string(std::move(value));
            x.uuid_.swap(*tmp);
            return true;
        };
        return f.object(x).fields(f.field("uuid", get_uuid, set_uuid));
    }

    /*! to std::string from Uuid
        \param uuid Uuid to use
        \return std:string from uuid
    */
    extern std::string to_string(const utility::Uuid &uuid);

    /*! to nlohmann::json from Uuid
        \param j json to set
        \param uuid uuid to use
    */
    void to_json(nlohmann::json &j, const utility::Uuid &uuid);

    /*! from nlohmann::json to Uuid
      \param j json to use
      \param uuid uuid to set
    */
    void from_json(const nlohmann::json &j, utility::Uuid &uuid);

    using UuidActorAddr = std::pair<utility::Uuid, caf::actor_addr>;
    // using UuidActor        = std::pair<utility::Uuid, caf::actor>;
    using TUuidActor       = std::tuple<utility::Uuid, caf::actor>;
    using UuidActorMap     = std::map<utility::Uuid, caf::actor>;
    using UuidUuidMap      = std::map<utility::Uuid, Uuid>;
    using UuidActorAddrMap = std::map<utility::Uuid, caf::actor_addr>;
    using UuidList         = std::list<utility::Uuid>;
    using UuidVector       = std::vector<utility::Uuid>;
    using UuidSet          = std::set<utility::Uuid>;

    /*!
        UuidActor

        Convenience class for wrapping a caf::actor and its uuid in a single entity.
        Cast to uuid or actor as required or use accessor methods.
    */
    class UuidActor {

      public:
        UuidActor(const UuidActor &o) = default;
        UuidActor()                   = default;
        UuidActor(const utility::Uuid uuid, const caf::actor actor)
            : uuid_(std::move(uuid)), actor_(std::move(actor)) {}

        UuidActor(const utility::Uuid uuid, const caf::actor_addr &actor)
            : uuid_(std::move(uuid)), actor_(std::move(caf::actor_cast<caf::actor>(actor))) {}

        UuidActor &operator=(const UuidActor &o) = default;

        friend bool operator==(const UuidActor &a, const UuidActor &b) {
            return (a.uuid_ == b.uuid_ && a.actor_ == b.actor_);
        };

        friend bool operator!=(const UuidActor &a, const UuidActor &b) { return !(a == b); };

        operator caf::actor &() { return actor_; }
        operator const caf::actor &() const { return actor_; }
        operator utility::Uuid &() { return uuid_; }
        operator const utility::Uuid &() const { return uuid_; }
        operator bool() const { return bool(actor_); }

        utility::Uuid &uuid() { return uuid_; }
        [[nodiscard]] const utility::Uuid &uuid() const { return uuid_; }
        caf::actor &actor() { return actor_; }
        [[nodiscard]] const caf::actor &actor() const { return actor_; }

        [[nodiscard]] caf::actor_addr actor_addr() const {
            return caf::actor_cast<caf::actor_addr>(actor_);
        }

        template <class Inspector> friend bool inspect(Inspector &f, UuidActor &x) {
            return f.object(x).fields(f.field("uuid", x.uuid_), f.field("actor", x.actor_));
        }

        utility::Uuid uuid_;
        caf::actor actor_;

      private:
    };

    using UuidUuidActor   = std::pair<utility::Uuid, UuidActor>;
    using UuidActorVector = std::vector<UuidActor>;

    /*!
        Uuid list container class.
    */
    class UuidListContainer {
      public:
        /*! Construct from vector
            \param uuids Init from vector of uuids
        */
        UuidListContainer(
            const std::vector<utility::Uuid> &uuids = std::vector<utility::Uuid>());
        /*! Construct from JsonStore
            \param jsn Init from JsonStore
        */
        UuidListContainer(const JsonStore &jsn);
        virtual ~UuidListContainer() = default;

        /*!
          \return Serialised data.
        */
        [[nodiscard]] JsonStore serialise() const;

        /*!
          \param uuid Uuid to match.
          \return Count of matching uuids.
        */
        [[nodiscard]] size_t count(const utility::Uuid &uuid) const;
        /*!
          \param uuid Uuid to match.
          \return At least one instance.
        */
        [[nodiscard]] bool contains(const utility::Uuid &uuid) const;
        /*!
          \return List empty.
        */
        [[nodiscard]] bool empty() const { return uuids_.empty(); }
        /*!
          \return List size.
        */
        [[nodiscard]] size_t size() const { return uuids_.size(); }

        /*!
          \return next uuid size.
        */
        [[nodiscard]] utility::Uuid next_uuid(const utility::Uuid &uuid) const;

        /*! Insert new uuid
            \param uuid Uuid to insert
            \param uuid_before Insert before this Uuid
        */
        void insert(const utility::Uuid &uuid, const utility::Uuid &uuid_before = Uuid());

        /*! Remove uuid
            \param uuid Uuid to remove
            \return success
        */
        bool remove(const utility::Uuid &uuid);

        /*! move uuid
            \param from Uuid to move
            \param to Uuid to move
            \return success
        */
        bool swap(const utility::Uuid &from, const utility::Uuid &to);

        //! clear list
        void clear() { uuids_.clear(); }

        /*! Move  uuid
            \param uuid Uuid to move
            \param uuid_before Move before this Uuid
            \return success
        */
        bool move(const utility::Uuid &uuid, const utility::Uuid &uuid_before = Uuid());

        //! \return uuid list
        [[nodiscard]] const UuidList &uuids() const { return uuids_; }

        //! \return uuid vector
        [[nodiscard]] UuidVector uuid_vector() const;

        //! \return  Equality
        bool operator==(const UuidListContainer &other) const { return uuids_ == other.uuids_; }

      private:
        //! uuids
        UuidList uuids_;
    };
} // namespace utility
} // namespace xstudio

namespace std {
template <> struct hash<xstudio::utility::Uuid> {
    using argument_type = xstudio::utility::Uuid;
    using result_type   = std::size_t;

    result_type operator()(argument_type const &uuid) const {
        std::hash<std::string> hasher;
        return static_cast<result_type>(hasher(xstudio::utility::to_string(uuid)));
    }
};
} // namespace std
