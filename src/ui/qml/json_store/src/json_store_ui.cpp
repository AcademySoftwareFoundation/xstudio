// SPDX-License-Identifier: Apache-2.0

// CAF_PUSH_WARNINGS
// #include <QString>
// CAF_POP_WARNINGS

#include "xstudio/ui/qml/json_store_ui.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace caf;
using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;

JsonStoreUI::JsonStoreUI(QObject *parent) : QMLActor(parent) {}

void JsonStoreUI::init(actor_system &system_) {
    QMLActor::init(system_);

    set_message_handler([=](actor_companion * /*self_*/) -> message_handler {
        return {
            [=](json_store::update_atom,
                const utility::JsonStore & /*change*/,
                const std::string & /*path*/,
                const utility::JsonStore &full) {
                set_json_string(QStringFromStd(full.dump(4)), true);
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](json_store::update_atom, const utility::JsonStore &json) {
                set_json_string(QStringFromStd(json.dump(4)), true);
            }};
    });
}


QString JsonStoreUI::json_string() { return json_string_; }

void JsonStoreUI::set_json_string(const QString &str, const bool from_actor) {
    if (str == json_string_)
        return;

    json_string_ = str;
    if (not from_actor)
        send_update(json_string_);

    emit json_string_changed();
}

void JsonStoreUI::subscribe(caf::actor actor) {
    monitor_.dispose();
    unsubscribe();

    monitor_ = self()->monitor(
        actor, [this, addr = actor.address()](const error &) { unsubscribe(); });

    store = actor;
    // get group, and watch..
    scoped_actor sys{system()};
    sys->mail(utility::get_group_atom_v)
        .request(actor, infinite)
        .receive(
            [&](const std::tuple<caf::actor, caf::actor, utility::JsonStore> &data) {
                const auto [js, grp, json] = data;
                store_events               = grp;

                scoped_actor sys{system()};
                request_receive<bool>(
                    *sys, store_events, broadcast::join_broadcast_atom_v, as_actor());

                set_json_string(QStringFromStd(json.dump(4)), true);
            },
            [&](const caf::error &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(e));
            });
}

void JsonStoreUI::unsubscribe() {
    store = caf::actor();

    scoped_actor sys{system()};

    try {
        utility::request_receive<bool>(
            *sys, store_events, broadcast::leave_broadcast_atom_v, as_actor());
    } catch (...) {
    }

    store_events = caf::actor();
    set_json_string(QString(), true);
}

void JsonStoreUI::send_update(QString text) {
    if (store != caf::actor()) {
        try {
            utility::JsonStore json_data(nlohmann::json::parse(text.toUtf8().constData()));
            anon_mail(json_store::set_json_atom_v, json_data).send(store);
        } catch (...) {
        }
    }
}