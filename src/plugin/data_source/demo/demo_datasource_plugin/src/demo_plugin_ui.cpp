// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "demo_plugin.hpp"
#include "demo_plugin_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"

CAF_PUSH_WARNINGS
#include <QQmlExtensionPlugin>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::demo_plugin;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;

DataModel::DataModel(QObject *parent) : super(parent) {

    init(CafSystemObject::get_actor_system());

    // a vector of { "job", "shot", "version_type ", ... etc }. These strings set the names of
    // the 'roleData' values that can be read or written to in the QML in our data model.
    // Note that they map exactly to the keys in our source json data that fills out the
    // model
    std::vector<std::string> role_names =
        utility::map_value_to_vec(DemoPlugin::data_model_role_names);
    setRoleNames(role_names);
}

void DataModel::init(caf::actor_system &_system) {

    super::init(_system);

    // Our demo plugin backend was instanced automatically by xSTUDIO on startup
    // because its 'resident' flag is true. The instance is owned by the
    // plugin manager component of xSTUDIO. Core components of xSTUDIO are
    // added to the CAF system 'registry' so we can get to them as follows:
    auto pm = _system.registry().template get<caf::actor>(plugin_manager_registry);
    try {

        // request_receive allows us to do a synchronised (blocking) message send
        // to some other actor in xSTUDIO's system and receive back a result
        // from the message handler of the target actor (in this case plugin
        // manager).

        // Note: when sending messages, the atom types must be appended with '_v'
        // to make an *instance* of the atom type. The template type must match
        // the return type for the corresponding message handler in the actor
        // we are requesting from (in this case PluginManagerActor)
        auto sys        = caf::scoped_actor(_system);
        backend_plugin_ = utility::request_receive<caf::actor>(
            *sys, pm, plugin_manager::get_resident_atom_v, DemoPlugin::PLUGIN_UUID);
        emit pluginActorAddressChanged();

        // we can now send a message to join the backend actor's event group so
        // that we receive messages that it broadcasts relating to updates to
        // the data. We're going to use our custom atom to do it. See the
        // message handler in demo_plugin.cpp to see the signature for receiving
        // this message. Note that as_actor() casts 'this' to caf::actor.
        const auto model_data = utility::request_receive<utility::JsonStore>(
            *sys, backend_plugin_, demo_plugin_custom_atom_v, as_actor());

        // uncomment to spam the entire data set to your terminal
        // std::cerr << model_data.dump(2) << "\n";

        // here we initialise our data tree
        setModelData(model_data);

    } catch (std::exception &e) {
        spdlog::warn("{} failed to get to backend: {}", __PRETTY_FUNCTION__, e.what());
    }

    // We can defined custom message handlers here so that messages can be
    // received from other components (actors) in xSTUDIO. In this case we will
    // be interfacing with the plugin backend
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](demo_plugin_custom_atom, const JsonStore &entire_data_set) {
                // this message
                setModelData(entire_data_set);
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](utility::event_atom, utility::notification_atom, const JsonStore &digest) {},

            [=](caf::message &m) {
                spdlog::warn(
                    "{} : unrecognised message received. Message content: {}",
                    __PRETTY_FUNCTION__,
                    to_string(m));
            }};
    });
}


QVariant DataModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {

        const auto &j = indexToData(index);
        result        = JSONTreeModel::data(index, role);

    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }

    return result;
}


bool DataModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        nlohmann::json &j = indexToData(index);
        result            = JSONTreeModel::setData(index, value, role);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result)
        emit dataChanged(index, index, roles);

    return result;
}
