// SPDX-License-Identifier: Apache-2.0
#include <filesystem>

#include <regex>
#include <algorithm>
#include <set>

#include "xstudio/media_hook/media_hook.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/json_store.hpp"

namespace fs = std::filesystem;

using namespace xstudio;
using namespace xstudio::media_hook;
using namespace xstudio::utility;

/* This default media hook plugin does very little except to
ensure that the fallback ACES OpenColorIO config is enabled
and media are set to the 'Raw' colourspace as a starting point.

To see a more complex Hook that is applying various rules to set
colour management metadata and modifying media frame ranges see
the dneg/dnhook code for reference.
*/
class DefaultMediaHook : public MediaHook {
  public:
    DefaultMediaHook() : MediaHook("Default") {}
    ~DefaultMediaHook() override = default;

    std::optional<utility::MediaReference> modify_media_reference(
        const utility::MediaReference &mr, const utility::JsonStore &jsn) override {
        utility::MediaReference result = mr;
        auto changed                   = false;

        // here we can add our own logic to modify the MediaReference by returning
        // a modified copy. An example use case would be to automatically trim a
        // slate frame, for example, by changing the frames data in the MediaReference
        if (not changed)
            return {};

        return result;
    }

    /*
    Inject any metadata we desire into the metadata json structure
    */

    utility::JsonStore modify_metadata(
        const utility::MediaReference &mr, const utility::JsonStore &metadata) override {
        utility::JsonStore result = metadata;
        // Example code to get the filepath from the MediaReference
        const caf::uri &uri =
            mr.container() or mr.uris().empty() ? mr.uri() : mr.uris()[0].first;

        const std::string path            = to_string(uri);
        auto ppath                        = uri_to_posix_path(uri);
        result["colour_pipeline"]         = colour_params(path, metadata);
        result["colour_pipeline"]["path"] = path;
        return result;
    }

    /*
    Colour management is enabled at this entry point. We can return a json dict
    containing key/value pairs that drive the built-in OCIO plugin in xstudio.
    Refer to the src/plugin/colour_pipeline/ocio/README.md file for more details.
    */

    utility::JsonStore
    colour_params(const std::string &path, const utility::JsonStore &metadata) {

        utility::JsonStore r(R"({})"_json);
        // using the builtin ACES config grom OCIO, assigning the colourspace
        // to be Raw
        r["ocio_config"]      = "cg-config-v1.0.0_aces-v1.3_ocio-v2.1";
        r["input_colorspace"] = "Raw";
        return r;
    }
};

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<MediaHookPlugin<MediaHookActor<DefaultMediaHook>>>(
                Uuid("e4e1d569-2338-4e6e-b127-5a9688df161a"),
                "Default Media Hook",
                "Ted Waine",
                "Minimal Hook to set up ACES colour management.",
                semver::version("1.0.0"))}));
}
}