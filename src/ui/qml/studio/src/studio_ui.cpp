// SPDX-License-Identifier: Apache-2.0

#include <iostream>

#include "xstudio/atoms.hpp"
#include "xstudio/session/session_actor.hpp"
#include "xstudio/ui/qml/playlist_ui.hpp"
#include "xstudio/ui/qml/studio_ui.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/logging.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

StudioUI::StudioUI(caf::actor_system &system, QObject *parent) : QMLActor(parent) {
    init(system);
}

void StudioUI::init(actor_system &system_) {
    QMLActor::init(system_);

    spdlog::debug("StudioUI init");

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {[=](utility::event_atom, utility::change_atom) {}};
    });
}

bool StudioUI::clearImageCache() {
    // get global cache.
    auto ic     = system().registry().template get<caf::actor>(image_cache_registry);
    auto result = false;
    try {
        scoped_actor sys{system()};
        result = request_receive<bool>(*sys, ic, clear_atom_v);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}
