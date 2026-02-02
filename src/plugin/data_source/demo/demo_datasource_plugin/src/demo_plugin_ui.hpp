// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/io/all.hpp>

#include "demo_plugin_atoms.hpp"

CAF_PUSH_WARNINGS
#include <QFuture>
#include <QList>
#include <QUuid>
#include <QVariantList>
#include <QQmlExtensionPlugin>
CAF_POP_WARNINGS

#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"

namespace xstudio {
namespace demo_plugin {

    /*
    DataModel class.

    We derive from xSTUDIO JSONTreeModel class. This is a QtAbstractItemModel
    that exposes our data in a tree-like structure that can be used to
    instance QML elements efficiently and recursively and is the recommended
    way to display lists, tables and trees of data in the xSTUDIO UI (and
    QML in general).

    JSONTreeModel is also a caf::actor / QObject mixin. This means that it
    can send and receive CAF messages to interact with other components of
    xSTUDIO. The limitation of the Qt 'mixin' approach is that the CAF
    messages are received via the Qt event loop. Thus the message handlers
    are always executed in the main Qt event loop(s) rather than the CAF
    actor system threadpool.

    There are special QML types that are used to build a UI from a
    QtAbstractItemModel. Typically ListView and Repeater and their
    derivatives. Refer to the Qt docs for more information.

    The actual data that backs the JSONTreeModel is a 'JsonTree' object.
    Each node of a JsonTree has json key/value entries and also an ordered list of
    children. Each child is itself a JsonTree (and so-on).

    At each node the json key/value pairs are exposed as 'role' data by the
    QtAbstractItemModel. The comments throughout this demo code provide
    further detail.
    */
    class DataModel : public caf::mixin::actor_object<ui::qml::JSONTreeModel> {

        Q_OBJECT

        QML_NAMED_ELEMENT(DemoPluginDatamodel)

        // QProperties can be directly accessed from the object in the QML code and we must
        // provide getter/setter/notification methods here in C++.
        Q_PROPERTY(
            QVariant usefulData READ usefulData WRITE setUsefulData NOTIFY usefulDataChanged)

        // Property to let us get the actor address for the plugin backend. This will allow us
        // to send messages to the backend from QML code
        Q_PROPERTY(
            QString pluginActorAddress READ pluginActorAddress NOTIFY pluginActorAddressChanged)


      public:
        using super = caf::mixin::actor_object<ui::qml::JSONTreeModel>;
        enum Roles { clientIdRole = super::Roles::LASTROLE, levelRole, nameRole };

        QVariant usefulData() const { return useful_data_; }
        QString pluginActorAddress() const {
            return ui::qml::actorToQString(system(), backend_plugin_);
        }

        void setUsefulData(const QVariant &value) {
            if (value != useful_data_) {
                useful_data_ = value;
                emit usefulDataChanged();
            }
        }

        DataModel(QObject *parent = nullptr);
        ~DataModel() override = default;

        void init(caf::actor_system &system);

        caf::actor_system &system() const {
            return const_cast<caf::actor_companion *>(self())->home_system();
        }

        // When roleData is read in QML, this method is invoked
        [[nodiscard]] QVariant
        data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        // When roleData is set in QML, this method is invoked
        [[nodiscard]] bool setData(
            const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

      signals:

        void usefulDataChanged();
        void pluginActorAddressChanged();

      private:
        QVariant useful_data_;
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
        }
    };
    //![plugin]

} // namespace demo_plugin
} // namespace xstudio