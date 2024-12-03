// SPDX-License-Identifier: Apache-2.0

#include "xstudio/ui/qml/global_store_model_ui.hpp"
#include "xstudio/global_store/global_store.hpp"

CAF_PUSH_WARNINGS
CAF_POP_WARNINGS

using namespace caf;
using namespace xstudio;
using namespace xstudio::ui::qml;

namespace {
QString capitaliseFirst(QString in) {
    QStringList parts = in.split("_");
    for (int i = 0; i < parts.size(); ++i) {
        parts[i] = parts[i].replace(0, 1, parts[i][0].toUpper());
    }
    return parts.join(" ");
}
} // namespace

GlobalStoreModel::GlobalStoreModel(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    setRoleNames(std::vector<std::string>(
        {"nameRole",
         "pathRole",
         "datatypeRole",
         "contextRole",
         "valueRole",
         "descriptionRole",
         "defaultValueRole",
         "overriddenValueRole",
         "overriddenPathRole"}));
}


void GlobalStoreModel::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(caf::drop);

    // join global store events.
    try {
        scoped_actor sys{system()};
        utility::print_on_create(as_actor(), "GlobalStoreModel");
        gsh_ = std::make_shared<global_store::GlobalStoreHelper>(system());

        utility::JsonStore tmp;
        auto gsh_group = gsh_->get_group(tmp);

        utility::request_receive<bool>(
            *sys, gsh_group, broadcast::join_broadcast_atom_v, as_actor());

        auto tree        = R"({})"_json;
        tree["children"] = storeToTree(tmp);

        // spdlog::warn("{}", tree.dump(2));

        setModelData(tree);

        // connect up auto save.
        {
            try {
                auto grp = utility::request_receive<caf::actor>(
                    *sys, gsh_->get_actor(), utility::get_event_group_atom_v);
                utility::request_receive<bool>(
                    *sys, grp, broadcast::join_broadcast_atom_v, as_actor());
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }

            try {
                autosave_ = utility::request_receive<bool>(
                    *sys, gsh_->get_actor(), xstudio::global_store::autosave_atom_v);
                emit autosaveChanged();
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](json_store::update_atom,
                const utility::JsonStore &change,
                const std::string &path,
                const utility::JsonStore &) { updateProperty(path, change); },

            [=](json_store::update_atom, const utility::JsonStore &json) {
                auto tree        = R"({})"_json;
                tree["children"] = storeToTree(json);
                setModelData(tree);
            },

            [=](utility::event_atom, global_store::autosave_atom, bool enabled) {
                if (autosave_ != enabled) {
                    autosave_ = enabled;
                    emit autosaveChanged();
                }
            },

            [=](utility::event_atom, utility::last_changed_atom, const utility::time_point &) {
            },
            [=](utility::event_atom,
                json_store::update_atom,
                const utility::JsonStore &,
                const std::string &) {},
            [=](utility::event_atom, utility::change_atom) {},


            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &g) {
                caf::aout(self())
                    << "GlobalStoreModel down: " << to_string(g.source) << std::endl;
            }};
    });
}

void GlobalStoreModel::setAutosave(const bool enabled) {
    scoped_actor sys{system()};
    autosave_ = enabled;
    sys->send(gsh_->get_actor(), xstudio::global_store::autosave_atom_v, enabled);
    gsh_->set_value(enabled, "/ui/qml/autosave");
    emit autosaveChanged();
}


//   "context": [
//     "QML_UI"
//   ],
//   "datatype": "string",
//   "default_value": "Playlists",
//   "description": "Default context.",
//   "overridden_path": "/u/al/.config/DNEG/xstudio/preferences/qml_ui.json",
//   "overridden_value": "Versions",
//   "path": "/plugin/data_source/shotgun/context",
//   "value": "Reference"
// } /plugin/data_source/shotgun/context


bool GlobalStoreModel::updateProperty(
    const std::string &path, const utility::JsonStore &change) {
    auto result = false;
    auto index =
        searchRecursive(QVariant::fromValue(QStringFromStd(path)), QString("pathRole"));

    try {
        // single term update
        //  should handle more that value...
        if (not index.isValid() and ends_with(path, "/value")) {
            index = searchRecursive(
                QVariant::fromValue(QStringFromStd(path.substr(0, path.find_last_of('/')))),
                QString("pathRole"));
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                if (j["value"] != change) {
                    // spdlog::warn("json differ {} {}", j.dump(2), change.dump(2));
                    QVector<int> roles({Roles::valueRole});
                    j["value"] = change;
                    result     = true;
                    emit dataChanged(index, index, roles);
                }
            }
        } else if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            // compare to current preset.
            if (change != j) {
                QVector<int> roles({});
                result = true;
                // spdlog::warn("json differ {} {}", j.dump(2), change.dump(2));

                auto fields = std::map<std::string, int>(
                    {{"default_value", Roles::defaultValueRole},
                     {"value", Roles::valueRole},
                     {"overridden_path", Roles::overriddenPathRole},
                     {"overridden_value", Roles::overriddenValueRole},
                     {"description", Roles::descriptionRole},
                     {"context", Roles::contextRole},
                     {"path", Roles::pathRole},
                     {"datatype", Roles::datatypeRole}});

                for (const auto &i : fields) {
                    if (j.count(i.first) and j.at(i.first) != change.at(i.first)) {
                        j[i.first] = change.at(i.first);
                        roles.push_back(i.second);
                    }
                }

                emit dataChanged(index, index, roles);
            }
        } else {
            spdlog::warn(
                "{} Unmatched update {} {}", __PRETTY_FUNCTION__, path, change.dump(2));
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }


    return result;
}


// convert to internal representation.
nlohmann::json GlobalStoreModel::storeToTree(const nlohmann::json &src) {

    auto result = R"([])"_json;

    for (const auto &[k, v] : src.items()) {
        if (v.count("datatype")) {
            // spdlog::warn("{}", v.dump(2));
            result.push_back(v);
        } else if (v.is_object()) {
            auto item        = R"({})"_json;
            item["path"]     = k;
            item["children"] = storeToTree(v);
            result.emplace_back(item);
        }
    }

    return result;
}

QVariant GlobalStoreModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);

        switch (role) {
        case JSONTreeModel::Roles::JSONRole:
            result = QVariantMapFromJson(j);
            break;

        case JSONTreeModel::Roles::JSONTextRole:
            result = QString::fromStdString(j.dump(2));
            break;

        case Roles::datatypeRole:
            if (j.count("datatype"))
                result = QString::fromStdString(j.at("datatype").get<std::string>());
            else
                result = QString::fromStdString("group");
            break;

        case Roles::pathRole:
            result = QString::fromStdString(j.at("path").get<std::string>());
            break;

        case Roles::contextRole: {
            auto tmp = QStringList();
            for (const auto &i : j.at("context"))
                tmp.append(QStringFromStd(i.get<std::string>()));
            result = tmp;
        } break;

        case Roles::descriptionRole:
            result = QString::fromStdString(j.at("description").get<std::string>());
            break;

        case Roles::overriddenPathRole:
            result = QString::fromStdString(j.at("overridden_path").get<std::string>());
            break;

        case Roles::nameRole:
        case Qt::DisplayRole:
            result = QString::fromStdString(j.at("path"));
            break;

        case Roles::valueRole:
            result = mapFromValue(j.at("value"));
            break;

        case Roles::defaultValueRole:
            result = mapFromValue(j.at("default_value"));
            break;

        case Roles::overriddenValueRole:
            result = mapFromValue(j.at("overridden_value"));
            break;

        default:
            break;
        }
    } catch (const std::exception &err) {

        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalId());
    }

    return result;
}

bool GlobalStoreModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        if (role == Roles::valueRole and index.isValid()) {
            nlohmann::json &j = indexToData(index);

            // test for change ?
            // push to global data model
            // spdlog::warn("variant {}", value.typeName());
            j["value"] = mapFromValue(value);
            // spdlog::warn("json {} ", j["value"].dump(2));

            if (gsh_)
                gsh_->set_value(j["value"], j["path"], false);
            result = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result) {
        emit dataChanged(index, index, roles);
    }

    return result;
}

PublicPreferencesModel::PublicPreferencesModel(QObject *parent) : super(parent) {
    init(CafSystemObject::get_actor_system());

    setRoleNames(std::vector<std::string>(
        {"nameRole",
         "pathRole",
         "datatypeRole",
         "contextRole",
         "valueRole",
         "descriptionRole",
         "defaultValueRole",
         "overriddenValueRole",
         "overriddenPathRole",
         "displayNameRole",
         "categoryRole",
         "optionsRole"}));
}


void PublicPreferencesModel::init(caf::actor_system &_system) {
    super::init(_system);

    self()->set_default_handler(caf::drop);

    // join global store events.
    try {
        scoped_actor sys{system()};
        utility::print_on_create(as_actor(), "PublicPreferencesModel");
        gsh_ = std::make_shared<global_store::GlobalStoreHelper>(system());

        utility::JsonStore tmp;
        auto gsh_group = gsh_->get_group(tmp);

        utility::request_receive<bool>(
            *sys, gsh_group, broadcast::join_broadcast_atom_v, as_actor());

        buildModel(tmp);

        // connect up auto save.
        {
            try {
                auto grp = utility::request_receive<caf::actor>(
                    *sys, gsh_->get_actor(), utility::get_event_group_atom_v);
                utility::request_receive<bool>(
                    *sys, grp, broadcast::join_broadcast_atom_v, as_actor());
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }

            try {
                autosave_ = utility::request_receive<bool>(
                    *sys, gsh_->get_actor(), xstudio::global_store::autosave_atom_v);
                emit autosaveChanged();
            } catch (const std::exception &e) {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
            }
        }


    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    set_message_handler([=](caf::actor_companion * /*self*/) -> caf::message_handler {
        return {
            [=](json_store::update_atom,
                const utility::JsonStore &change,
                const std::string &path,
                const utility::JsonStore &) { updateProperty(path, change); },

            [=](json_store::update_atom, const utility::JsonStore &json) { buildModel(json); },

            [=](utility::event_atom, global_store::autosave_atom, bool enabled) {
                if (autosave_ != enabled) {
                    autosave_ = enabled;
                    emit autosaveChanged();
                }
            },

            [=](utility::event_atom, utility::last_changed_atom, const utility::time_point &) {
            },
            [=](utility::event_atom,
                json_store::update_atom,
                const utility::JsonStore &,
                const std::string &) {},
            [=](utility::event_atom, utility::change_atom) {},


            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg &g) {
                caf::aout(self())
                    << "GlobalStoreModel down: " << to_string(g.source) << std::endl;
            }};
    });
}

void PublicPreferencesModel::setAutosave(const bool enabled) {
    scoped_actor sys{system()};
    autosave_ = enabled;
    sys->send(gsh_->get_actor(), xstudio::global_store::autosave_atom_v, enabled);
    gsh_->set_value(enabled, "/ui/qml/autosave");
    emit autosaveChanged();
}


//   "context": [
//     "QML_UI"
//   ],
//   "datatype": "string",
//   "default_value": "Playlists",
//   "description": "Default context.",
//   "overridden_path": "/u/al/.config/DNEG/xstudio/preferences/qml_ui.json",
//   "overridden_value": "Versions",
//   "path": "/plugin/data_source/shotgun/context",
//   "value": "Reference"
// } /plugin/data_source/shotgun/context


bool PublicPreferencesModel::updateProperty(
    const std::string &path, const utility::JsonStore &change) {
    auto result = false;
    auto index =
        searchRecursive(QVariant::fromValue(QStringFromStd(path)), QString("pathRole"));

    try {
        // single term update
        //  should handle more that value...
        if (not index.isValid() and ends_with(path, "/value")) {
            index = searchRecursive(
                QVariant::fromValue(QStringFromStd(path.substr(0, path.find_last_of('/')))),
                QString("pathRole"));
            if (index.isValid()) {
                nlohmann::json &j = indexToData(index);
                if (j["value"] != change) {
                    // spdlog::warn("json differ {} {}", j.dump(2), change.dump(2));
                    QVector<int> roles({Roles::valueRole});
                    j["value"] = change;
                    result     = true;
                    emit dataChanged(index, index, roles);
                }
            }
        } else if (index.isValid()) {
            nlohmann::json &j = indexToData(index);
            // compare to current preset.
            if (change != j) {
                QVector<int> roles({});
                result = true;
                // spdlog::warn("json differ {} {}", j.dump(2), change.dump(2));

                static const auto fields = std::map<std::string, int>(
                    {{"default_value", Roles::defaultValueRole},
                     {"value", Roles::valueRole},
                     {"overridden_path", Roles::overriddenPathRole},
                     {"overridden_value", Roles::overriddenValueRole},
                     {"description", Roles::descriptionRole},
                     {"context", Roles::contextRole},
                     {"path", Roles::pathRole},
                     {"datatype", Roles::datatypeRole},
                     {"options", Roles::optionsRole},
                     {"display_name", Roles::displayNameRole},
                     {"category", Roles::categoryRole}});

                for (const auto &i : fields) {
                    if (j.count(i.first) and j.at(i.first) != change.at(i.first)) {
                        j[i.first] = change.at(i.first);
                        roles.push_back(i.second);
                    }
                }

                emit dataChanged(index, index, roles);
            }
        } else {
            spdlog::warn(
                "{} Unmatched update {} {}", __PRETTY_FUNCTION__, path, change.dump(2));
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }


    return result;
}

void PublicPreferencesModel::buildModel(const utility::JsonStore &entireGlobalStore) {

    // the idea here is we flatten the global store into a tree of depth 2.
    // The first level lets us group preferences (that we want the user to
    // access) accorind to their 'category'. Under this we just duplicate each
    // preference in that category as a child node. In QML our repeaters/list
    // view does the rest to make widgets for the prefs.

    auto tree = R"({"children": []})"_json;
    storeToTree(entireGlobalStore, tree);
    setModelData(tree);
}

// convert to internal representation.
void PublicPreferencesModel::storeToTree(const nlohmann::json &src, nlohmann::json &tree) {

    auto find_or_make_child = [](nlohmann::json &tree,
                                 const std::string &category) -> nlohmann::json & {
        nlohmann::json &children = tree["children"];
        for (auto &v : children) {

            if (v["category"].get<std::string>() == category) {
                return v;
            }
        }

        nlohmann::json new_child = R"({})"_json;
        new_child["category"]    = category;
        children.push_back(new_child);
        return children.back();
    };

    auto result = R"([])"_json;

    for (const auto &[k, v] : src.items()) {
        if (v.count("category") && v["category"].is_string()) {

            const std::string category     = v["category"].get<std::string>();
            nlohmann::json &category_group = find_or_make_child(tree, category);
            category_group["children"].push_back(v);

        } else if (v.is_object()) {
            storeToTree(v, tree);
        }
    }
}

QVariant PublicPreferencesModel::data(const QModelIndex &index, int role) const {
    auto result = QVariant();

    try {
        const auto &j = indexToData(index);
        switch (role) {
        case JSONTreeModel::Roles::JSONRole:
            result = QVariantMapFromJson(j);
            break;

        case JSONTreeModel::Roles::JSONTextRole:
            result = QString::fromStdString(j.dump(2));
            break;

        case Roles::datatypeRole:
            if (j.count("datatype"))
                result = QString::fromStdString(j.at("datatype").get<std::string>());
            else
                result = QString::fromStdString("group");
            break;

        case Roles::pathRole:
            if (j.contains("path")) {
                result = QString::fromStdString(j.at("path").get<std::string>());
            }
            break;

        case Roles::contextRole: {
            auto tmp = QStringList();
            for (const auto &i : j.at("context"))
                tmp.append(QStringFromStd(i.get<std::string>()));
            result = tmp;
        } break;

        case Roles::descriptionRole:
            result = QString::fromStdString(j.at("description").get<std::string>());
            break;

        case Roles::overriddenPathRole:
            result = QString::fromStdString(j.at("overridden_path").get<std::string>());
            break;

        case Roles::valueRole:
            result = mapFromValue(j.at("value"));
            break;

        case Roles::defaultValueRole:
            result = mapFromValue(j.at("default_value"));
            break;

        case Roles::overriddenValueRole:
            result = mapFromValue(j.at("overridden_value"));
            break;

        case Roles::categoryRole:
            result = mapFromValue(j.at("category"));
            break;

        case Roles::optionsRole:
            result = mapFromValue(j.at("options"));
            break;
        case Roles::nameRole:
        case Qt::DisplayRole:
        case Roles::displayNameRole:
            if (j.contains("display_name")) {
                result = mapFromValue(j.at("display_name"));
            } else {
                QString p = mapFromValue(j.at("path")).toString();
                result    = capitaliseFirst(p.mid(p.lastIndexOf("/") + 1));
            }
            break;

        default:
            break;
        }
    } catch (const std::exception &err) {

        spdlog::warn(
            "{} {} {} {} {}",
            __PRETTY_FUNCTION__,
            err.what(),
            role,
            index.row(),
            index.internalId());
    }

    return result;
}

bool PublicPreferencesModel::setData(
    const QModelIndex &index, const QVariant &value, int role) {
    bool result = false;

    QVector<int> roles({role});

    try {
        if (role == Roles::valueRole and index.isValid()) {
            nlohmann::json &j = indexToData(index);

            // test for change ?
            // push to global data model
            // spdlog::warn("variant {}", value.typeName());
            j["value"] = mapFromValue(value);
            // spdlog::warn("json {} ", j["value"].dump(2));

            if (gsh_)
                gsh_->set_value(j["value"], j["path"], false);
            result = true;
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (result) {
        emit dataChanged(index, index, roles);
    }

    return result;
}

// "context": [
//    "QML_UI"
//  ],
//  "datatype": "json",
//  "default_value": {
//    "properties": null
//  },
//  "description": "Prefs relating to window position.",
//  "overridden_path": "/u/al/.config/DNEG/xstudio/preferences/qml_ui.json",
//  "overridden_value": {
//    "height": 1166,
//    "properties": null,
//    "template_key": "/ui/qml/shotgun_browser_settings",
//    "visibility": 0,
//    "width": 1643,
//    "x": 1098,
//    "y": 497
//  },
