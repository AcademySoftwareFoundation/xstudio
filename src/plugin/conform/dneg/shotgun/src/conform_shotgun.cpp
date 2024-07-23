// SPDX-License-Identifier: Apache-2.0

#include "xstudio/conform/conformer.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"

using namespace xstudio;
using namespace xstudio::conform;
using namespace xstudio::utility;

class DNegConform : public Conformer {
  public:
    DNegConform(const utility::JsonStore &prefs = utility::JsonStore()) : Conformer(prefs) {}
    ~DNegConform() override = default;
    std::vector<std::string> conform_tasks() override {
        return std::vector<std::string>({"Test"});
    }

    ConformReply conform_request(
        const std::string &conform_task,
        const utility::JsonStore &conform_detail,
        const ConformRequest &request) override {
        spdlog::warn("conform_request {} {}", conform_task, conform_detail.dump(2));
        spdlog::warn("conform_request {}", request.playlist_json_.dump(2));

        for (const auto &i : request.items_) {
            spdlog::warn("conform_request {}", std::get<0>(i).dump(2));
        }

        return ConformReply();
    }
};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<ConformPlugin<ConformPluginActor<DNegConform>>>(
                Uuid("ebeecb15-75c0-4aa2-9cc7-1b3ad2491c39"),
                "DNeg",
                "DNeg",
                "DNeg Conformer",
                semver::version("1.0.0"))}));
}
}
