// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <regex>

#include "xstudio/atoms.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio;
using namespace xstudio::plugin_manager;
using namespace xstudio::utility;

class TestActor : public caf::event_based_actor {
  public:
    TestActor(caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore())
        : caf::event_based_actor(cfg) {
        spdlog::debug("Created TestActor");
        print_on_exit(this, "TestActor");

        behavior_.assign(
            [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](name_atom) -> std::string { return "hello"; });
    }

    caf::behavior make_behavior() override { return behavior_; }

  private:
    caf::behavior behavior_;
};

class TestPlugin : public PluginFactory {
  public:
    TestPlugin() : PluginFactory() {}
    ~TestPlugin() override = default;

    [[nodiscard]] std::string name() const override { return "hello"; }
    [[nodiscard]] utility::Uuid uuid() const override {
        return Uuid("17e4323c-8ee7-4d9c-b74a-57ba805c10e8");
    }
    [[nodiscard]] PluginType type() const override { return PluginFlags::PF_CUSTOM; }
    [[nodiscard]] bool resident() const override { return false; }
    [[nodiscard]] std::string author() const override { return "author"; }
    [[nodiscard]] std::string description() const override { return "description"; }
    [[nodiscard]] semver::version version() const override { return semver::version("0.1.0"); }

    [[nodiscard]] caf::actor spawn(
        caf::blocking_actor &sys,
        const utility::JsonStore &json = utility::JsonStore()) override {
        return sys.spawn<TestActor>(json);
    }
};

class TestPFC : public PluginFactoryCollection {
  public:
    TestPFC() : PluginFactoryCollection() {
        factories_.emplace_back(std::make_shared<TestPlugin>());
        factories_.emplace_back(std::make_shared<PluginFactoryTemplate<TestActor>>(
            Uuid("e4e1d569-2338-4e6e-b127-5a9688df161a"), "template_test"));
    }
};

extern "C" {
PluginFactoryCollection *plugin_factory_collection_ptr() { return new TestPFC(); }
}
