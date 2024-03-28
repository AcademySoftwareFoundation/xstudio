// SPDX-License-Identifier: Apache-2.0

#include "data_source_shotgun.tcc"

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<DataSourcePlugin<ShotgunDataSourceActor<ShotgunDataSource>>>(
                Uuid("33201f8d-db32-4278-9c40-8c068372a304"),
                "Shotgun",
                "DNeg",
                "Shotgun Data Source",
                semver::version("1.0.0"),
                "import Shotgun 1.0; ShotgunRoot {}"
                // "import Shotgun 1.0; ShotgunMenu {}"
                )}));
}
}
