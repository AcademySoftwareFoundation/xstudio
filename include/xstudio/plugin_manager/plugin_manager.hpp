// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <list>
#include <string>

#include "xstudio/plugin_manager/plugin_factory.hpp"

namespace xstudio {
namespace plugin_manager {

    class PluginEntry {
      public:
        PluginEntry(std::shared_ptr<PluginFactory> pf, std::string path)
            : plugin_factory_(std::move(pf)), path_(std::move(path)) {
            // auto enable xstudio plugins
            // we might want to use uuids list.
            if (plugin_factory_.get()->author() != "xStudio")
                enabled_ = false;
        }

        virtual ~PluginEntry() = default;

        [[nodiscard]] std::string path() const { return path_; }
        [[nodiscard]] PluginFactory *factory() const { return plugin_factory_.get(); }
        [[nodiscard]] bool enabled() const { return enabled_; }
        void set_enabled(const bool enable) { enabled_ = enable; }

      private:
        std::shared_ptr<PluginFactory> plugin_factory_;
        std::string path_;
        bool enabled_{true};
    };

    class PluginDetail {
      public:
        PluginDetail()          = default;
        virtual ~PluginDetail() = default;

        PluginDetail(const PluginEntry &pe)
            : enabled_(pe.enabled()),
              path_(pe.path()),
              uuid_(pe.factory()->uuid()),
              name_(pe.factory()->name()),
              type_(pe.factory()->type()),
              resident_(pe.factory()->resident()),
              author_(pe.factory()->author()),
              description_(pe.factory()->description()),
              version_(pe.factory()->version()) {}

        bool enabled_;
        std::string path_;
        utility::Uuid uuid_;
        std::string name_;
        PluginType type_;
        bool resident_;
        std::string author_;
        std::string description_;
        semver::version version_;

        template <class Inspector> friend bool inspect(Inspector &f, PluginDetail &x) {
            return f.object(x).fields(
                f.field("enabled", x.enabled_),
                f.field("path", x.path_),
                f.field("uuid", x.uuid_),
                f.field("name", x.name_),
                f.field("type", x.type_),
                f.field("resident", x.resident_),
                f.field("author", x.author_),
                f.field("description", x.description_),
                f.field("version", x.version_));
        }
    };

    class PluginManager {
      public:
        PluginManager(std::list<std::string> plugin_paths = std::list<std::string>());
        virtual ~PluginManager() = default;

        void emplace_back_path(const std::string plugin_path) {
            plugin_paths_.emplace_back(plugin_path);
        }
        void emplace_front_path(const std::string plugin_path) {
            plugin_paths_.emplace_front(plugin_path);
        }
        size_t load_plugins();

        std::list<std::string> &plugin_paths() { return plugin_paths_; }
        std::vector<PluginDetail> plugin_detail() {
            std::vector<PluginDetail> details;

            for (const auto &i : factories_)
                details.emplace_back(PluginDetail(i.second));

            return details;
        }


        std::map<utility::Uuid, PluginEntry> &factories() { return factories_; }
        [[nodiscard]] caf::actor spawn(
            caf::blocking_actor &sys,
            const utility::Uuid &uuid,
            const utility::JsonStore &json = utility::JsonStore());
        [[nodiscard]] std::string spawn_widget_ui(const utility::Uuid &uuid);
        [[nodiscard]] std::string spawn_menu_ui(const utility::Uuid &uuid);

      private:
        std::list<std::string> plugin_paths_;
        std::map<utility::Uuid, PluginEntry> factories_;
    };
} // namespace plugin_manager
} // namespace xstudio
