// SPDX-License-Identifier: Apache-2.0

#include "snapshot_model_ui.hpp"

CAF_PUSH_WARNINGS
#include <QFutureWatcher>
#include <QQmlExtensionPlugin>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::ui::qml;
using namespace xstudio::session_snapshots;

SnapshotMenuModel::SnapshotMenuModel(QObject *parent) : JSONTreeModel(parent) {

    // This model provides the same roles as the MenusModelData class which
    // is the QAbstractItemModel that drives xSTUDIO's dynamic menu creation.
    setRoleNames(std::vector<std::string>{
        "uuid",
        "menu_model",
        "menu_item_type",
        "name",
        "is_checked",
        "choices",
        "current_choice",
        "hotkey_uuid",
        "menu_icon",
        "custom_menu_qml",
        "user_data",
        "snapshot_filesystem_path",
        "hotkey_sequence",
        "menu_item_enabled",
        "watch_visibility"});

    try {
        items_.bind_ignore_entry_func(ignore_not_session);
        rescan(QModelIndex(), 1);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

Q_INVOKABLE void SnapshotMenuModel::rescan(const QModelIndex &index, const int depth) {

    // slightly awkward . The items_ contains a list of 'root' items. One root
    // might contain the folder of another root item.
    // For example, I might have /user_data/ as one of my snapshot folders
    // and /user_data/ted/ as another root snapshot folder. They both refer
    // to /user_data/ted.
    //
    // So we have to check for this scenario and make sure all nodes in the
    // FileSystemItem tree are updated if they refer to the same folder
    std::vector<FileSystemItem *> items;
    bool changed = false;

    if (index.isValid()) {
        auto path = UriFromQUrl(get(index, "snapshot_filesystem_path").toUrl());
        for (auto &item : items_) {
            auto i = item.find_by_path(path);
            if (i)
                items.push_back(i);
        }

    } else {
        items.push_back(&items_);
        changed = true;
    }

    for (auto &item : items) {
        changed |= item->scan(depth);
    }

    if (changed && !items.empty()) {
        auto jsn = toMenuModelItemData(items[0]);
        if (index.isValid()) {
            setData(index, QVariantMapFromJson(jsn), JSONRole);
        } else {
            setModelData(jsn);
        }
    }
}

utility::JsonStore SnapshotMenuModel::toMenuModelItemData(FileSystemItem *item) {

    // here we recursively build a JsonStore the represents a tree of menus.
    // This is converted to model data in qml by the base class and drives
    // the XsMenu and XsMenuItem qml items to dynamicall build nested menus
    // that show the filesystem folders that have been added to the snapshot
    // paths

    utility::JsonStore menu_item_data;
    const std::string snapshot_filesystem_path(to_string(item->path()));

    if (item->type() == utility::FSIT_FILE) {

        menu_item_data["name"]                     = item->name();
        menu_item_data["menu_item_type"]           = "button";
        menu_item_data["menu_item_enabled"]        = true;
        menu_item_data["snapshot_filesystem_path"] = snapshot_filesystem_path;
        menu_item_data["watch_visibility"]         = false;
        menu_item_data["user_data"]                = "SNAPSHOT_LOAD";


    } else if (item->type() != utility::FSIT_NONE) {

        // Not a file but a folder ... so we need to build a submenu to
        // show the content of this subfolder

        menu_item_data["name"]                     = item->name();
        menu_item_data["menu_item_type"]           = "menu";
        menu_item_data["children"]                 = nlohmann::json::array();
        menu_item_data["snapshot_filesystem_path"] = snapshot_filesystem_path;

        // setting this means the menu system will run a callback when the
        // corresponding sub-menu for this folder becomes visible. We use
        // this to trigger a scan to fill in the sub-menu contents
        menu_item_data["watch_visibility"] = true;

        // make separate lists of files and subfolders
        std::vector<FileSystemItem *> subfolders;
        std::vector<FileSystemItem *> files;
        for (auto p = item->begin(); p != item->end(); ++p) {
            if (p->type() == utility::FSIT_FILE) {
                files.push_back(&(*p));
            } else {
                subfolders.push_back(&(*p));
            }
        }

        // sort  alaphabetically
        std::sort(
            subfolders.begin(), subfolders.end(), [](const auto &a, const auto &b) -> bool {
                return utility::to_lower(a->name()) < utility::to_lower(b->name());
            });
        std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) -> bool {
            return utility::to_lower(a->name()) < utility::to_lower(b->name());
        });

        // now add the subfolders as children of this menu
        if (subfolders.size()) {
            for (auto &subfolder : subfolders) {
                menu_item_data["children"].push_back(toMenuModelItemData(subfolder));
            }
            utility::JsonStore divider;
            divider["name"]              = "";
            divider["menu_item_type"]    = "divider";
            divider["menu_item_enabled"] = true;
            menu_item_data["children"].push_back(divider);
        }

        // now add the files as children of this menu
        if (files.size()) {
            for (auto &file : files) {
                menu_item_data["children"].push_back(toMenuModelItemData(file));
            }
            utility::JsonStore divider;
            divider["name"]              = "";
            divider["menu_item_type"]    = "divider";
            divider["menu_item_enabled"] = true;
            menu_item_data["children"].push_back(divider);
        }

        // append the common menu items for the folder
        if (item != &items_) {
            utility::JsonStore save_snapshot_item;
            save_snapshot_item["name"]                     = "Save Snapshot ...";
            save_snapshot_item["menu_item_type"]           = "button";
            save_snapshot_item["menu_item_enabled"]        = true;
            save_snapshot_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            save_snapshot_item["watch_visibility"]         = false;
            save_snapshot_item["user_data"]                = "SNAPSHOT_SAVE";
            menu_item_data["children"].push_back(save_snapshot_item);

            utility::JsonStore save_selected_snapshot_item;
            save_selected_snapshot_item["name"]              = "Save Selected As Snapshot ...";
            save_selected_snapshot_item["menu_item_type"]    = "button";
            save_selected_snapshot_item["menu_item_enabled"] = true;
            save_selected_snapshot_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            save_selected_snapshot_item["watch_visibility"]         = false;
            save_selected_snapshot_item["user_data"]                = "SNAPSHOT_SELECTED_SAVE";
            menu_item_data["children"].push_back(save_selected_snapshot_item);


            utility::JsonStore options_menu_item;
            options_menu_item["name"]             = "Options";
            options_menu_item["menu_item_type"]   = "menu";
            options_menu_item["children"]         = nlohmann::json::array();
            options_menu_item["watch_visibility"] = false;

            utility::JsonStore create_folder_item;
            create_folder_item["name"]                     = "Create New Folder ...";
            create_folder_item["menu_item_type"]           = "button";
            create_folder_item["menu_item_enabled"]        = true;
            create_folder_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            create_folder_item["watch_visibility"]         = false;
            create_folder_item["user_data"]                = "SNAPSHOT_NEW_FOLDER";
            options_menu_item["children"].push_back(create_folder_item);

            utility::JsonStore reveal_folder_item;
            reveal_folder_item["name"]                     = "Reveal On Disk ...";
            reveal_folder_item["menu_item_type"]           = "button";
            reveal_folder_item["menu_item_enabled"]        = true;
            reveal_folder_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            reveal_folder_item["watch_visibility"]         = false;
            reveal_folder_item["user_data"]                = "SNAPSHOT_REVEAL";
            options_menu_item["children"].push_back(reveal_folder_item);

            menu_item_data["children"].push_back(options_menu_item);
        }

        // the first snapshot menus have a 'remove' option. We know we are
        // building the first level menu if our 'item' is a child of items_.
        bool top_level = false;
        for (auto &i : items_) {
            if (item == &i) {
                top_level = true;
                break;
            }
        }

        if (top_level) {
            utility::JsonStore divider;
            divider["name"]              = "";
            divider["menu_item_type"]    = "divider";
            divider["menu_item_enabled"] = true;
            menu_item_data["children"].push_back(divider);

            utility::JsonStore remove_menu_item;
            remove_menu_item["name"]                     = "Remove " + item->name() + " Menu";
            remove_menu_item["menu_item_type"]           = "button";
            remove_menu_item["menu_item_enabled"]        = true;
            remove_menu_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            remove_menu_item["watch_visibility"]         = false;
            remove_menu_item["user_data"]                = "SNAPSHOT_REMOVE_SNAPSHOT";
            menu_item_data["children"].push_back(remove_menu_item);
        }

        if (item == &items_) {
            utility::JsonStore add_snapshot_menu_item;
            add_snapshot_menu_item["name"]                     = "Add Snapshot Folder ...";
            add_snapshot_menu_item["menu_item_type"]           = "button";
            add_snapshot_menu_item["menu_item_enabled"]        = true;
            add_snapshot_menu_item["snapshot_filesystem_path"] = snapshot_filesystem_path;
            add_snapshot_menu_item["watch_visibility"]         = false;
            add_snapshot_menu_item["user_data"]                = "SNAPSHOT_ADD";
            menu_item_data["children"].push_back(add_snapshot_menu_item);
        }
    }

    return menu_item_data;
}

void SnapshotMenuModel::setPaths(const QVariant &value) {
    try {

        if (not value.isNull()) {
            paths_ = value;
            emit pathsChanged();

            auto jsn = mapFromValue(paths_);
            if (jsn.is_array()) {
                items_.clear();

                for (const auto &i : jsn) {
                    if (i.count("name") and i.count("path")) {
                        auto uri = caf::make_uri(i.at("path"));
                        if (uri)
                            items_.insert(items_.end(), FileSystemItem(i.at("name"), *uri));
                    }
                }
            }
            rescan(QModelIndex(), 1);
        }

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

bool SnapshotMenuModel::createFolder(const QModelIndex &index, const QString &name) {
    auto result = false;

    // get path..
    if (index.isValid()) {
        auto uri =
            caf::make_uri(StdFromQString(get(index, "snapshot_filesystem_path").toString()));

        if (uri) {
            auto path     = uri_to_posix_path(*uri);
            auto new_path = fs::path(path) / StdFromQString(name);
            try {
                fs::create_directory(new_path);
                rescan(index.parent().parent());
            } catch (const std::exception &err) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        } else {
            spdlog::warn(
                "{} {} {}",
                __PRETTY_FUNCTION__,
                "failed to create uri",
                StdFromQString(get(index, "snapshot_filesystem_path").toString()));
        }
    } else {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, "Invalid index");
    }

    return result;
}

QUrl SnapshotMenuModel::buildSavePath(const QModelIndex &index, const QString &name) const {
    // get path..
    auto result = QUrl();
    if (index.isValid()) {
        auto uri =
            caf::make_uri(StdFromQString(get(index, "snapshot_filesystem_path").toString()));
        if (uri) {
            auto path     = uri_to_posix_path(*uri);
            auto new_path = fs::path(path) / std::string(StdFromQString(name) + ".xsz");
            result        = QUrlFromUri(posix_path_to_uri(new_path.string()));
        }
    }

    return result;
}

SessionSnapshotsPlugin::SessionSnapshotsPlugin(
    caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "SessionSnapshotsPlugin", init_settings) {
    // this call is essential to set-up the base class
    make_behavior();

    // new method for instantiating a 'singleton' qml item which can
    // do a one-time insertion of menu items into any menu model

    // In this case we add a custom menu item to the 'main menu bar' menu model.
    // We provide the qml code to instantiate the menu widget when the user
    // clicks on the top level menu item.
    register_singleton_qml(
        R"(
            import QtQuick 2.15
            import xstudio.qml.models 1.0

            XsMenuModelItem {
                id: menu_item
                text: "Snapshots"
                menuPath: ""
                menuItemType: "custom"
                menuItemPosition: 4.0
                menuModelName: "main menu bar"
                customMenuQml: "import SessionSnapshots 1.0; SessionSnapshotsMenu {}"
            }
        )");
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<SessionSnapshotsPlugin>>(
                SessionSnapshotsPlugin::PLUGIN_UUID,
                "SessionSnapshots",
                plugin_manager::PluginFlags::PF_UTILITY,
                true, // resident
                "Al Crate/Ted Waine",
                "Session Snapshots Plugin - Menu driven system for saving and loading sessions "
                "from specific filesystem locations.")}));
}
}

//![plugin]
class SnapshotMenuModelQml : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

  public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<SnapshotMenuModel>(uri, 1, 0, "SessionSnapshotsMenuModel");
    }
};
//![plugin]
#include "snapshot_model_ui.moc"
