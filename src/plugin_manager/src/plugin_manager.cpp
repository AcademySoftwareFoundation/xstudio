// SPDX-License-Identifier: Apache-2.0
#include <dlfcn.h>
#include <filesystem>

#include <fstream>
#include <iostream>

#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::plugin_manager;
using namespace xstudio::utility;

namespace fs = std::filesystem;


PluginManager::PluginManager(std::list<std::string> plugin_paths)
    : plugin_paths_(std::move(plugin_paths)) {}

size_t PluginManager::load_plugins() {
    // scan for .so for each path.
    size_t loaded = 0;
    for (const auto &path : plugin_paths_) {
        try {
            // read dir content..
            for (const auto &entry : fs::directory_iterator(path)) {
                if (not fs::is_regular_file(entry.status()) or
                    not(entry.path().extension() == ".so"))
                    continue;

                // only want .so
                // clear any errors..
                dlerror();

                // open .so
                void *hndl = dlopen(entry.path().c_str(), RTLD_NOW);
                if (hndl == nullptr) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, dlerror());
                    continue;
                }


                plugin_factory_collection_ptr pfcp;
                *(void **)(&pfcp) = dlsym(hndl, "plugin_factory_collection_ptr");
                if (pfcp == nullptr) {
                    spdlog::debug("{} {}", __PRETTY_FUNCTION__, dlerror());
                    dlclose(hndl);
                    continue;
                }

                PluginFactoryCollection *pfc = nullptr;
                try {
                    pfc = pfcp();
                    for (const auto &i : pfc->factories()) {
                        if (not factories_.count(i->uuid())) {
                            // new plugin..
                            loaded++;
                            factories_.emplace(i->uuid(), PluginEntry(i, entry.path()));
                            spdlog::debug(
                                "Add plugin {} {} {}",
                                to_string(i->uuid()),
                                i->name(),
                                entry.path().string());
                        } else {
                            spdlog::warn(
                                "Ignore duplicate plugin {} {}",
                                i->name(),
                                entry.path().string());
                        }
                    }
                } catch (const std::exception &err) {
                    spdlog::warn(
                        "{} Failed to init plugin {} {}",
                        __PRETTY_FUNCTION__,
                        entry.path().c_str(),
                        err.what());
                }
                if (pfc)
                    delete pfc;
            }
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }
    return loaded;
}

caf::actor PluginManager::spawn(
    caf::blocking_actor &sys, const utility::Uuid &uuid, const utility::JsonStore &json) {

    auto spawned = caf::actor();
    if (factories_.count(uuid))
        spawned = factories_.at(uuid).factory()->spawn(sys, json);
    else
        throw std::runtime_error("Invalid plugin uuid");

    return spawned;
}

std::string PluginManager::spawn_widget_ui(const utility::Uuid &uuid) {
    if (factories_.count(uuid))
        return factories_.at(uuid).factory()->spawn_widget_ui();
    return "";
}

std::string PluginManager::spawn_menu_ui(const utility::Uuid &uuid) {
    if (factories_.count(uuid))
        return factories_.at(uuid).factory()->spawn_menu_ui();
    return "";
}
