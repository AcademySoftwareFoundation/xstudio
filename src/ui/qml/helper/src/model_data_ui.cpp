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

    if (model_name_ != StdFromQString(name)) {

        model_name_ = StdFromQString(name);

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
        emit modelDataNameChanged();
    }
}

void UIModelData::init(caf::actor_system &system) {
    super::init(system);

    self()->set_default_handler(caf::drop);

    try {
        utility::print_on_create(as_actor(), "SessionModel");
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {

            [=](utility::event_atom,
                xstudio::ui::model_data::set_node_data_atom,
                const std::string path,
                const utility::JsonStore &data) {
                try {

                    QModelIndex idx   = getPathIndex(nlohmann::json::json_pointer(path));
                    nlohmann::json &j = indexToData(idx);
                    j                 = data;
                    emit dataChanged(idx, idx, QVector<int>());
                } catch (std::exception &e) {
                    spdlog::warn(
                        "{} {} : {} {}", __PRETTY_FUNCTION__, e.what(), path, data.dump());
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::set_node_data_atom,
                const std::string path,
                const utility::JsonStore &data,
                const std::string role) {
                try {

                    QModelIndex idx   = getPathIndex(nlohmann::json::json_pointer(path));
                    nlohmann::json &j = indexToData(idx);
                    if (!j.contains(role) || j[role] != data) {
                        j[role] = data;
                        for (size_t i = 0; i < role_names_.size(); ++i) {
                            if (role_names_[i] == role) {
                                emit dataChanged(idx, idx, QVector<int>({Roles::LASTROLE + i}));
                                break;
                            }
                        }
                    }

                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }
            },
            [=](utility::event_atom,
                xstudio::ui::model_data::model_data_atom,
                const utility::JsonStore &data) { setModelData(data); }};
    });
}

bool UIModelData::setData(const QModelIndex &index, const QVariant &value, int role) {

    bool result = JSONTreeModel::setData(index, value, role);

    try {

        auto path = getIndexPath(index).to_string();

        if (role == Roles::JSONRole) {

            utility::JsonStore j;
            if (std::string(value.typeName()) == "QJSValue") {
                j = nlohmann::json::parse(
                    QJsonDocument::fromVariant(value.value<QJSValue>().toVariant())
                        .toJson(QJsonDocument::Compact)
                        .constData());
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

    setRoleNames(std::vector<std::string>{"view_name", "view_qml_path"});
    setModelDataName("view widgets");
}

void ViewsModelData::register_view(QString qml_path, QString view_name) {

    auto rc = rowCount(index(-1, -1)); // QModelIndex());
    insertRowsSync(rc, 1, index(-1, -1));
    QModelIndex view_reg_index = index(rc, 0, index(-1, -1));
    set(view_reg_index, QVariant(view_name), QString("view_name"));
    set(view_reg_index, QVariant(qml_path), QString("view_qml_path"));
}


ReskinPanelsModel::ReskinPanelsModel(QObject *parent)
    : UIModelData(
          parent,
          std::string("reskin panels model"),
          std::string("/ui/qml/reskin_windows_and_panels_model")) {}


MenuModelItem::MenuModelItem(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());
}

MenuModelItem::~MenuModelItem() {

    auto central_models_data_actor = self()->home_system().registry().template get<caf::actor>(
        global_ui_model_data_registry);

    if (central_models_data_actor) {
        anon_send(
            central_models_data_actor,
            ui::model_data::remove_node_atom_v,
            StdFromQString(menu_name_),
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
                const std::string path,
                const std::string role,
                const utility::JsonStore &data) {
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
            menu_item_data["name"] = StdFromQString(text_);
            if (!hotkey_.isEmpty())
                menu_item_data["hotkey"] = StdFromQString(hotkey_);
            if (!current_choice_.isEmpty())
                menu_item_data["current_choice"] = StdFromQString(current_choice_);
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
            menu_item_data["menu_item_type"]     = StdFromQString(menu_item_type_);
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
