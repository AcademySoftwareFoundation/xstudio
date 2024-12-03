// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/ui/qml/model_data_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/ui/qml/session_model_ui.hpp"
#include "xstudio/ui/qml/QTreeModelToTableModel.hpp"
#include "xstudio/global_store/global_store.hpp"


using namespace xstudio::utility;
using namespace xstudio;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;


UIModelData::UIModelData(
    QObject *parent,
    const std::string &model_name,
    const std::string &data_preference_path,
    const std::vector<std::string> &role_names)
    : super(parent), model_name_(model_name), data_preference_path_(data_preference_path) {

    init(CafSystemObject::get_actor_system());

    if (!role_names.empty()) {
        setRoleNames(role_names);
    }

    try {

        central_models_data_actor_ =
            system().registry().template get<caf::actor>(global_ui_model_data_registry);

        caf::scoped_actor sys{self()->home_system()};

        auto data = request_receive<utility::JsonStore>(
            *sys,
            central_models_data_actor_,
            ui::model_data::register_model_data_atom_v,
            model_name,
            data_preference_path_,
            as_actor());

        setModelData(data);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}


std::mutex UIModelData::mutex_;
QMap<QString, QObject *> UIModelData::context_object_lookup_map_;

UIModelData::UIModelData(QObject *parent) : super(parent) {

    init(CafSystemObject::get_actor_system());

    central_models_data_actor_ =
        system().registry().template get<caf::actor>(global_ui_model_data_registry);
}

void UIModelData::setModelDataName(QString name) {

    std::string m_name = StdFromQString(name);
    if (model_name_ != m_name) {

        if (model_name_ != "") {
            // stops us from watching the old model
            anon_send(
                central_models_data_actor_,
                ui::model_data::register_model_data_atom_v,
                model_name_,
                true,
                as_actor());
        }

        model_name_ = m_name;

        caf::scoped_actor sys{self()->home_system()};

        // we send empty data to 'register' but if the model already exists
        // we'll be sent back what's already in the model
        if (data_preference_path_.empty()) {
            auto data = request_receive<utility::JsonStore>(
                *sys,
                central_models_data_actor_,
                ui::model_data::register_model_data_atom_v,
                model_name_,
                utility::JsonStore(nlohmann::json::parse("{}")),
                as_actor());

            // now we update with the returned model data
            setModelData(data);

        } else {

            auto data = request_receive<utility::JsonStore>(
                *sys,
                central_models_data_actor_,
                ui::model_data::register_model_data_atom_v,
                model_name_,
                data_preference_path_,
                utility::JsonStore(modelData()),
                as_actor());

            // now we update with the returned model data
            setModelData(data);
        }
        emit lengthChanged();

        // process app/user..
        emit modelDataNameChanged();
    }
}

void UIModelData::init(caf::actor_system &system) {
    super::init(system);

    self()->set_default_handler(caf::drop);

    try {
        utility::print_on_create(as_actor(), "UIModelData");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {

            [=](utility::event_atom,
                xstudio::ui::model_data::set_node_data_atom,
                const std::string model_name,
                const std::string path,
                const utility::JsonStore &data) {
                try {
                    const std::string filtered_path = apply_filter(path);
                    if (filtered_path == "")
                        return;
                    QModelIndex idx = getPathIndex(nlohmann::json::json_pointer(filtered_path));
                    JSONTreeModel::setData(idx, QVariantMapFromJson(data), Roles::JSONRole);
                } catch (std::exception &e) {
                    spdlog::debug(
                        "{} {} : {} {} {}",
                        __PRETTY_FUNCTION__,
                        e.what(),
                        path,
                        data.dump(),
                        path);
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::set_node_data_atom,
                const std::string model_name,
                const std::string path,
                const utility::JsonStore &data,
                const std::string role,
                const utility::Uuid &uuid) {
                try {

                    const std::string filtered_path = apply_filter(path);
                    if (filtered_path == "")
                        return;
                    QModelIndex idx = getPathIndex(nlohmann::json::json_pointer(filtered_path));
                    nlohmann::json &j = indexToData(idx);
                    if (!j.contains(role) || j[role] != data) {
                        j[role] = data;
                        for (size_t i = 0; i < role_names_.size(); ++i) {
                            if (role_names_[i] == role) {
                                emit dataChanged(
                                    idx,
                                    idx,
                                    QVector<int>({static_cast<int>(
                                        Roles::LASTROLE + static_cast<int>(i))}));
                                break;
                            }
                        }
                    }

                } catch (std::exception &e) {
                    if (!length()) {
                        // we have no data - Let's say we are exposing the model
                        // called 'foo'. If the backend object that wants to
                        // expose itself in 'foo' hasn't got around to pushing
                        // its data to central_models_data_actor_ against the
                        // 'foo' model ID, but we have created the UI model data
                        // access thing and said we want the 'foo' model data
                        // then we can be in this situation. Do a force fetch
                        // to ensure we are updated now.
                        caf::scoped_actor sys{self()->home_system()};
                        auto data = request_receive<utility::JsonStore>(
                            *sys,
                            central_models_data_actor_,
                            ui::model_data::register_model_data_atom_v,
                            model_name_,
                            utility::JsonStore(nlohmann::json::parse("{}")),
                            as_actor());

                        // process app/user..
                        setModelData(data);
                    } else {
                        spdlog::debug(
                            "{} {} {} {} {} {} {}",
                            __PRETTY_FUNCTION__,
                            e.what(),
                            path,
                            model_name_,
                            model_name,
                            data.dump(),
                            role);
                    }
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::model_data_atom,
                const std::string model_name,
                const utility::JsonStore &data) -> bool {
                if (debug_) {
                    debug_ = false;
                }
                if (model_name == model_name_) {
                    setModelData(data);
                }
                return true;
            }};
    });
}

bool UIModelData::setData(const QModelIndex &index, const QVariant &value, int role) {

    bool result = false;
    try {

        auto path = getIndexPath(index).to_string();

        if (role == Roles::JSONRole) {

            utility::JsonStore j;
            if (std::string(value.typeName()) == "QJSValue") {
                j = nlohmann::json::parse(
                    QJsonDocument::fromVariant(value.value<QJSValue>().toVariant())
                        .toJson(QJsonDocument::Compact)
                        .constData());
            } else if (std::string(value.typeName()) == "QString") {
                std::string value_string = StdFromQString(value.toString());
                j                        = nlohmann::json::parse(value_string);
            } else {
                j = nlohmann::json::parse(QJsonDocument::fromVariant(value)
                                              .toJson(QJsonDocument::Compact)
                                              .constData());
            }

            anon_send(
                central_models_data_actor_,
                xstudio::ui::model_data::set_node_data_atom_v,
                model_name_,
                reverse_apply_filter(path),
                j);


        } else {

            result = JSONTreeModel::setData(index, value, role);

            auto id = role - Roles::LASTROLE;
            if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                auto field        = role_names_.at(id);
                nlohmann::json &j = indexToData(index);
                anon_send(
                    central_models_data_actor_,
                    xstudio::ui::model_data::set_node_data_atom_v,
                    model_name_,
                    reverse_apply_filter(path),
                    utility::JsonStore(j[field]),
                    field);
            }
        }

    } catch (const std::exception &err) {
        auto id = role - Roles::LASTROLE;
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    return result;
}

bool UIModelData::removeRows(int row, int count, const QModelIndex &parent) {

    auto result = false;

    try {

        auto path = getIndexPath(parent).to_string();
        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::remove_rows_atom_v,
            model_name_,
            reverse_apply_filter(path),
            row,
            count);

        result = true;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }
    return result;
}

bool UIModelData::removeRowsSync(int row, int count, const QModelIndex &parent) {

    auto result = false;

    try {


        auto path = getIndexPath(parent).to_string();

        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        a->send(
            central_models_data_actor_,
            xstudio::ui::model_data::remove_rows_atom_v,
            model_name_,
            reverse_apply_filter(path),
            row,
            count,
            false);

        result = JSONTreeModel::removeRows(row, count, parent);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }
    return result;
}


bool UIModelData::moveRows(
    const QModelIndex &sourceParent,
    int sourceRow,
    int count,
    const QModelIndex &destinationParent,
    int destinationChild) {

    auto result = false;

    try {

        result = JSONTreeModel::moveRows(
            sourceParent, sourceRow, count, destinationParent, destinationChild);

        // we send the whole data set to the central models data store as
        // move rows hasn't been properly implemented on that backend
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        a->send(
            central_models_data_actor_,
            xstudio::ui::model_data::reset_model_atom_v,
            model_name_,
            utility::JsonStore(modelData()),
            false);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }
    return false;
}

bool UIModelData::insertRows(int row, int count, const QModelIndex &parent) {

    auto result = false;


    try {

        auto path = getIndexPath(parent).to_string();

        auto bod = R"({})";

        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::insert_rows_atom_v,
            model_name_,
            reverse_apply_filter(path),
            row,
            count,
            utility::JsonStore(nlohmann::json::parse(bod)));

        result = true;

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        result = false;
    }

    return result;
}

bool UIModelData::insertRowsSync(int row, int count, const QModelIndex &parent) {

    bool result = false;
    try {

        auto path = getIndexPath(parent).to_string();

        auto bod = R"({})";

        // update the backend model, but don't broadcast the change back to
        // this instance of UIModelData, because we are going to update our
        // own local copy of the model data
        auto a = caf::actor_cast<caf::event_based_actor *>(self());
        a->send(
            central_models_data_actor_,
            xstudio::ui::model_data::insert_rows_atom_v,
            model_name_,
            reverse_apply_filter(path),
            row,
            count,
            utility::JsonStore(nlohmann::json::parse(bod)),
            false);

        // here we update our own
        result = JSONTreeModel::insertRows(row, count, parent);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

void UIModelData::nodeActivated(
    const QModelIndex &idx, const QString &data, QObject *context_panel) {

    try {

        auto path = getIndexPath(idx).to_string();

        if (context_panel) {
            add_context_object_lookup(context_panel);
        }
        // Tell the backend item that a node in the model has been 'activated'
        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::menu_node_activated_atom_v,
            model_name_,
            reverse_apply_filter(path),
            StdFromQString(Helpers::objPtrTostring(context_panel)));

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

std::string UIModelData::apply_filter(const std::string &path) const {

    // this 'filter' is a way of reducing a model that is some set of
    // rows (with only a depth of 1 to a single row). This filtered_child_idx_
    // gives us the index of the row that we're interested in.
    //
    // This is useful when we have a model that has lots of rows (like a set
    // of attributes) but we're just interested in a single attribute (so we
    // can create a UI widget to control just that attribute) instead of
    // repeating over all of the attributes in the set.
    //
    // look for 'singleAttributeName' in the QML UI code to see where this
    // is used.

    if (filtered_child_idx_ != -1) {
        // when we get here it's because the backend is sending us an update
        // about some row in the model. Our own clone of the model has been
        // reduced to one row, however. First check that the row being modified
        // is the one that we have filtered down to.
        if (fmt::format("/children/{}", filtered_child_idx_) != path) {
            return std::string("");
        }
        // now we make path valid for our own filtered model data, which is
        // simple as we just have one row
        return std::string("/children/0");
    }
    return path;
}

std::string UIModelData::reverse_apply_filter(const std::string &path) const {

    // see notes above.
    if (filtered_child_idx_ != -1) {
        // we use the reverse filter to change the json path to the row in our
        // local (filtered) data to the json path for the whole model data that
        // exists in the backend GlobalUIModelData class
        return fmt::format("/children/{}", filtered_child_idx_);
        ;
    }
    return path;
}

void UIModelData::restoreDefaults() {

    anon_send(
        central_models_data_actor_,
        xstudio::ui::model_data::reset_model_atom_v,
        model_name_,
        data_preference_path_);
}

void UIModelData::add_context_object_lookup(QObject *obj) {
    std::unique_lock l(mutex_);
    context_object_lookup_map_[Helpers::objPtrTostring(obj)] = obj;
}

QObject *UIModelData::context_object_lookup(const QString &obj_str) {

    std::unique_lock l(mutex_);
    auto p = context_object_lookup_map_.find(obj_str);
    if (p != context_object_lookup_map_.end()) {
        return p.value();
    }
    return nullptr;
}


MenusModelData::MenusModelData(QObject *parent) : UIModelData(parent) {
    setRoleNames(std::vector<std::string>{
        "uuid",
        "menu_model",
        "menu_item_type",
        "name",
        "is_checked",
        "choices",
        "choices_ids",
        "current_choice",
        "hotkey_uuid",
        "menu_icon",
        "custom_menu_qml",
        "user_data",
        "hotkey_sequence",
        "menu_item_enabled",
        "menu_item_context",
        "watch_visibility"});
}


ViewsModelData::ViewsModelData(QObject *parent) : UIModelData(parent) {

    setRoleNames(std::vector<std::string>{
        "view_name", "position", "view_qml_source", "singleton_qml_source"});
    setModelDataName("view widgets");

    // make the rows in the model order by the 'button_position' role
    anon_send(
        central_models_data_actor_,
        ui::model_data::set_row_ordering_role_atom_v,
        "view widgets",
        "position");
}

void ViewsModelData::register_view(QString qml_path, QString view_name, const float position) {

    /*auto rc = rowCount(index(-1, -1)); // QModelIndex());
    insertRowsSync(rc, 1, index(-1, -1));
    QModelIndex view_reg_index = index(rc, 0, index(-1, -1));
    std::ignore                = set(view_reg_index, QVariant(view_name), QString("view_name"));
    std::ignore = set(view_reg_index, QVariant(qml_path), QString("view_qml_source"));
    std::ignore = set(view_reg_index, QVariant(position), QString("position"));*/

    utility::JsonStore data;
    data["view_name"]       = StdFromQString(view_name);
    data["view_qml_source"] = StdFromQString(qml_path);
    data["position"]        = position;
    anon_send(
        central_models_data_actor_,
        ui::model_data::insert_rows_atom_v,
        "view widgets", // the model called 'view widgets' is what's used to build the
                        // panels menu
        "",             // (path) add to root
        0,              // row
        1,              // count
        data);
}

QVariant ViewsModelData::view_qml_source(QString view_name) {

    QModelIndex idx = search(QVariant(view_name), "view_name");
    if (idx.isValid()) {
        return get(idx, "view_qml_source");
    }
    return QVariant();
}

PopoutWindowsData::PopoutWindowsData(QObject *parent) : UIModelData(parent) {

    setRoleNames(std::vector<std::string>{
        "view_name",
        "view_qml_source",
        "icon_path",
        "button_position",
        "window_is_visible",
        "user_data",
        "hotkey_uuid"});

    setModelDataName("popout windows");

    // make the rows in the model order by the 'button_position' role
    anon_send(
        central_models_data_actor_,
        ui::model_data::set_row_ordering_role_atom_v,
        "popout windows",
        "button_position");
}

void PopoutWindowsData::register_popout_window(
    QString name,
    QString qml_path,
    QString icon_path,
    float button_position,
    const QUuid hotkey) {

    utility::JsonStore data;
    data["view_name"]         = StdFromQString(name);
    data["icon_path"]         = StdFromQString(icon_path);
    data["view_qml_source"]   = StdFromQString(qml_path);
    data["button_position"]   = button_position;
    data["window_is_visible"] = false;
    if (!hotkey.isNull()) {
        data["hotkey_uuid"] = UuidFromQUuid(hotkey);
    }

    try {

        anon_send(
            central_models_data_actor_,
            ui::model_data::insert_rows_atom_v,
            "popout windows",
            "", // (path) add to root
            0,  // row
            1,  // count
            data);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}


SingletonsModelData::SingletonsModelData(QObject *parent) : UIModelData(parent) {

    setRoleNames(std::vector<std::string>{"source"});
    setModelDataName("singleton items");
}

void SingletonsModelData::register_singleton_qml(const QString &qml_code) {

    int rc = rowCount();
    insertRowsSync(rc, 1);
    QModelIndex idx = index(rc, 0);
    std::ignore     = set(idx, qml_code, "source");
}

PanelsModel::PanelsModel(QObject *parent)
    : UIModelData(
          parent,
          std::string("panels model"),
          std::string("/ui/qml/reskin_windows_and_panels_model"),
          std::vector<std::string>{
              "window_name",
              "width",
              "height",
              "current_layout",
              "position_x",
              "position_y",
              "children",
              "layout_name",
              "enabled",
              "split_horizontal",
              "child_dividers",
              "current_tab",
              "tab_view",
              "tabs_hidden",
              "user_data"}) {}


void PanelsModel::close_panel(QModelIndex panel_index) {

    // Logic for closing a 'panel' is not trivial. Panels are hosted in
    // 'splitters' which chop up the window area into resizable sections
    // Splitters can have splitters as children, meaning you can subdivide
    // the xSTUDIO interface many times with really flexible panel arrangements.
    // When you want to close a panel, the json tree data that backs the
    // arrangement needs to be reconfigured carefully to get the expected
    // behaviour ....

    // how many siblings including the panel we are about to delete?
    const int siblings = rowCount(panel_index.parent());

    if (siblings > 2) {

        // get the divider positions from the parent
        QVariant dividers = get(panel_index.parent(), "child_dividers");
        if (dividers.userType() == QMetaType::QVariantList) {
            QList<QVariant> divs = dividers.toList();
            divs.removeAt(panel_index.row() ? panel_index.row() - 1 : 0);
            std::ignore = set(panel_index.parent(), divs, "child_dividers");
        }
        removeRowsSync(panel_index.row(), 1, panel_index.parent());

    } else if (siblings == 2) {

        const QModelIndex parentIdx = panel_index.parent();
        const int parentRow         = parentIdx.row();
        // delete the panel
        removeRowsSync(panel_index.row(), 1, parentIdx);

        // move the sibling panel (which is now at row 0) up one level to
        // replace the parent panel
        moveRows(parentIdx, 0, 1, parentIdx.parent(), parentIdx.row());

        // now remove the empty parent (whose row has gone up by one due to
        // the moveRows above)
        removeRowsSync(parentRow + 1, 1, parentIdx.parent());

    } else {

        // do nothing if there is only one panel at this index - we can't
        // collapse a panel that isn't part of a split panel
    }
}

int PanelsModel::add_layout(QString layout_name, QModelIndex root, QString tabType) {

    // make a node at the root which is the new layout. Note we always insert
    // at the end so it shows leftmost in the list of layouts in the UI
    int rc = rowCount(root);
    insertRowsSync(rc, 1, root);
    QModelIndex layoutIndex = index(rc, 0, root);

    // now make a node one layer deeper which is the single panel filling the
    // new layout
    insertRowsSync(0, 1, layoutIndex);
    QModelIndex panelIndex = index(0, 0, layoutIndex);

    // now make a node one layer deeper which is the tabs group inside the panel
    insertRowsSync(0, 1, panelIndex);
    QModelIndex tab_index = index(0, 0, panelIndex);

    // insert a default tab
    std::ignore = set(tab_index, tabType, "tab_view");
    std::ignore = set(panelIndex, 0, "current_tab");

    // Set the layout name
    std::ignore = set(layoutIndex, layout_name, "layout_name");
    std::ignore = set(layoutIndex, true, "enabled");

    return rc;
}

void PanelsModel::split_panel(QModelIndex panel_index, bool horizontal_split) {

    QPersistentModelIndex parentNode(panel_index.parent());
    const int insertion_row = panel_index.row();

    QVariant h = get(parentNode, "split_horizontal");
    if (!h.isNull() && h.canConvert(QMetaType::Bool) && h.toBool() == horizontal_split) {

        // parent splitter is of type matching the type of split we want
        // so we need to insert a new panel.

        // we need to reset the divider positions for the parent splitter
        // now it has more children
        int num_dividers = rowCount(parentNode);
        QList<QVariant> divider_positions;
        for (int i = 0; i < num_dividers; ++i) {
            divider_positions.push_back(float(i + 1) / float(num_dividers + 1));
        }
        std::ignore = set(parentNode, divider_positions, "child_dividers");

        // do the insertion of the new panel
        insertRowsSync(insertion_row + 1, 1, parentNode);

        // now make a node one layer deeper which is the tabs group for the
        // new panel
        insertRowsSync(0, 1, index(insertion_row + 1, 0, parentNode));
        QModelIndex new_node_index = index(0, 0, index(insertion_row + 1, 0, parentNode));

        // insert a defaul tab
        std::ignore = set(new_node_index, "Playlists", "tab_view");
        std::ignore = set(new_node_index, 0, "current_tab");


    } else {

        // insert a new node at the same level as the panel being split
        insertRowsSync(insertion_row + 1, 1, parentNode);

        // move the existing panel down one level, so it is a child node of
        // the node position that it started at
        moveRows(parentNode, insertion_row, 1, index(insertion_row + 1, 0, parentNode), 0);

        QModelIndex new_tabs_group_index = index(insertion_row, 0, parentNode);

        std::ignore =
            set(index(insertion_row, 0, parentNode), horizontal_split, "split_horizontal");
        std::ignore =
            set(index(insertion_row, 0, parentNode),
                QVariantList({QVariant(0.51111f)}),
                "child_dividers");

        // now add another child which is our tabs group
        insertRowsSync(1, 1, new_tabs_group_index);

        // and now add another child on level deeper which is our actual tab
        QModelIndex new_sub_tabs_group_index = index(1, 0, new_tabs_group_index);
        insertRowsSync(0, 1, new_sub_tabs_group_index);

        // set the 'view' that our tab contains
        std::ignore = set(index(0, 0, new_sub_tabs_group_index), "Playlists", "tab_view");

        // wet the index of the current tag
        std::ignore = set(new_sub_tabs_group_index, 0, "current_tab");
    }
}

QModelIndex PanelsModel::duplicate_layout(QModelIndex layout_index) {

    nlohmann::json layout_data = indexToFullData(layout_index);
    int rc                     = rowCount(layout_index.parent());
    insertRowsSync(rc, 1, layout_index.parent());
    QModelIndex idx = index(rc, 0, layout_index.parent());
    setData(idx, QVariantMapFromJson(layout_data), Roles::JSONRole);
    return idx;
}

void PanelsModel::storeFloatingWindowData(QString window_name, QVariant data) {

    QModelIndex idx = searchRecursive(window_name, QString("window_name"));
    if (!idx.isValid()) {
        int rc = rowCount();
        insertRowsSync(rc, 1);
        idx = index(rc, 0);
        set(idx, window_name, "window_name");
    }
    set(idx, data, "user_data");
}

QVariant PanelsModel::retrieveFloatingWindowData(QString window_name) {
    QVariant result;
    QModelIndex idx = searchRecursive(window_name, QString("window_name"));
    if (idx.isValid()) {
        result = get(idx, "user_data");
    }
    return result;
}

MediaListColumnsModel::MediaListColumnsModel(QObject *parent)
    : UIModelData(
          parent,
          "media metata exposure model",
          std::string("/ui/qml/media_list_columns_config")) {
    setRoleNames(std::vector<std::string>{
        "title",
        "metadata_path",
        "data_type",
        "size",
        "object",
        "resizable",
        "sortable",
        "position",
        "uuid",
        "info_key",
        "regex_match",
        "regex_format"});
}

MediaListFilterModel::MediaListFilterModel(QObject *parent) : super(parent) {
    setDynamicSortFilter(true);
}

bool MediaListFilterModel::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

    // filter in Media items only
    QVariant t = sourceModel()->data(
        sourceModel()->index(source_row, 0, source_parent), SessionModel::Roles::typeRole);

    if (t.toString() != "Media")
        return false;

    // return false;
    // surely we never run this if sourceModel is nullptr but check just in case.
    if (search_string_.isEmpty())
        return true;

    // here we get the mediaDisplayInfoRole data ... this is QList<QList<QVariant>>
    // The outer list is because we have multiple media list panels with independently
    // configured columns of information - one inner list per media list panel
    // The inner list is the data for each column that is shown in the media list
    // panel against each media item
    QVariant v = sourceModel()->data(
        sourceModel()->index(source_row, 0, source_parent),
        SessionModel::Roles::mediaDisplayInfoRole);


    QList<QVariant> cols = v.toList();
    if (cols.size()) {
        bool match = false;
        // do a string match against any bit of data that can be converted
        // to a string
        for (const auto &col : cols) {
            if (col.toString().contains(search_string_, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        return match;
    }
    return true;
}

QModelIndex MediaListFilterModel::rowToSourceIndex(const int row) const {

    QModelIndex srcIdx          = mapToSource(index(row, 0));
    QTreeModelToTableModel *mdl = dynamic_cast<QTreeModelToTableModel *>(sourceModel());
    if (mdl) {
        srcIdx = mdl->mapToModel(srcIdx);
    }
    return srcIdx;
}

int MediaListFilterModel::sourceIndexToRow(const QModelIndex &idx) const {

    // idx comes from SessionModelUI, which is the sourceModel of OUR sourceModel
    QModelIndex remapped        = idx;
    QTreeModelToTableModel *mdl = dynamic_cast<QTreeModelToTableModel *>(sourceModel());
    if (mdl) {
        // map from SessionModelUI to OUR sourceModel
        remapped = mdl->mapFromModel(remapped);
    }
    // now map from OUR sourceModel to our own index
    remapped = mapFromSource(remapped);
    if (!remapped.isValid())
        return -1;
    return remapped.row();
}

int MediaListFilterModel::getRowWithMatchingRoleData(
    const QVariant &searchValue, const QString &searchRole) const {

    QTreeModelToTableModel *mdl = dynamic_cast<QTreeModelToTableModel *>(sourceModel());
    if (mdl) {
        // map from SessionModelUI to OUR sourceModel
        SessionModel *sessionModel = dynamic_cast<SessionModel *>(mdl->model());
        if (sessionModel) {
            // note - we use this for the auto-scroll in the media list. We have the uuid of the
            // on-screen media, we need to find our row in the model. If we are looking at a
            // playlist, we don't want to match to media in a subset or timeline that also
            // contains the media. The depth=2 in this call means we only search the playlist
            // not its children
            QModelIndex r =
                sessionModel->searchRecursive(searchValue, searchRole, mdl->rootIndex(), 0, 2);
            if (r.isValid()) {
                return sourceIndexToRow(r);
            }
        }
    }
    return -1;
}

MenuModelItem::MenuModelItem(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());
}

MenuModelItem::~MenuModelItem() {

    // TODO: Big optimisations required. Printing debug out here shows that
    // many of these are created and destroyed when xstudio starts up. Something
    // is not working the way we want!
    // std::cerr << "OUT " << to_string(as_actor()) << " " << StdFromQString(menu_name_) <<
    // "\n";
}

void MenuModelItem::init(caf::actor_system &system) {
    super::init(system);

    self()->set_default_handler(caf::drop);

    try {
        utility::print_on_create(as_actor(), "MenuModelItem");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {

            [=](utility::event_atom,
                xstudio::ui::model_data::menu_node_activated_atom,
                const std::string path,
                const utility::JsonStore &menu_item_data,
                const std::string &user_data, // for hotkeys this is the 'context'
                const bool fromHotkey) {
                if (fromHotkey) {
                    emit hotkeyActivated();
                } else {
                    QObject *context =
                        UIModelData::context_object_lookup(QStringFromStd(user_data));
                    emit activated(context);
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::set_node_data_atom,
                const std::string model_name,
                const std::string path,
                const std::string role,
                const utility::JsonStore &data,
                const utility::Uuid &menu_item_uuid) {
                dont_update_model_ = true;
                if (role == "is_checked" && data.is_boolean()) {
                    setIsChecked(data.get<bool>());
                } else if (role == "current_choice" && data.is_string()) {
                    setCurrentChoice(QStringFromStd(data.get<std::string>()));
                }
                dont_update_model_ = false;
            }};
    });
}

void MenuModelItem::setMenuPath(const QString &path) {
    if (path != menu_path_) {
        menu_path_ = path;
        emit menuPathChanged();
        insertIntoMenuModel();
    }
}

void MenuModelItem::setText(const QString &text) {
    if (text != text_) {
        text_ = text;
        emit textChanged();
        insertIntoMenuModel();
    }
}

void MenuModelItem::setMenuName(const QString &menu_name) {
    if (menu_name != menu_name_) {
        menu_name_ = menu_name;
        emit menuNameChanged();
        insertIntoMenuModel();
    }
}

void MenuModelItem::insertIntoMenuModel() {

    if (!dont_update_model_ && !menu_name_.isEmpty() && menu_path_ != "Undefined" &&
        (!text_.isEmpty() || menu_item_type_ == "divider" || menu_item_type_ == "radiogroup")) {

        try {

            auto central_models_data_actor =
                self()->home_system().registry().template get<caf::actor>(
                    global_ui_model_data_registry);

            utility::JsonStore menu_item_data;

            std::string name_string       = StdFromQString(text_);
            menu_item_data["name"]        = name_string;
            menu_item_data["hotkey_uuid"] = UuidFromQUuid(hotkey_uuid_);

            if (!current_choice_.isEmpty())
                menu_item_data["current_choice"] = StdFromQString(current_choice_);
            if (!choices_.empty()) {
                auto choices = nlohmann::json::parse("[]");
                for (const auto &c : choices_) {
                    choices.insert(choices.begin() + choices.size(), 1, StdFromQString(c));
                }
                menu_item_data["choices"] = choices;
            }
            if (menu_item_type_ == "toggle" || menu_item_type_ == "toggle_settings") {
                menu_item_data["is_checked"] = is_checked_;
            }

            // this needs some explanation! Our flexible way of creating menu items
            // causes some headaches. We can put an XsMenuModelItem anywhere in
            // qml code to insert an item into some menu. But what happens when
            // we have multiple instances of the *same* panel (which instances
            // the same menu multiple times?). For example, the qml code for the
            // media list might require us to declare some menu items for the
            // right-click popup menu ... there is only one menu model instance
            // living in GlobalUIModelData that describes this menu. But we have
            // multiple instances of the menu and multiple instances of MenuModelItem
            // that populates the menu. To get around this we can sniff out the
            // panel that the MenuModelItem is created under. We can also sniff
            // out the panel that the XsMenuItem (the actual graphical menu item
            // that we see in the GUI) is created under. We can cross reference
            // these fellas to call back to the correct MenuModelItem when the
            // XsMenuItem is triggered by the user etc.
            //
            // If singleton_ is true, this checking is turned off. We assume
            // that there is a singleton MenuModelItem that wants to insert itself
            // into all instances of the given menu and always get a callback
            // when its item is clicked on in one of those instances.
            //
            // The singleton case is useful when you have, say, a datasource
            // plugin that wants to insert items into the media list menu. For
            // example you might want a feature where it loads the latest version
            // of some media that's already in the media list... in this case
            // you want an identical menu item to appear in the pop-up menu of
            // each media list. When the user clicks on it you want a single
            // callback to be run but you want a handle giving access to the
            // context (i.e. the mediaList) in which the menu event orignated
            const std::string context = StdFromQString(Helpers::objPtrTostring(panel_context_));

            // this string identifies a menu item in the 'tree' of the menu
            // model. If multiple MenuModelItems exist for the same point in
            // the tree (due to multiple panel instances resulting in multiple
            // instances of MenuModelItem) they will share
            std::string full_path = StdFromQString(menu_name_) + StdFromQString(menu_path_) +
                                    StdFromQString(text_) + context;

            if (menu_item_type_ == "divider") {
                // since dividers often don't have a 'text_' string we use their
                // menu item position to make their ID
                full_path += fmt::format("{}", menu_item_position_);
            }
            const auto item_id = utility::Uuid::generate_from_name(full_path.c_str());

            if (!model_entry_id_.is_null() && item_id != model_entry_id_) {
                // NEEDS OPTIMISATION!!!! THIS GETS CALLED LIKE MAD AS MENU_ITEMS BUILD
                std::string menu_name_string = StdFromQString(menu_name_);
                anon_send(
                    central_models_data_actor,
                    ui::model_data::remove_node_atom_v,
                    menu_name_string,
                    model_entry_id_);
            }
            model_entry_id_ = item_id;

            menu_item_data["menu_item_position"] = menu_item_position_;
            menu_item_data["menu_item_type"]     = StdFromQString(menu_item_type_);
            menu_item_data["uuid"]               = model_entry_id_;
            menu_item_data["menu_icon"]          = StdFromQString(menu_custom_icon_);
            menu_item_data["custom_menu_qml"]    = StdFromQString(custom_menu_qml_);
            menu_item_data["menu_item_enabled"]  = enabled_;
            menu_item_data["menu_item_context"]  = context;

            if (!user_data_.isNull()) {
                menu_item_data["user_data"] = qvariant_to_json(user_data_);
            }


            std::string menu_name_string = StdFromQString(menu_name_);
            std::string menu_path_string = StdFromQString(menu_path_);
            anon_send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                menu_name_string,
                menu_path_string,
                menu_item_data,
                as_actor());

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}

void MenuModelItem::setMenuPathPosition(const QString &menu_path, const QVariant position) {
    if (!menu_path.isEmpty() && !menu_name_.isEmpty()) {

        auto central_models_data_actor =
            self()->home_system().registry().template get<caf::actor>(
                global_ui_model_data_registry);

        bool ok;
        const float pos = position.toFloat(&ok);

        if (ok) {
            anon_send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                StdFromQString(menu_name_),
                StdFromQString(menu_path),
                pos);
        } else {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, "Third parameter should be a number.");
        }
    }
}

QObject *MenuModelItem::contextPanel() {
    QQmlContext *c = qmlContext(this);
    QObject *pobj  = this;
    while (c) {
        QObject *cobj = c->contextObject();
        if (cobj && cobj->objectName() == "XsPanelParent") {
            return pobj;
        }
        pobj = cobj;
        c    = c->parentContext();
    }
    return nullptr;
}

PanelMenuModelFilter::PanelMenuModelFilter(QObject *parent) : QSortFilterProxyModel(parent) {

    QObject::connect(this, &QSortFilterProxyModel::sourceModelChanged, [=]() {
        if (auto m = dynamic_cast<UIModelData *>(sourceModel())) {
            menu_item_context_role_id_ = m->roleId(QString("menu_item_context"));
            source_model_              = m;
        }
    });
}

bool PanelMenuModelFilter::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

    if (menu_item_context_role_id_) {

        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        auto menu_item_context =
            sourceModel()->data(index, menu_item_context_role_id_).toString();
        return menu_item_context.isEmpty() || menu_item_context == panel_address_;
    }

    return true;
}