// SPDX-License-Identifier: Apache-2.0

#pragma once

// #include <array>
// #include <caf/uri.hpp>
// #include <cstddef>
// #include <map>
// #include <nlohmann/json.hpp>
// #include <optional>
// #include <string>

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/utility/media_reference.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace conform {

    // item json, we might need to expand this with more detail, may need to support clips.
    // might need a custom handler on items to generate more usful hints.
    typedef std::tuple<utility::JsonStore> ConformRequestItem;

    struct ConformRequest {
        ConformRequest(
            const utility::UuidActor playlist,
            const utility::JsonStore playlist_json,
            const std::vector<ConformRequestItem> items)
            : playlist_(std::move(playlist)),
              playlist_json_(std::move(playlist_json)),
              items_(std::move(items)) {}
        ConformRequest()  = default;
        ~ConformRequest() = default;

        utility::UuidActor playlist_;
        utility::JsonStore playlist_json_;
        std::vector< // request item
            ConformRequestItem>
            items_;

        template <class Inspector> friend bool inspect(Inspector &f, ConformRequest &x) {
            return f.object(x).fields(
                f.field("pl", x.playlist_),
                f.field("plj", x.playlist_json_),
                f.field("items", x.items_));
        }
    };

    typedef std::tuple<
        bool,                    // exists in playlist
        utility::MediaReference, // media json
        utility::UuidActor       // reference to media actor
        >
        ConformReplyItem;

    struct ConformReply {
        ConformReply()  = default;
        ~ConformReply() = default;
        std::vector<std::optional<std::vector<ConformReplyItem>>> items_;

        template <class Inspector> friend bool inspect(Inspector &f, ConformReply &x) {
            return f.object(x).fields(f.field("items", x.items_));
        }
    };

    class Conformer {
      public:
        Conformer(const utility::JsonStore &prefs = utility::JsonStore());
        virtual ~Conformer() = default;
        virtual void update_preferences(const utility::JsonStore &prefs);

        virtual ConformReply conform_request(
            const std::string &conform_task,
            const utility::JsonStore &conform_detail,
            const ConformRequest &request);

        virtual std::vector<std::string> conform_tasks();
    };

    template <typename T>
    class ConformPlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        ConformPlugin(
            utility::Uuid uuid,
            std::string name        = "",
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginType(plugin_manager::PluginFlags::PF_CONFORM),
                  false,
                  author,
                  description,
                  version) {}
        ~ConformPlugin() override = default;
    };

    template <typename T> class ConformPluginActor : public caf::event_based_actor {

      public:
        ConformPluginActor(
            caf::actor_config &cfg, const utility::JsonStore &jsn = utility::JsonStore())
            : caf::event_based_actor(cfg), conform_(jsn) {

            spdlog::debug("Created ConformPluginActor");
            utility::print_on_exit(this, "ConformPluginActor");

            {
                auto prefs = global_store::GlobalStoreHelper(system());
                utility::JsonStore js;
                utility::join_broadcast(this, prefs.get_group(js));
                conform_.update_preferences(js);
            }

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

                [=](conform_atom,
                    const std::string &conform_task,
                    const utility::JsonStore &conform_detail,
                    const ConformRequest &request) -> ConformReply {
                    return conform_.conform_request(conform_task, conform_detail, request);
                },

                [=](conform_tasks_atom) -> std::vector<std::string> {
                    return conform_.conform_tasks();
                },

                [=](json_store::update_atom,
                    const utility::JsonStore & /*change*/,
                    const std::string & /*path*/,
                    const utility::JsonStore &full) {
                    delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
                },

                [=](json_store::update_atom, const utility::JsonStore &js) {
                    try {
                        conform_.update_preferences(js);
                    } catch (const std::exception &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    }
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T conform_;
    };

} // namespace conform
} // namespace xstudio
