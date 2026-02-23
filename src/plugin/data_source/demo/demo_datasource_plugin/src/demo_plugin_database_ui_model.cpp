// SPDX-License-Identifier: Apache-2.0
#include <caf/actor_registry.hpp>

#include "demo_plugin.hpp"
#include "demo_plugin_database_ui_model.hpp"
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

        // connect to the backend plugin by sending it a handle to ourselves here
        sys->mail(new_database_model_instance_atom_v, as_actor(), true).send(backend_plugin_);

        emit pluginActorAddressChanged();

    } catch (std::exception &e) {
        spdlog::warn("{} failed to get to backend: {}", __PRETTY_FUNCTION__, e.what());
    }

    // We can defined custom message handlers here so that messages can be
    // received from other components (actors) in xSTUDIO. In this case we will
    // be interfacing with the plugin backend
    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](database_model_reset_atom) {
                // we get this message when the backend database is initialised with
                // a full dataset.
                beginResetModel();
                // index_tree_entries_.clear();
                endResetModel();
                emit modelReset();
            },
            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},

            [=](utility::event_atom, utility::notification_atom, const JsonStore &digest) {},

            [=](database_row_count_atom,
                const int num_rows,
                int parent_row,
                size_t parent_index_id) {
                QModelIndex parent_idx = createIndex(parent_row, 0, parent_index_id);
                InternalDataEntry *e =
                    static_cast<InternalDataEntry *>(parent_idx.internalPointer());
                if (e) {
                    int num_rows_to_insert =
                        e->num_rows == -1 ? num_rows : (num_rows - e->num_rows);
                    if (num_rows_to_insert) {
                        beginInsertRows(parent_idx, 0, num_rows_to_insert - 1);
                        insertRows(0, num_rows_to_insert, parent_idx);
                        endInsertRows();
                        e->num_rows = num_rows;
                    }
                } else {
                    insertRows(0, num_rows, parent_idx);
                }
            },

            [=](database_entry_atom,
                const utility::JsonStore &value,
                const DATA_MODEL_ROLE &role,
                int row,
                size_t index_id) {
                QModelIndex idx      = createIndex(row, 0, index_id);
                InternalDataEntry *e = static_cast<InternalDataEntry *>(idx.internalPointer());
                if (e) {
                    const QVariant qdata = xstudio::ui::qml::json_to_qvariant(value);
                    auto p               = e->data.find(role);
                    if (p == e->data.end() || p->second != qdata) {
                        e->data[role] = qdata;
                        emit dataChanged(idx, idx, {static_cast<int>(role)});
                    }
                }
            },

            [=](caf::message &m) {
                spdlog::warn(
                    "{} : unrecognised message received. Message content: {}",
                    __PRETTY_FUNCTION__,
                    to_string(m));
            }};
    });
}

QHash<int, QByteArray> DataModel::roleNames() const {

    QHash<int, QByteArray> roles;
    using int_t = std::underlying_type_t<DATA_MODEL_ROLE>;
    for (const auto &p : DemoPlugin::data_model_role_names) {

        roles[static_cast<int_t>(p.first)] = p.second.c_str();
    }

    return roles;
}

int DataModel::rowCount(const QModelIndex &parent) const {

    const InternalDataEntry *e =
        static_cast<const InternalDataEntry *>(parent.internalPointer());
    if (e && e->num_rows != -1) {
        return e->num_rows;
    }

    // asynchronous request to get the row count from the database. The DB will
    // send us back a message, to be processed by the message handler above
    self()
        ->mail(
            database_row_count_atom_v,
            is_versions_list_,
            e ? e->key : "",
            parent.row(),
            size_t(parent.internalId()))
        .send(backend_plugin_);

    // we don't know the row count yet!
    return 0;

    // Below is how we would do a synchronous darta request, which blocks
    // on the request_receive - slow database will freeze xSTUDIO UI. Not ideal!

    /*auto sys = caf::scoped_actor(system());
    int result = 0;
    try {


        result = utility::request_receive<int>(
            *sys,
            backend_plugin_,
            database_row_count_atom_v,
            e ? e->key : "");
        if (e) e->row_count = result;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;*/
}

int DataModel::columnCount(const QModelIndex &parent) const { return 1; }

bool DataModel::hasChildren(const QModelIndex &parent) const { return rowCount(parent) != 0; }

const DataModel::InternalDataEntry *
DataModel::get_entry(int row, int column, const InternalDataEntry *parent_entry) const {

    const DataModel::InternalDataEntry *result = nullptr;
    auto &internal_data         = const_cast<InternalData &>(index_tree_entries_);
    const std::string entry_key = InternalDataEntry::make_key(row, column, parent_entry);
    auto p                      = internal_data.find(entry_key);
    if (p != internal_data.end()) {
        result = p->second.get();
    } else {
        result = new InternalDataEntry(row, column, entry_key, parent_entry);
        internal_data[entry_key].reset(result);
    }
    return result;
}


QModelIndex DataModel::index(int row, int column, const QModelIndex &parent) const {

    if (row < 0)
        return createIndex(row, column);
    const InternalDataEntry *parent_entry =
        parent.isValid() ? static_cast<const InternalDataEntry *>(parent.internalPointer())
                         : nullptr;
    const InternalDataEntry *entry = get_entry(row, column, parent_entry);
    return createIndex(row, column, entry);
}


QModelIndex DataModel::parent(const QModelIndex &child) const {

    InternalDataEntry *e = static_cast<InternalDataEntry *>(child.internalPointer());
    if (e && e->parent) {
        return createIndex(e->parent->row, e->parent->column, e->parent);
    }
    return QModelIndex();
}

QVariant DataModel::data(const QModelIndex &index, int role) const {

    InternalDataEntry *e = static_cast<InternalDataEntry *>(index.internalPointer());
    if (e && e->data.contains(role))
        return e->data[role];

    // This is how we do an asynchronous data request to the backend plugin -
    // we therefore do not block the UI when the database is slow to respond
    self()
        ->mail(
            database_entry_atom_v,
            is_versions_list_,
            e ? e->key : "",
            static_cast<DATA_MODEL_ROLE>(role),
            index.row(),
            size_t(index.internalId()))
        .send(backend_plugin_);

    // no data ... YET!
    return QVariant(QString());

    // Below is how we would do a synchronous darta request, which blocks
    // on the request_receive - slow database will freeze xSTUDIO UI. Not ideal!

    /*
    auto sys = caf::scoped_actor(system());
    try {

        const auto data = utility::request_receive<utility::JsonStore>(
            *sys,
            backend_plugin_,
            database_entry_atom_v,
            e ? e->key : "",
            static_cast<DATA_MODEL_ROLE>(role));

        result = xstudio::ui::qml::json_to_qvariant(data);
        if (e) e->data[role] = result;

    } catch (const std::exception &err) {
        spdlog::warn("{} {} {} {}", __PRETTY_FUNCTION__, err.what(), role, index.row());
    }*/
}


bool DataModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;
    return result;
}

bool DataModel::canFetchMore(const QModelIndex &parent) const {
    auto result          = false;
    InternalDataEntry *e = static_cast<InternalDataEntry *>(parent.internalPointer());
    if (!e || e->num_rows == -1)
        result = true;
    return result;
}

QFuture<QVariant> DataModel::getRecordsByUuid(const QStringList &media_uuids) {

    // The assumption here is that getting records from our database is slow
    // so we use QFuture to deliver the result asynchronously. Search the QML
    // to see how this is used.
    return QtConcurrent::run([=]() {
        QVariantList r;
        auto sys = caf::scoped_actor(system());
        try {

            // It would be more efficient to make a singe request_receive to
            // the backend plugin here, but this is just a demo!
            for (const auto &media_uuid : media_uuids) {

                const auto data = utility::request_receive<utility::JsonStore>(
                    *sys,
                    backend_plugin_,
                    database_record_from_uuid_atom_v,
                    utility::Uuid(media_uuid.toStdString()));
                r.append(xstudio::ui::qml::json_to_qvariant(data));
            }

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }
        return QVariant(r);
    });
}

void DataModel::setSelection(const QModelIndexList &indexes) {

    std::vector<std::string> keys;
    for (const auto &index : indexes) {

        InternalDataEntry *e = static_cast<InternalDataEntry *>(index.internalPointer());
        if (e)
            keys.push_back(e->key);
        self()->mail(shot_tree_selection_atom_v, keys).send(backend_plugin_);
    }
}

DemoPluginVersionsModel::DemoPluginVersionsModel(QObject *parent) : super(parent) {

    init(CafSystemObject::get_actor_system());
}

void DemoPluginVersionsModel::init(caf::actor_system &_system) {

    super::init(_system);

    // See code comments in DataModel class
    auto pm = _system.registry().template get<caf::actor>(plugin_manager_registry);
    try {

        auto sys            = caf::scoped_actor(_system);
        backend_plugin_ = utility::request_receive<caf::actor>(
            *sys, pm, plugin_manager::get_resident_atom_v, DemoPlugin::PLUGIN_UUID);

        // register ourselves with the backend plugin so it can send us updates to the
        // selected versions
        sys->mail(new_database_model_instance_atom_v, as_actor(), false).send(backend_plugin_);

    } catch (std::exception &e) {
        spdlog::warn("{} failed to get to backend: {}", __PRETTY_FUNCTION__, e.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](database_model_reset_atom, const utility::JsonStore &data) {
                // Selected versions has changed
                beginResetModel();
                data_ = data;
                endResetModel();
            },
            [=](
                data_source::put_data_atom,
                const std::string &version_uuid, 
                const std::string &role_name,
                const utility::JsonStore &role_value) {
                // database data has changed
                int row = 0;
                try {
                    for (auto & p: data_) {
                        if (p.contains("uuid") && version_uuid == p["uuid"].get<std::string>()) {
                            if (p.at(role_name) != role_value) {
                                p[role_name] = role_value;
                                for (const auto &x : DemoPlugin::data_model_role_names) {
                                    if (role_name == x.second) {
                                        auto idx = createIndex(row, 0);
                                        emit dataChanged(idx, idx, QList<int>({static_cast<int>(x.first), Qt::DisplayRole, Qt::EditRole}));
                                    }
                                }
                            }
                        }
                        row++;
                    }
                } catch (std::exception & e) {
                    spdlog::warn("{} failed to get to backend: {}", __PRETTY_FUNCTION__, e.what());
                }
            },
            [=](caf::message &m) {
                spdlog::warn(
                    "{} : unrecognised message received. Message content: {}",
                    __PRETTY_FUNCTION__,
                    to_string(m));
            }};
    });
}

QHash<int, QByteArray> DemoPluginVersionsModel::roleNames() const {

    QHash<int, QByteArray> roles;
    using int_t = std::underlying_type_t<DATA_MODEL_ROLE>;
    for (const auto &p : DemoPlugin::data_model_role_names) {
        roles[static_cast<int_t>(p.first)] = p.second.c_str();
    }
    return roles;
}

int DemoPluginVersionsModel::rowCount(const QModelIndex &parent) const { return data_.size(); }

QVariant DemoPluginVersionsModel::data(const QModelIndex &index, int role) const {

    auto p = DemoPlugin::data_model_role_names.find(static_cast<DATA_MODEL_ROLE>(role));
    if (index.row() < data_.size() && p != DemoPlugin::data_model_role_names.end()) {
        try {
            return xstudio::ui::qml::json_to_qvariant(data_[index.row()][p->second]);
        } catch (...) {
        }
    }
    return QVariant(QString());
}

QVariant DemoPluginVersionsModel::data(const QModelIndex &index, const QString roleName) const {

    const std::string rname = roleName.toStdString();
    for (const auto &p : DemoPlugin::data_model_role_names) {
        if (rname == p.second) {
            return data(index, static_cast<int>(p.first));
        }
    }
    return QVariant(QString());
}

void DemoPluginVersionsModel::set(const QModelIndex &index, QVariant value, QString role) {

    try {
        // Note - the solution here for setting data in the database is deliberately
        // long winded in order to demonstrate how an even from the database can
        // trigger an update in the UI.
        // Here we send a message to the backend plugin to set a key/value in
        // a recrod in the versions table. Using the version uuid to identify it.
        const utility::JsonStore j(xstudio::ui::qml::qvariant_to_json(value));
        self()->mail(
            set_database_value_atom_v,
            utility::Uuid(data(index, "uuid").toString().toStdString()),
            j,
            role.toStdString()).send(backend_plugin_);
    } catch (std::exception &e) {
        spdlog::warn("{}: {}", __PRETTY_FUNCTION__, e.what());
    }

}

