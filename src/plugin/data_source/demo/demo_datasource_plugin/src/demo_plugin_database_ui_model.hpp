// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "demo_plugin.hpp"

CAF_PUSH_WARNINGS
#include <QFuture>
#include <QList>
#include <QUuid>
#include <QVariantList>
#include <QQmlExtensionPlugin>
#include <QAbstractItemModel>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio {
namespace demo_plugin {

    /*
    DataModel class.

    We derive from QAbstractItemModelto expose our simple database data in
    a tree-like structure that can be used to instance QML elements
    efficiently and recursively and is the recommended way to display lists,
    tables and trees of data in the xSTUDIO UI (and QML in general).

    This class is also a caf::actor / QObject mixin. This means that it
    can send and receive CAF messages to interact with other components of
    xSTUDIO. The limitation of the Qt 'mixin' approach is that the CAF
    messages are received via the Qt event loop. Thus the message handlers
    are always executed in the main Qt UI event loop(s) rather than the CAF
    actor system threadpool. We need to keep that in mind when processing

    There are special QML types that are used to build a UI from a
    QtAbstractItemModel. Typically ListView and Repeater and their
    derivatives. Refer to the Qt docs for more information as well as the
    demo QML code that is part of this project.

    */
    class DataModel : public caf::mixin::actor_object<QAbstractItemModel> {

        Q_OBJECT

        QML_NAMED_ELEMENT(DemoPluginDatamodel)

        // Property to let us get the actor address for the plugin backend. This will allow us
        // to send messages to the backend from QML code
        Q_PROPERTY(
            QString pluginActorAddress READ pluginActorAddress NOTIFY pluginActorAddressChanged)

        // This property lets us
        Q_PROPERTY(
            bool isVersionsList READ isVersionsList WRITE setIsVersionsList NOTIFY
                isVersionsListChanged)

      public:
        using super = caf::mixin::actor_object<QAbstractItemModel>;

        bool isVersionsList() const { return is_versions_list_; }
        QString pluginActorAddress() const {
            return ui::qml::actorToQString(system(), backend_plugin_);
        }
        void setIsVersionsList(const bool v) {
            if (v != is_versions_list_) {
                is_versions_list_ = v;
                emit isVersionsListChanged();
            }
        }

        DataModel(QObject *parent = nullptr);
        ~DataModel() override = default;

        void init(caf::actor_system &system);

        caf::actor_system &system() const {
            return const_cast<caf::actor_companion *>(self())->home_system();
        }

        /* These pure virtual methods from QAbstractItemModel need to be overriden*/
        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] bool
        hasChildren(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QModelIndex
        index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QModelIndex parent(const QModelIndex &child) const override;
        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
        [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

        // When roleData is set in QML, this method is invoked
        [[nodiscard]] bool setData(
            const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

        // a-synchronous request to fetch all data for a list of nodes in
        // the tree
        Q_INVOKABLE [[nodiscard]] QFuture<QVariant>
        getRecordsByUuid(const QStringList &media_uuids);

        Q_INVOKABLE void setSelection(const QModelIndexList &index);

      signals:

        void pluginActorAddressChanged();
        void modelReset();
        void isVersionsListChanged();

      private:
        // QAbstractItemModel requires us to back the model with our own
        // internal data. In this example we create an InternalDataEntry for every
        // node in the tree but only 'on demand'. We are able to record the
        // heirarchy of nodes in the tree through the simple use of storing
        // the node 'row' (we don't actually use columns) and also storing
        // a pointer to the parent node. We can do this because Qt will
        // only every request information about the model from the bottom
        // up so for any given node we have always already been asked to
        // make an index for the parent index. See the index() method in
        // this class.
        struct InternalDataEntry {

            InternalDataEntry(
                const int r, const int c, const std::string k, const InternalDataEntry *p)
                : row(r), column(c), key(k), parent(p) {}

            const int row;
            const int column;

            // key is a useable nlohmann::json_pointer the we can use to
            // lookup into the database itself tp get to the correspondin
            // node in the job/sequence/shot 'database'. The data entry
            // for the 3rd row of the 2nd row in the 1st row of the root
            // would have a key of "/rows/1/rows/2/rows/3"
            const std::string key;

            const InternalDataEntry *parent;
            int num_rows = -1;
            // The actual data (model values) at this node in the tree
            std::map<int, QVariant> data;

            static std::string make_key(int row, int column, const InternalDataEntry *parent) {
                if (row == -1)
                    return std::string();
                if (parent)
                    return parent->key + "/rows/" + std::to_string(row);
                return "/rows/" + std::to_string(row);
            }
        };

        const InternalDataEntry *
        get_entry(int row, int column, const InternalDataEntry *parent_entry) const;

        typedef std::map<std::string, std::shared_ptr<const InternalDataEntry>> InternalData;
        InternalData index_tree_entries_;

        caf::actor backend_plugin_;
        bool is_versions_list_ = {false};
    };

    /*
    DemoPluginVersionsModel class.

    We derive from QAbstractListModel for a much simpler dynamic data model. This simply
    exposes a flat list of database records that are shown on the right side of the
    interface. The way this works is we 'subscribe' to events broadcast be the backend
    plugin. When the user selects one or more shots or sequences in the shot tree, the
    backend plugin is informed of this by the DataModel class instance. The backend
    plugin then requests the corresponding records (for the selected sequences and shots)
    from the database and broadcasts the result as an event. Our DemoPluginVersionsModel
    intance(s) receive those events and update their model data to make the database
    records avaiable as a flat list.

    */
    class DemoPluginVersionsModel : public caf::mixin::actor_object<QAbstractListModel> {

        Q_OBJECT

        QML_NAMED_ELEMENT(DemoPluginVersionsModel)

      public:
        using super = caf::mixin::actor_object<QAbstractListModel>;

        DemoPluginVersionsModel(QObject *parent = nullptr);
        ~DemoPluginVersionsModel() override = default;

        void init(caf::actor_system &system);

        caf::actor_system &system() const {
            return const_cast<caf::actor_companion *>(self())->home_system();
        }

        [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        Q_INVOKABLE QVariant data(const QModelIndex &index, const QString roleName) const;

        Q_INVOKABLE void set(const QModelIndex &index, QVariant value, QString role);

      private:
        utility::JsonStore data_;
        caf::actor backend_plugin_;
    };


    // We require this boiler-plate to register our custom class as a QML
    // item:

    //![plugin]
    class DemoPluginQML : public QQmlExtensionPlugin {
        Q_OBJECT

        Q_PLUGIN_METADATA(IID "xstudio-project.demoplugin.ui")
        void registerTypes(const char *uri) override {
            qmlRegisterType<xstudio::demo_plugin::DataModel>(
                "demoplugin.qml", 1, 0, "DemoPluginDatamodel");
            qmlRegisterType<xstudio::demo_plugin::DemoPluginVersionsModel>(
                "demoplugin.qml", 1, 0, "DemoPluginVersionsModel");
        }
    };
    //![plugin]

} // namespace demo_plugin
} // namespace xstudio