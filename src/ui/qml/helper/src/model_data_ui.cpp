// SPDX-License-Identifier: Apache-2.0
#include <set>
#include <nlohmann/json.hpp>

#include "xstudio/ui/qml/model_data_ui.hpp"
#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/ui/qml/helper_ui.hpp"
#include "xstudio/global_store/global_store.hpp"


using namespace xstudio::utility;
using namespace xstudio;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;


UIModelData::UIModelData(
    QObject *parent, const std::string &model_name, const std::string &data_preference_path)
    : super(parent), model_name_(model_name), data_preference_path_(data_preference_path) {

    init(CafSystemObject::get_actor_system());

    try {

        central_models_data_actor_ =
            system().registry().template get<caf::actor>(global_ui_model_data_registry);

        caf::scoped_actor sys{self()->home_system()};

        auto data = request_receive<utility::JsonStore>(
            *sys,
            central_models_data_actor_,
            ui::model_data::register_model_data_atom_v,
            model_name,
            data_preference_path,
            as_actor());

        setModelData(data);

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
}

UIModelData::UIModelData(QObject *parent) : super(parent) {

    init(CafSystemObject::get_actor_system());

    central_models_data_actor_ =
        system().registry().template get<caf::actor>(global_ui_model_data_registry);
}

void UIModelData::setModelDataName(QString name) {

    std::string m_name = StdFromQString(name);
    if (model_name_ != m_name) {

        model_name_ = m_name;

        anon_send(
            central_models_data_actor_,
            ui::model_data::register_model_data_atom_v,
            model_name_,
            utility::JsonStore(nlohmann::json::parse("{}")),
            as_actor());

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

                    QModelIndex idx   = getPathIndex(nlohmann::json::json_pointer(path));
                    nlohmann::json &j = indexToData(idx);
                    j                 = data;
                    emit dataChanged(idx, idx, QVector<int>());
                } catch (std::exception &e) {
                    spdlog::warn(
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

                    QModelIndex idx   = getPathIndex(nlohmann::json::json_pointer(path));
                    nlohmann::json &j = indexToData(idx);
                    if (!j.contains(role) || j[role] != data) {
                        j[role] = data;
                        for (size_t i = 0; i < role_names_.size(); ++i) {
                            if (role_names_[i] == role) {

                                emit dataChanged(
                                    idx,
                                    idx,
                                    QVector<int>({static_cast<int>(Roles::LASTROLE + static_cast<int>(i))}));

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
                        // suppressing this warning, because you can get it when it's not
                        // a problem if this node gets a set_node_data message before the
                        // given node has been added. The backend model is all fine, but
                        // we can get a bit out of sync here and it's no big deal.
                        spdlog::debug("{} {} {}", __PRETTY_FUNCTION__, e.what(), path);
                    }
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::model_data_atom,
                const std::string model_name,
                const utility::JsonStore &data) { setModelData(data); }};
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
                j = nlohmann::json::parse(value_string);
            } else {
                j = nlohmann::json::parse(QJsonDocument::fromVariant(value)
                                              .toJson(QJsonDocument::Compact)
                                              .constData());
            }

            anon_send(
                central_models_data_actor_,
                xstudio::ui::model_data::set_node_data_atom_v,
                model_name_,
                path,
                j);


        } else {

            result = JSONTreeModel::setData(index, value, role);

            auto id = role - Roles::LASTROLE;
            if (id >= 0 and id < static_cast<int>(role_names_.size())) {
                auto field = role_names_.at(id);
                anon_send(
                    central_models_data_actor_,
                    xstudio::ui::model_data::set_node_data_atom_v,
                    model_name_,
                    path,
                    utility::JsonStore(mapFromValue(value)),
                    field);
            }
        }

    } catch (const std::exception &err) {
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
            path,
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

        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::remove_rows_atom_v,
            model_name_,
            path,
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

    // TODO!!

    /*if (destinationChild < 0 or count < 1 or sourceRow < 0)
        return false;

    try {
        nlohmann::json *src_children = nullptr;

        // get src array
        if (sourceParent.isValid()) {
            nlohmann::json &j = indexToData(sourceParent);
            if (j.contains(children_))
                src_children = &(j.at(children_));
        } else if (not sourceParent.isValid()) {
            src_children = &data_;
        }

        // check valid move..
        auto moveLast  = sourceRow + count - 1;

        if (moveLast >= static_cast<int>(src_children->size()))
            return false;

        auto sourceParentPath = getIndexPath(sourceParent.internalId()).to_string();
        auto destinationParentPath = getIndexPath(destinationParent.internalId()).to_string();

        anon_send(
                central_models_data_actor_,
                xstudio::ui::model_data::model_data_atom_v,
                utility::Uuid("8f399658-d2ba-4e62-8887-64fc4c6b7a17"),
                sourceParentPath,
                sourceRow,
                count,
                destinationParentPath,
                destinationChild);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        return false;
    }*/

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
            path,
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
        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::insert_rows_atom_v,
            model_name_,
            path,
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

void UIModelData::nodeActivated(const QModelIndex &idx) {

    try {

        auto path = getIndexPath(idx).to_string();

        // Tell the backend item that a node in the model has been 'activated'
        anon_send(
            central_models_data_actor_,
            xstudio::ui::model_data::menu_node_activated_atom_v,
            model_name_,
            path);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

MenusModelData::MenusModelData(QObject *parent) : UIModelData(parent) {
    setRoleNames(std::vector<std::string>{
        "uuid",
        "menu_model",
        "menu_item_type",
        "name",
        "is_checked",
        "choices",
        "current_choice",
        "hotkey"});
}


ViewsModelData::ViewsModelData(QObject *parent) : UIModelData(parent) {

    setRoleNames(std::vector<std::string>{"view_name", "view_qml_source"});
    setModelDataName("view widgets");
}

void ViewsModelData::register_view(QString qml_path, QString view_name) {

    auto rc = rowCount(index(-1, -1)); // QModelIndex());
    insertRowsSync(rc, 1, index(-1, -1));
    QModelIndex view_reg_index = index(rc, 0, index(-1, -1));
    std::ignore                = set(view_reg_index, QVariant(view_name), QString("view_name"));
    std::ignore = set(view_reg_index, QVariant(qml_path), QString("view_qml_source"));
}

QVariant ViewsModelData::view_qml_source(QString view_name) {

    QModelIndex idx = search(QVariant(view_name), "view_name");
    if (idx.isValid()) {
        return get(idx, "view_qml_source");
    }
    return QVariant();
}

ReskinPanelsModel::ReskinPanelsModel(QObject *parent)
    : UIModelData(
          parent,
          std::string("reskin panels model"),
          std::string("/ui/qml/reskin_windows_and_panels_model")) {}

void ReskinPanelsModel::close_panel(QModelIndex panel_index) {

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

        removeRows(panel_index.row(), 1, panel_index.parent());

        // get the divider positions from the parent
        QVariant dividers = get(panel_index.parent(), "child_dividers");
        if (dividers.userType() == QMetaType::QVariantList) {
            QList<QVariant> divs = dividers.toList();
            divs.removeAt(panel_index.row() ? panel_index.row() - 1 : 0);
            std::ignore = set(panel_index.parent(), divs, "child_dividers");
        }

    } else if (siblings == 2) {

        QModelIndex parentNode = panel_index.parent();

        // get the json data about the other panel that's not being deleted
        nlohmann::json other_panel_data = indexToFullData(index(
            !panel_index
                 .row(), // we have two rows ... we want the OTHER row to the one being removed
            0,
            parentNode));

        // now we wipe out the parent splitter with the 'other' panel
        setData(parentNode, QVariantMapFromJson(other_panel_data), Roles::JSONRole);

    } else {

        // do nothing if there is only one panel at this index - we can't
        // collapse a panel that isn't part of a split panel
    }
}

void ReskinPanelsModel::split_panel(QModelIndex panel_index, bool horizontal_split) {

    QModelIndex parentNode  = panel_index.parent();
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

        // do the insertion
        nlohmann::json j;
        j["current_tab"] = 0;
        j["children"]    = nlohmann::json::parse(R"([{"tab_view" : "Playlists"}])");

        insertRowsSync(insertion_row, 1, parentNode);
        setData(index(insertion_row, 0, parentNode), QVariantMapFromJson(j), Roles::JSONRole);


    } else {

        nlohmann::json current_panel_data = indexToFullData(panel_index);

        // parent splitter type does not match the type of split we want
        // so we need to replace the current panel with a new slitter
        // of the desired type

        // this is the data of the new splitter - new divider position is
        // at 0.5
        nlohmann::json j;
        j["child_dividers"]   = nlohmann::json::parse(R"([0.5])");
        j["split_horizontal"] = horizontal_split;

        // this is the new panel
        nlohmann::json new_child;
        new_child["current_tab"] = 0;
        new_child["children"]    = nlohmann::json::parse(R"([{"tab_view" : "Playlists"}])");

        // add the existing panel and new panel to the new splitter
        j["children"] = nlohmann::json::array();
        j["children"].push_back(current_panel_data);
        j["children"].push_back(new_child);

        // wipe out the existing panel with the new splitter and its children
        setData(panel_index, QVariantMapFromJson(j), Roles::JSONRole);
    }
}

void ReskinPanelsModel::duplicate_layout(QModelIndex layout_index) {

    nlohmann::json layout_data = indexToFullData(layout_index);
    int rc                     = rowCount(layout_index.parent());
    insertRowsSync(rc, 1, layout_index.parent());
    QModelIndex idx = index(rc, 0, layout_index.parent());
    setData(idx, QVariantMapFromJson(layout_data), Roles::JSONRole);
}


MediaListColumnsModel::MediaListColumnsModel(QObject *parent)
    : UIModelData(
          parent,
          std::string("media list columns model"),
          std::string("/ui/qml/media_list_columns_config")) {}

MenuModelItem::MenuModelItem(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());
}

MenuModelItem::~MenuModelItem() {

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    if (central_models_data_actor) {
        std::string menu_name_string = StdFromQString(menu_name_);
        anon_send(
            central_models_data_actor,
            ui::model_data::remove_node_atom_v,
            menu_name_string,
            model_entry_id_);
    }
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
                const std::string path) { emit activated(); },
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

    if (!dont_update_model_ && !menu_name_.isEmpty() && menu_path_ != "Undefined") {
        try {

            auto central_models_data_actor =
                self()->home_system().registry().template get<caf::actor>(
                    global_ui_model_data_registry);

            utility::JsonStore menu_item_data;
            std::string  name_string = StdFromQString(text_);
            menu_item_data["name"] = name_string;
            if (!hotkey_.isEmpty()) {
                std::string hotkey_string = StdFromQString(hotkey_);
                menu_item_data["hotkey"]  = hotkey_string;
            }
            if (!current_choice_.isEmpty()) {
                std::string current_choice_string = StdFromQString(current_choice_);
                menu_item_data["current_choice"]  = current_choice_string;
            }

            if (!choices_.empty()) {
                auto choices = nlohmann::json::parse("[]");
                for (const auto &c : choices_) {
                    choices.insert(choices.begin() + choices.size(), 1, StdFromQString(c));
                }
                menu_item_data["choices"] = choices;
            }
            if (menu_item_type_ == "toggle") {
                menu_item_data["is_checked"] = is_checked_;
            }
            menu_item_data["menu_item_position"] = menu_item_position_;
            std::string menu_item_type_string    = StdFromQString(menu_item_type_);
            menu_item_data["menu_item_type"]     = menu_item_type_string;
            menu_item_data["uuid"]               = model_entry_id_;


            anon_send(
                central_models_data_actor,
                ui::model_data::insert_or_update_menu_node_atom_v,
                StdFromQString(menu_name_),
                StdFromQString(menu_path_),
                menu_item_data,
                as_actor());

        } catch (std::exception &e) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
        }
    }
}
