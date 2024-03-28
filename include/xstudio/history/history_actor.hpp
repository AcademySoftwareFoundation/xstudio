// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <set>

#include "xstudio/atoms.hpp"
#include "xstudio/history/history.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/uuid.hpp"

namespace xstudio {
namespace history {

    using namespace xstudio::utility;

    template <typename V> class HistoryActor : public caf::event_based_actor {
      public:
        HistoryActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        HistoryActor(
            caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "HistoryActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        History<V> base_;
    };

    template <typename V>
    HistoryActor<V>::HistoryActor(caf::actor_config &cfg, const utility::JsonStore &jsn)
        : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {

        init();
    }

    template <typename V>
    HistoryActor<V>::HistoryActor(caf::actor_config &cfg, const utility::Uuid &uuid)
        : caf::event_based_actor(cfg) {
        base_.set_uuid(uuid);

        init();
    }

    template <typename V> void HistoryActor<V>::init() {
        print_on_create(this, "HistoryActor");
        print_on_exit(this, "HistoryActor");

        behavior_.assign(
            base_.make_get_uuid_handler(),
            base_.make_get_type_handler(),
            make_get_event_group_handler(caf::actor()),
            base_.make_get_detail_handler(this, caf::actor()),

            [=](plugin_manager::enable_atom) -> bool { return base_.enabled(); },

            [=](plugin_manager::enable_atom, const bool enabled) -> bool {
                base_.set_enabled(enabled);
                return true;
            },

            [=](utility::clear_atom) -> bool {
                base_.clear();
                return true;
            },

            [=](media_cache::count_atom) -> int { return static_cast<int>(base_.count()); },

            [=](media_cache::count_atom, const int count) -> bool {
                base_.set_max_count(static_cast<size_t>(count));
                return true;
            },

            [=](undo_atom) -> result<V> {
                auto i = base_.undo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](redo_atom) -> result<V> {
                auto i = base_.redo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](log_atom, const V &value) -> bool {
                if (base_.enabled()) {
                    base_.push(value);
                    return true;
                }
                return false;
            },

            [=](utility::serialise_atom) -> utility::JsonStore {
                utility::JsonStore jsn;
                jsn["base"] = base_.serialise();
                return jsn;
            });
    }

    template <typename K, typename V> class HistoryMapActor : public caf::event_based_actor {
      public:
        HistoryMapActor(caf::actor_config &cfg, const utility::JsonStore &jsn);
        HistoryMapActor(
            caf::actor_config &cfg, const utility::Uuid &uuid = utility::Uuid::generate());

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "HistoryActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        HistoryMap<K, V> base_;
    };

    template <typename K, typename V>
    HistoryMapActor<K, V>::HistoryMapActor(
        caf::actor_config &cfg, const utility::JsonStore &jsn)
        : caf::event_based_actor(cfg), base_(static_cast<utility::JsonStore>(jsn["base"])) {

        init();
    }

    template <typename K, typename V>
    HistoryMapActor<K, V>::HistoryMapActor(caf::actor_config &cfg, const utility::Uuid &uuid)
        : caf::event_based_actor(cfg) {
        base_.set_uuid(uuid);

        init();
    }

    template <typename K, typename V> void HistoryMapActor<K, V>::init() {
        print_on_create(this, "HistoryActor");
        print_on_exit(this, "HistoryActor");

        behavior_.assign(
            base_.make_get_uuid_handler(),
            base_.make_get_type_handler(),
            make_get_event_group_handler(caf::actor()),
            base_.make_get_detail_handler(this, caf::actor()),

            [=](plugin_manager::enable_atom) -> bool { return base_.enabled(); },

            [=](plugin_manager::enable_atom, const bool enabled) -> bool {
                base_.set_enabled(enabled);
                return true;
            },

            [=](utility::clear_atom) -> bool {
                base_.clear();
                return true;
            },

            [=](media_cache::count_atom) -> int { return static_cast<int>(base_.count()); },

            [=](media_cache::count_atom, const int count) -> bool {
                base_.set_max_count(static_cast<size_t>(count));
                return true;
            },

            [=](undo_atom) -> result<V> {
                auto i = base_.undo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](redo_atom) -> result<V> {
                auto i = base_.redo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](undo_atom, const K &key) -> result<V> {
                auto i = base_.undo(key);
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](redo_atom, const K &key) -> result<V> {
                auto i = base_.redo(key);
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](log_atom, const K &key, const V &value) -> bool {
                if (base_.enabled()) {
                    base_.push(key, value);
                    return true;
                }
                return false;
            },

            [=](utility::serialise_atom) -> utility::JsonStore {
                utility::JsonStore jsn;
                jsn["base"] = base_.serialise();
                return jsn;
            });
    }

    template <> void HistoryMapActor<utility::sys_time_point, utility::JsonStore>::init() {
        print_on_create(this, "HistoryActor");
        print_on_exit(this, "HistoryActor");

        behavior_.assign(
            base_.make_get_uuid_handler(),
            base_.make_get_type_handler(),
            make_get_event_group_handler(caf::actor()),
            base_.make_get_detail_handler(this, caf::actor()),

            [=](plugin_manager::enable_atom) -> bool { return base_.enabled(); },

            [=](plugin_manager::enable_atom, const bool enabled) -> bool {
                base_.set_enabled(enabled);
                return true;
            },

            [=](utility::clear_atom) -> bool {
                base_.clear();
                return true;
            },

            [=](media_cache::count_atom) -> int { return static_cast<int>(base_.count()); },

            [=](media_cache::count_atom, const int count) -> bool {
                base_.set_max_count(static_cast<size_t>(count));
                return true;
            },

            [=](undo_atom) -> result<utility::JsonStore> {
                auto i = base_.undo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](undo_atom, const utility::sys_time_duration &duration)
                -> result<std::vector<utility::JsonStore>> {
                auto peek = base_.peek_undo();

                if (peek) {
                    auto result = std::vector<utility::JsonStore>();
                    while (true) {
                        auto next_peek = base_.peek_undo();
                        if (next_peek and *next_peek >= (*peek) - duration) {
                            peek   = next_peek;
                            auto i = base_.undo();
                            if (i) {
                                result.push_back(*i);
                            }
                        } else
                            break;
                    }
                    return result;
                }

                return make_error(xstudio_error::error, "No history");
            },


            [=](redo_atom, const utility::sys_time_duration &duration)
                -> result<std::vector<utility::JsonStore>> {
                auto peek = base_.peek_redo();

                if (peek) {
                    auto result = std::vector<utility::JsonStore>();
                    while (true) {
                        auto next_peek = base_.peek_redo();
                        if (next_peek and *next_peek >= (*peek) - duration) {
                            peek   = next_peek;
                            auto i = base_.redo();
                            if (i) {
                                result.push_back(*i);
                            }
                        } else
                            break;
                    }
                    return result;
                }

                return make_error(xstudio_error::error, "No history");
            },


            [=](redo_atom) -> result<utility::JsonStore> {
                auto i = base_.redo();
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](undo_atom, const utility::sys_time_point &key) -> result<utility::JsonStore> {
                auto i = base_.undo(key);
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](redo_atom, const utility::sys_time_point &key) -> result<utility::JsonStore> {
                auto i = base_.redo(key);
                if (i)
                    return *i;
                return make_error(xstudio_error::error, "No history");
            },

            [=](log_atom,
                const utility::sys_time_point &key,
                const utility::JsonStore &value) -> bool {
                if (base_.enabled()) {
                    base_.push(key, value);
                    return true;
                }
                return false;
            },

            [=](utility::serialise_atom) -> utility::JsonStore {
                utility::JsonStore jsn;
                jsn["base"] = base_.serialise();
                return jsn;
            });
    }


} // namespace history
} // namespace xstudio
