// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>

#include <semver.hpp>

#include <caf/all.hpp>

#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/plugin_manager/enums.hpp"

// Use the following macros to declare plugins.
// See src/plugin/data_source/demo/demo_datasource_plugin/src/demo_plugin.cpp for example useage

#define XSTUDIO_PLUGIN_DECLARE_BEGIN()                                                                 \
    extern "C" {                                                                               \
    plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {                 \
                                                                                               \
        auto pfc = new plugin_manager::PluginFactoryCollection();

#define XSTUDIO_PLUGIN_DECLARE_END()                                                                   \
    return pfc;                                                                                \
    }                                                                                          \
    }

#define XSTUDIO_REGISTER_PLUGIN(                                                                       \
    PLUGIN_CLASS, UUID, NAME, TYPE, RESIDENT, AUTHOR, DESCRIPTION, VERSION)                    \
    pfc->register_plugin<PLUGIN_CLASS>(                                                        \
        UUID, #NAME, TYPE, RESIDENT, #AUTHOR, #DESCRIPTION, semver::version(#VERSION));

#define XSTUDIO_REGISTER_MEDIA_READER_PLUGIN(PLUGIN_CLASS, UUID, NAME, AUTHOR, DESCRIPTION, VERSION)   \
    pfc->register_plugin<media_reader::MediaReaderActor<PLUGIN_CLASS>>(                        \
        UUID,                                                                                  \
        #NAME,                                                                                 \
        plugin_manager::PluginFlags::PF_MEDIA_READER,                                          \
        false,                                                                                 \
        #AUTHOR,                                                                               \
        #DESCRIPTION,                                                                          \
        semver::version(#VERSION));


namespace xstudio {
namespace plugin_manager {

    class PluginFactory {
      public:
        PluginFactory()          = default;
        virtual ~PluginFactory() = default;

        [[nodiscard]] virtual std::string name() const        = 0;
        [[nodiscard]] virtual utility::Uuid uuid() const      = 0;
        [[nodiscard]] virtual PluginType type() const         = 0;
        [[nodiscard]] virtual bool resident() const           = 0;
        [[nodiscard]] virtual std::string author() const      = 0;
        [[nodiscard]] virtual std::string description() const = 0;
        [[nodiscard]] virtual semver::version version() const = 0;

        // optional
        [[nodiscard]] virtual caf::actor
        spawn(caf::blocking_actor &, const utility::JsonStore & = utility::JsonStore()) {
            return caf::actor();
        }

        virtual void *instance_q_object(void *parent_q_object) { return nullptr; }
    };

    template <typename T> class PluginFactoryTemplate : public PluginFactory {
      public:
        PluginFactoryTemplate(
            utility::Uuid uuid,
            std::string _name       = "",
            PluginType type         = PluginFlags::PF_CUSTOM,
            bool resident           = false,
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : uuid_(std::move(uuid)),
              name_(std::move(_name)),
              type_(type),
              resident_(resident),
              author_(std::move(author)),
              description_(std::move(description)),
              version_(std::move(version)) {}
        ~PluginFactoryTemplate() override = default;

        [[nodiscard]] std::string name() const override { return name_; }
        [[nodiscard]] utility::Uuid uuid() const override { return uuid_; }
        [[nodiscard]] PluginType type() const override { return type_; }
        [[nodiscard]] bool resident() const override { return resident_; }
        [[nodiscard]] std::string author() const override { return author_; }
        [[nodiscard]] std::string description() const override { return description_; }
        [[nodiscard]] semver::version version() const override { return version_; }
        [[nodiscard]] caf::actor spawn(
            caf::blocking_actor &sys,
            const utility::JsonStore &json = utility::JsonStore()) override {
            return sys.spawn<T>(json);
        }

        utility::Uuid uuid_;
        std::string name_;
        PluginType type_;
        bool resident_;
        std::string author_;
        std::string description_;
        semver::version version_;
        std::string ui_widget_string_;
        std::string ui_menu_string_;

      private:
        caf::actor instance_;
    };


    template <typename T> class PluginFactoryTemplate2 : public PluginFactory {
      public:
        PluginFactoryTemplate2(
            utility::Uuid uuid,
            std::string name        = "",
            PluginType type         = PluginFlags::PF_CUSTOM,
            bool resident           = false,
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0"))
            : uuid_(std::move(uuid)),
              name_(std::move(name)),
              type_(type),
              resident_(resident),
              author_(std::move(author)),
              description_(std::move(description)),
              version_(std::move(version)) {}
        ~PluginFactoryTemplate2() override = default;

        [[nodiscard]] std::string name() const override { return name_; }
        [[nodiscard]] utility::Uuid uuid() const override { return uuid_; }
        [[nodiscard]] PluginType type() const override { return type_; }
        [[nodiscard]] bool resident() const override { return resident_; }
        [[nodiscard]] std::string author() const override { return author_; }
        [[nodiscard]] std::string description() const override { return description_; }
        [[nodiscard]] semver::version version() const override { return version_; }

        void *instance_q_object(void *parent_q_object) override {
            return T::instance_q_object(parent_q_object);
        }

        utility::Uuid uuid_;
        std::string name_;
        PluginType type_;
        bool resident_;
        std::string author_;
        std::string description_;
        semver::version version_;
    };


    typedef PluginFactory *(*plugin_factory_ptr)();

    class PluginFactoryCollection {
      public:
        PluginFactoryCollection(
            std::vector<std::shared_ptr<PluginFactory>> vpf =
                std::vector<std::shared_ptr<PluginFactory>>())
            : factories_(std::move(vpf)) {}
        virtual ~PluginFactoryCollection() = default;

        template <typename T>
        void register_plugin(
            utility::Uuid uuid,
            std::string name        = "",
            PluginType type         = PluginFlags::PF_CUSTOM,
            bool resident           = false,
            std::string author      = "",
            std::string description = "",
            semver::version version = semver::version("0.0.0")) {
            factories_.push_back(
                std::make_shared<PluginFactoryTemplate<T>>(
                    uuid, name, type, resident, author, description, version));
        }

        std::vector<std::shared_ptr<PluginFactory>> &factories() { return factories_; }

      protected:
        std::vector<std::shared_ptr<PluginFactory>> factories_;
    };

    typedef PluginFactoryCollection *(*plugin_factory_collection_ptr)();

} // namespace plugin_manager
} // namespace xstudio
