// SPDX-License-Identifier: Apache-2.0
#include <chrono>

#include "xstudio/atoms.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/colour_pipeline/colour_cache_actor.hpp"
#include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/time_cache.hpp"

using namespace xstudio;
using namespace std::chrono_literals;
using namespace xstudio::media_cache;
using namespace xstudio::global_store;
using namespace xstudio::utility;
using namespace xstudio::colour_pipeline;
using namespace caf;

GlobalColourCacheActor::GlobalColourCacheActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {
    size_t max_size  = std::numeric_limits<size_t>::max();
    size_t max_count = std::numeric_limits<size_t>::max();

    print_on_exit(this, "GlobalColourCacheActor");

    system().registry().put(colour_cache_registry, this);

    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        max_count = preference_value<size_t>(j, "/core/colour_cache/max_count");
        max_size  = preference_value<size_t>(j, "/core/colour_cache/max_size") * 1024 * 1024;
    } catch (...) {
    }

    cache_.set_max_size(max_size);
    cache_.set_max_count(max_count);

    behavior_.assign(
        [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

        [=](clear_atom) { cache_.clear(); },

        [=](count_atom) -> size_t { return cache_.count(); },

        [=](erase_atom, const std::string &key) { cache_.erase(key); },

        [=](erase_atom, const std::string &key, const utility::Uuid &uuid) {
            cache_.erase(key, uuid);
        },

        [=](erase_atom, const utility::Uuid &uuid) { cache_.erase(uuid); },

        [=](json_store::update_atom,
            const JsonStore & /*change*/,
            const std::string &path,
            const JsonStore &full) {
            try {
                if (starts_with(path, "/core/colour_cache/max_size") or
                    starts_with(path, "/core/colour_cache/max_count")) {
                    auto new_count =
                        preference_value<size_t>(full, "/core/colour_cache/max_count");
                    auto new_size =
                        preference_value<size_t>(full, "/core/colour_cache/max_size") * 1024 *
                        1024;
                    if (cache_.max_size() != new_size)
                        cache_.set_max_size(new_size);
                    if (cache_.max_count() != new_count)
                        cache_.set_max_count(new_count);
                }
            } catch (...) {
            }
        },

        [=](json_store::update_atom, const JsonStore &js) {
            delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, js, "", js);
        },

        [=](keys_atom) -> std::vector<std::string> { return cache_.keys(); },

        [=](preserve_atom, const std::string &key) -> bool { return cache_.preserve(key); },

        [=](preserve_atom, const std::string &key, const time_point &time) -> bool {
            //std::cerr << "K " << key << "\n";
            return cache_.preserve(key, time);
        },

        [=](preserve_atom, const std::string &key, const time_point &time, const Uuid &uuid)
            -> bool { 
                //std::cerr << "K1 " << key << "\n";
                return cache_.preserve(key, time, uuid); 
                },

        [=](retrieve_atom, const std::string &key) -> ColourOperationDataPtr {
                //std::cerr << "K2 " << key << "\n";

            return cache_.retrieve(key);
        },

        [=](retrieve_atom, const std::string &key, const time_point &time)
            -> ColourOperationDataPtr { 
                //std::cerr << "K3 " << key << "\n";
                return cache_.retrieve(key, time); 
                },

        [=](retrieve_atom, const std::string &key, const time_point &time, const Uuid &uuid)
            -> ColourOperationDataPtr { 
                                //std::cerr << "K4 " << key << "\n";

                return cache_.retrieve(key, time, uuid); 
                },

        [=](size_atom) -> size_t { return cache_.size(); },

        [=](store_atom, const std::string &key, ColourOperationDataPtr buf) -> bool {
            //std::cerr << "K5 " << key << "\n";
            return cache_.store(key, buf);
        },

        [=](store_atom,
            const std::string &key,
            ColourOperationDataPtr buf,
            const time_point &when) -> bool { 
                //std::cerr << "K6 " << key << "\n";
                return cache_.store(key, buf, when); 
                },

        [=](store_atom,
            const std::string &key,
            ColourOperationDataPtr buf,
            const time_point &when,
            const utility::Uuid &uuid) -> bool {
                //std::cerr << "K7 " << key << "\n";

            return cache_.store(key, buf, when, false, uuid);
        });
}


void GlobalColourCacheActor::on_exit() { system().registry().erase(colour_cache_registry); }
