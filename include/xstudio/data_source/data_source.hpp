// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <caf/all.hpp>
#include <cstddef>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"

namespace xstudio {
namespace data_source {

    using namespace caf;

    class DataSource {
      public:
        [[nodiscard]] std::string name() const { return name_; }
        virtual ~DataSource() = default;

        virtual utility::JsonStore get_data(const utility::JsonStore &) {
            throw std::runtime_error("Invalid operation");
            return utility::JsonStore();
        }
        virtual utility::JsonStore put_data(const utility::JsonStore &) {
            throw std::runtime_error("Invalid operation");
            return utility::JsonStore();
        }
        virtual utility::JsonStore post_data(const utility::JsonStore &) {
            throw std::runtime_error("Invalid operation");
            return utility::JsonStore();
        }
        virtual utility::JsonStore use_data(const utility::JsonStore &) {
            throw std::runtime_error("Invalid operation");
            return utility::JsonStore();
        }

      protected:
        DataSource(std::string name) : name_(std::move(name)) {}

      private:
        const std::string name_;
    };

    template <typename T>
    class DataSourcePlugin : public plugin_manager::PluginFactoryTemplate<T> {
      public:
        DataSourcePlugin(
            utility::Uuid uuid,
            std::string name             = "",
            std::string author           = "",
            std::string description      = "",
            semver::version version      = semver::version("0.0.0"),
            std::string widget_ui_string = "",
            std::string menu_ui_string   = "")
            : plugin_manager::PluginFactoryTemplate<T>(
                  uuid,
                  name,
                  plugin_manager::PluginFlags::PF_DATA_SOURCE,
                  true,
                  author,
                  description,
                  version,
                  widget_ui_string,
                  menu_ui_string) {}
        ~DataSourcePlugin() override = default;
    };

    template <typename T> class DataSourceActor : public caf::event_based_actor {
      public:
        DataSourceActor(
            caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore())
            : caf::event_based_actor(cfg) {

            spdlog::debug("Created DataSourceActor {}", data_source_.name());
            // print_on_exit(this, "MediaHookActor");

            behavior_.assign(
                [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
                [=](utility::name_atom) -> std::string { return data_source_.name(); },
                [=](get_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
                    try {
                        return data_source_.get_data(js);
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    return utility::JsonStore();
                },

                [=](use_data_atom, const caf::uri &) -> utility::UuidActorVector {
                    return utility::UuidActorVector();
                },

                [=](use_data_atom,
                    const caf::actor &media,
                    const utility::FrameRate &media_rate) -> result<utility::UuidActorVector> {
                    return utility::UuidActorVector();
                },

                [=](use_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
                    try {
                        return data_source_.use_data(js);
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    return utility::JsonStore();
                },

                [=](use_data_atom, const utility::JsonStore &, const bool)
                    -> result<utility::UuidActorVector> { return utility::UuidActorVector(); },

                [=](put_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
                    try {
                        return data_source_.put_data(js);
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    return utility::JsonStore();
                },
                [=](post_data_atom,
                    const utility::JsonStore &js) -> result<utility::JsonStore> {
                    try {
                        return data_source_.post_data(js);
                    } catch (const std::exception &err) {
                        return make_error(xstudio_error::error, err.what());
                    }
                    return utility::JsonStore();
                });
        }

        caf::behavior make_behavior() override { return behavior_; }

      private:
        caf::behavior behavior_;
        T data_source_;
    };

} // namespace data_source
} // namespace xstudio
