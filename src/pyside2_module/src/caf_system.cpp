// SPDX-License-Identifier: Apache-2.0
#include "xstudio/atoms.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/bookmark/bookmark.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/utility/serialise_headers.hpp"

#include "caf_system.hpp"

using namespace xstudio;

static std::shared_ptr<CafSys> s_instance_ = nullptr;

CafSys *CafSys::instance() {
    if (!s_instance_) {
        s_instance_.reset(new CafSys());
    }
    return s_instance_.get();
}

utility::JsonStore load_preferences() {

    auto prefs = utility::JsonStore();
    if (not global_store::preference_load_defaults(
            prefs, utility::xstudio_root("/preference"))) {
        spdlog::error(
            "Failed to load application preferences {}", utility::xstudio_root("/preference"));
        std::exit(EXIT_FAILURE);
    }
    return prefs;
}

CafSys::CafSys() {

    ACTOR_INIT_GLOBAL_META()

    caf::core::init_global_meta_objects();
    caf::io::middleman::init_global_meta_objects();

    const char *args[] = {"--caf.scheduler.policy=sharing"};
    conf               = new caf_utility::caf_config(1, const_cast<char **>(args));

    the_system_ = new caf::actor_system(*conf);
    utility::start_logger(spdlog::level::info);

    // preferences are required to set the comms port range, which is
    // required to enable xStudio python API
    auto prefs = load_preferences();

    // Create the global actor that manages highest level objects
    global_actor_ = the_system_->spawn<global::GlobalActor>(prefs);

    {
        // This bit is wrong - I'm doing this to kick the global actor
        // to enable the API. I don't know the correct mechanism for this
        caf::scoped_actor self{*(the_system_)};
        self->send(global_actor_, json_store::update_atom_v, prefs);
    }
}

CafSys::~CafSys() {

    {
        caf::scoped_actor self{*(the_system_)};
        self->send_exit(global_actor_, caf::exit_reason::user_shutdown);
    }

    // cancel actors talking to them selves.
    the_system_->clock().cancel_all();
    utility::stop_logger();
    delete the_system_;
    delete conf;
}
