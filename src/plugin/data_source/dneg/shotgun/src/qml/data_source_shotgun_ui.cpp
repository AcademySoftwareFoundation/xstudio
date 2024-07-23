// SPDX-License-Identifier: Apache-2.0
#include "data_source_shotgun_ui.hpp"
#include "shotgun_model_ui.hpp"

#include "../data_source_shotgun.hpp"
#include "../data_source_shotgun_definitions.hpp"

#include "xstudio/utility/string_helpers.hpp"
#include "xstudio/ui/qml/json_tree_model_ui.hpp"
#include "xstudio/global_store/global_store.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/ui/qml/module_ui.hpp"
#include "xstudio/utility/chrono.hpp"

#include <QProcess>
#include <QQmlExtensionPlugin>
#include <qdebug.h>

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::ui::qml;
using namespace std::chrono_literals;
using namespace xstudio::global_store;

const auto PresetModelLookup = std::map<std::string, std::string>(
    {{"edit", "editPresetsModel"},
     {"edit_filter", "editFilterModel"},
     {"media_action", "mediaActionPresetsModel"},
     {"media_action_filter", "mediaActionFilterModel"},
     {"note", "notePresetsModel"},
     {"note_filter", "noteFilterModel"},
     {"note_tree", "noteTreePresetsModel"},
     {"playlist", "playlistPresetsModel"},
     {"playlist_filter", "playlistFilterModel"},
     {"reference", "referencePresetsModel"},
     {"reference_filter", "referenceFilterModel"},
     {"shot", "shotPresetsModel"},
     {"shot_filter", "shotFilterModel"},
     {"shot_tree", "shotTreePresetsModel"}});

const auto PresetPreferenceLookup = std::map<std::string, std::string>(
    {{"edit", "presets/edit"},
     {"edit_filter", "global_filters/edit"},
     {"media_action", "presets/media_action"},
     {"media_action_filter", "global_filters/media_action"},
     {"note", "presets/note"},
     {"note_filter", "global_filters/note"},
     {"note_tree", "presets/note_tree"},
     {"playlist", "presets/playlist"},
     {"playlist_filter", "global_filters/playlist"},
     {"reference", "presets/reference"},
     {"reference_filter", "global_filters/reference"},
     {"shot", "presets/shot"},
     {"shot_filter", "global_filters/shot"},
     {"shot_tree", "presets/shot_tree"}});


ShotgunDataSourceUI::ShotgunDataSourceUI(QObject *parent) : QMLActor(parent) {

    term_models_   = new QQmlPropertyMap(this);
    result_models_ = new QQmlPropertyMap(this);
    preset_models_ = new QQmlPropertyMap(this);

    term_models_->insert(
        "primaryLocationModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("primaryLocationModel"))
        ->populate(locationsJSON);

    term_models_->insert("stepModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("stepModel"))->populate(R"([
        {"name": "None"}
    ])"_json);

    term_models_->insert("twigTypeCodeModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("twigTypeCodeModel"))
        ->populate(TwigTypeCodes);

    term_models_->insert("onDiskModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("onDiskModel"))->populate(R"([
        {"name": "chn"},
        {"name": "lon"},
        {"name": "mtl"},
        {"name": "mum"},
        {"name": "syd"},
        {"name": "van"}
    ])"_json);

    term_models_->insert("lookbackModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("lookbackModel"))->populate(R"([
        {"name": "Today"},
        {"name": "1 Day"},
        {"name": "3 Days"},
        {"name": "7 Days"},
        {"name": "20 Days"},
        {"name": "30 Days"},
        {"name": "30-60 Days"},
        {"name": "60-90 Days"},
        {"name": "100-150 Days"},
        {"name": "Future Only"}
    ])"_json);

    term_models_->insert("boolModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("boolModel"))->populate(R"([
        {"name": "True"},
        {"name": "False"}
    ])"_json);

    term_models_->insert("resultLimitModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("resultLimitModel"))->populate(R"([
        {"name": "1"},
        {"name": "2"},
        {"name": "4"},
        {"name": "8"},
        {"name": "10"},
        {"name": "20"},
        {"name": "40"},
        {"name": "100"},
        {"name": "200"},
        {"name": "500"},
        {"name": "1000"},
        {"name": "2500"},
        {"name": "4500"}
    ])"_json);

    term_models_->insert("orderByModel", QVariant::fromValue(new ShotgunListModel(this)));
    qvariant_cast<ShotgunListModel *>(term_models_->value("orderByModel"))->populate(R"([
        {"name": "Date And Time ASC"},
        {"name": "Date And Time DESC"},
        {"name": "Created ASC"},
        {"name": "Created DESC"},
        {"name": "Updated ASC"},
        {"name": "Updated DESC"},
        {"name": "Client Submit ASC"},
        {"name": "Client Submit DESC"},
        {"name": "Version ASC"},
        {"name": "Version DESC"}
    ])"_json);

    updateQueryValueCache(
        "Completion Location",
        qvariant_cast<ShotgunListModel *>(term_models_->value("primaryLocationModel"))
            ->modelData());
    updateQueryValueCache("TwigTypeCode", TwigTypeCodes);

    term_models_->insert("shotStatusModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("projectModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("userModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("referenceTagModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("departmentModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("locationModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "reviewLocationModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("playlistTypeModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "productionStatusModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert("noteTypeModel", QVariant::fromValue(new ShotgunListModel(this)));
    term_models_->insert(
        "pipelineStatusModel", QVariant::fromValue(new ShotgunListModel(this)));

    {
        auto model  = new ShotModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setSequenceMap(&sequences_map_);
        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("shotResultsModel", QVariant::fromValue(filter));
        result_models_->insert("shotResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new ShotModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setSequenceMap(&sequences_map_);
        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("shotTreeResultsModel", QVariant::fromValue(filter));
        result_models_->insert("shotTreeResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new PlaylistModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("playlistResultsModel", QVariant::fromValue(filter));
        result_models_->insert("playlistResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model = new EditModel(this);
        model->populate(
            R"([{"name": "Edit 001"}, {"name": "Edit 002"}, {"name": "Edit 003"}, {"name": "Edit 004"}])"_json);

        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("editResultsModel", QVariant::fromValue(filter));
        result_models_->insert("editResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new ReferenceModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        model->setSequenceMap(&sequences_map_);
        filter->setSourceModel(model);

        result_models_->insert("referenceResultsModel", QVariant::fromValue(filter));
        result_models_->insert("referenceResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new NoteModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("noteResultsModel", QVariant::fromValue(filter));
        result_models_->insert("noteResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new NoteModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);

        result_models_->insert("noteTreeResultsModel", QVariant::fromValue(filter));
        result_models_->insert("noteTreeResultsBaseModel", QVariant::fromValue(model));
    }

    {
        auto model  = new MediaActionModel(this);
        auto filter = new ShotgunFilterModel(this);

        model->setQueryValueCache(&query_value_cache_);
        filter->setSourceModel(model);
        model->setSequenceMap(&sequences_map_);

        result_models_->insert("mediaActionResultsModel", QVariant::fromValue(filter));
        result_models_->insert("mediaActionResultsBaseModel", QVariant::fromValue(model));
    }

    for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
             {"shotPresetsModel", "shotFilterModel"},
             {"shotTreePresetsModel", "shotFilterModel"},
             {"playlistPresetsModel", "playlistFilterModel"},
             {"editPresetsModel", "editFilterModel"},
             {"referencePresetsModel", "referenceFilterModel"},
             {"notePresetsModel", "noteFilterModel"},
             {"noteTreePresetsModel", "noteFilterModel"},
             {"mediaActionPresetsModel", "mediaActionFilterModel"}}) {

        if (not preset_models_->contains(QStringFromStd(m))) {
            auto model = new ShotgunTreeModel(this);
            model->setSequenceMap(&sequences_map_);
            preset_models_->insert(QStringFromStd(m), QVariant::fromValue(model));
        }

        if (not preset_models_->contains(QStringFromStd(f))) {
            auto filter = new ShotgunTreeModel(this);
            preset_models_->insert(QStringFromStd(f), QVariant::fromValue(filter));
        }
    }

    init(CafSystemObject::get_actor_system());

    for (const auto &[m, n] : std::vector<std::tuple<std::string, std::string>>{
             {"shotPresetsModel", "shot"},
             {"shotTreePresetsModel", "shot_tree"},
             {"playlistPresetsModel", "playlist"},
             {"editPresetsModel", "edit"},
             {"referencePresetsModel", "reference"},
             {"notePresetsModel", "note"},
             {"noteTreePresetsModel", "note_tree"},
             {"mediaActionPresetsModel", "media_action"},

             {"shotFilterModel", "shot_filter"},
             {"playlistFilterModel", "playlist_filter"},
             {"editFilterModel", "edit_filter"},
             {"referenceFilterModel", "reference_filter"},
             {"noteFilterModel", "note_filter"},
             {"mediaActionFilterModel", "media_action_filter"}}) {
        connect(
            qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(m))),
            &ShotgunTreeModel::modelChanged,
            this,
            [this, n = n]() { syncModelChanges(QStringFromStd(n)); });
    }
}

QObject *ShotgunDataSourceUI::groupModel(const int project_id) {
    if (not groups_map_.count(project_id)) {
        groups_map_[project_id]        = new ShotgunListModel(this);
        groups_filter_map_[project_id] = new ShotgunFilterModel(this);
        groups_filter_map_[project_id]->setSourceModel(groups_map_[project_id]);

        getGroupsFuture(project_id);
    }

    return groups_filter_map_[project_id];
}

void ShotgunDataSourceUI::createSequenceModels(const int project_id) {
    if (not sequences_map_.count(project_id)) {
        sequences_map_[project_id]      = new ShotgunListModel(this);
        sequences_tree_map_[project_id] = new ShotgunSequenceModel(this);
        getSequencesFuture(project_id);
    }
}

QObject *ShotgunDataSourceUI::sequenceModel(const int project_id) {
    createSequenceModels(project_id);
    return sequences_map_[project_id];
}

QObject *ShotgunDataSourceUI::sequenceTreeModel(const int project_id) {
    createSequenceModels(project_id);
    return sequences_tree_map_[project_id];
}

void ShotgunDataSourceUI::createShotModels(const int project_id) {
    if (not shots_map_.count(project_id)) {
        shots_map_[project_id]        = new ShotgunListModel(this);
        shots_filter_map_[project_id] = new ShotgunFilterModel(this);
        shots_filter_map_[project_id]->setSourceModel(shots_map_[project_id]);
        getShotsFuture(project_id);
    }
}

void ShotgunDataSourceUI::createCustomEntity24Models(const int project_id) {
    if (not custom_entity_24_map_.count(project_id)) {
        custom_entity_24_map_[project_id] = new ShotgunListModel(this);
        getCustomEntity24Future(project_id);
    }
}


QObject *ShotgunDataSourceUI::shotModel(const int project_id) {
    createShotModels(project_id);
    return shots_map_[project_id];
}

QObject *ShotgunDataSourceUI::customEntity24Model(const int project_id) {
    createCustomEntity24Models(project_id);
    return custom_entity_24_map_[project_id];
}

QObject *ShotgunDataSourceUI::shotSearchFilterModel(const int project_id) {
    createShotModels(project_id);
    return shots_filter_map_[project_id];
}

QObject *ShotgunDataSourceUI::playlistModel(const int project_id) {
    if (not playlists_map_.count(project_id)) {
        playlists_map_[project_id] = new ShotgunListModel(this);
        getPlaylistsFuture(project_id);
    }

    return playlists_map_[project_id];
}

//  unused ?
QString ShotgunDataSourceUI::getShotSequence(const int project_id, const QString &shot) {
    QString result;

    if (sequences_map_.count(project_id)) {
        // get data..
        const auto &data = sequences_map_[project_id]->modelData();

        auto needle = StdFromQString(shot);

        for (const auto &i : data) {
            try {
                for (const auto &s : i.at("relationships").at("shots").at("data")) {
                    if (s.at("type") == "Shot" and s.at("name") == needle) {
                        result = QStringFromStd(i.at("attributes").at("code"));
                        break;
                    }
                }
            } catch (...) {
            }
            if (not result.isEmpty())
                break;
        }
    }

    return result;
}

void ShotgunDataSourceUI::syncModelChanges(const QString &preset) {
    auto tmp = StdFromQString(preset);

    // the change has already happened
    if (not(preset_update_pending_.count(tmp) and preset_update_pending_[tmp])) {
        preset_update_pending_[tmp] = true;

        delayed_anon_send(as_actor(), std::chrono::seconds(5), shotgun_preferences_atom_v, tmp);
    }
}


void ShotgunDataSourceUI::setLiveLinkMetadata(QString &data) {
    if (data == "null")
        data = "{}";

    try {
        if (data != live_link_metadata_string_) {
            try {
                live_link_metadata_ = JsonStore(nlohmann::json::parse(StdFromQString(data)));
                live_link_metadata_string_ = data;
            } catch (...) {
            }

            try {
                auto project = live_link_metadata_.at("metadata")
                                   .at("shotgun")
                                   .at("version")
                                   .at("relationships")
                                   .at("project")
                                   .at("data")
                                   .at("name")
                                   .get<std::string>();
                auto project_id = live_link_metadata_.at("metadata")
                                      .at("shotgun")
                                      .at("version")
                                      .at("relationships")
                                      .at("project")
                                      .at("data")
                                      .at("id")
                                      .get<int>();

                // update model caches.
                groupModel(project_id);
                sequenceModel(project_id);
                shotModel(project_id);
                playlistModel(project_id);

                emit projectChanged(project_id, QStringFromStd(project));
            } catch (...) {
            }

            // trigger update of models with new livelink data..
            for (const auto &i :
                 {"shotPresetsModel",
                  "shotTreePresetsModel",
                  "playlistPresetsModel",
                  "editPresetsModel",
                  "referencePresetsModel",
                  "notePresetsModel",
                  "noteTreePresetsModel",
                  "mediaActionPresetsModel"})
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(i))
                    ->updateLiveLinks(live_link_metadata_);

            emit liveLinkMetadataChanged();
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

QString ShotgunDataSourceUI::getShotgunUserName() {
    QString result; // = QString(get_user_name());

    auto ind =
        qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
            ->search(QVariant::fromValue(QString(get_login_name().c_str())), "loginRole");
    if (ind != -1)
        result = qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
                     ->get(ind)
                     .toString();

    return result;
}

// remove old system presets from user preferences.
utility::JsonStore ShotgunDataSourceUI::purgeOldSystem(
    const utility::JsonStore &vprefs, const utility::JsonStore &dprefs) const {

    utility::JsonStore result = dprefs;

    size_t count = 0;

    for (const auto &i : vprefs) {
        try {
            if (i.value("type", "") == "system")
                count++;
        } catch (...) {
        }
    }

    if (count != result.size()) {
        spdlog::warn("Purging old presets. {} {}", count, result.size());
        for (const auto &i : vprefs) {
            try {
                if (i.value("type", "") == "system")
                    continue;
            } catch (...) {
            }
            result.push_back(i);
        }
    } else {
        result = vprefs;
    }

    return result;
}

void ShotgunDataSourceUI::populatePresetModel(
    const utility::JsonStore &prefs,
    const std::string &path,
    ShotgunTreeModel *model,
    const bool purge_old,
    const bool clear_flags) {
    try {
        // replace embedded envvars
        std::string tmp;

        // spdlog::warn("path {}", path);

        if (purge_old)
            tmp = expand_envvars(purgeOldSystem(
                                     preference_value<JsonStore>(prefs, path),
                                     preference_default_value<JsonStore>(prefs, path))
                                     .dump());
        else
            tmp = expand_envvars(preference_value<JsonStore>(prefs, path).dump());

        model->populate(JsonStore(nlohmann::json::parse(tmp)));
        if (clear_flags) {
            model->clearLoaded();
            model->clearExpanded();
            model->clearLiveLinks();
        } else {
            // filter model set index 0 as active.
            model->setActivePreset(0);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

Q_INVOKABLE void ShotgunDataSourceUI::resetPreset(const QString &qpreset, const int index) {
    try {
        auto prefs              = GlobalStoreHelper(system());
        auto preset             = StdFromQString(qpreset);
        auto defval             = JsonStore();
        ShotgunTreeModel *model = nullptr;

        if (preset == "Versions")
            preset = "shot";
        else if (preset == "Playlists")
            preset = "playlist";
        else if (preset == "Notes")
            preset = "note";
        else if (preset == "Media Actions")
            preset = "media_action";
        else if (preset == "Edits")
            preset = "edit";
        else if (preset == "Reference")
            preset = "reference";
        else if (preset == "Versions Tree")
            preset = "shot_tree";
        else if (preset == "Notes Tree")
            preset = "note_tree";

        if (PresetModelLookup.count(preset)) {
            defval = prefs.default_value<JsonStore>(
                "/plugin/data_source/shotgun/" + PresetPreferenceLookup.at(preset));
            model = qvariant_cast<ShotgunTreeModel *>(
                preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))));
        }

        if (model == nullptr)
            return;

        // expand envs
        defval = JsonStore(nlohmann::json::parse(expand_envvars(defval.dump())));

        // update and add
        // update globals ?
        // and presets..
        JsonStore data(model->modelData().at("queries"));

        // massage format..
        for (const auto &i : defval) {
            auto name = i.at("name").get<std::string>();
            // find name in current..
            auto found = false;
            for (int j = 0; j < static_cast<int>(data.size()); j++) {
                if (data[j].at("name") == name) {
                    found = true;

                    if (index != -1 && j != index)
                        break;

                    data[j] = i;
                    break;
                }
            }
            // no match then insert,,

            if (not found)
                data.push_back(i);
        }

        // reset model..
        model->populate(data);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotgunDataSourceUI::loadPresets(const bool purge_old) {
    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore js;
        prefs.get_group(js);

        try {
            maximum_result_count_ =
                preference_value<int>(js, "/plugin/data_source/shotgun/maximum_result_count");
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        try {
            qvariant_cast<ShotgunListModel *>(term_models_->value("stepModel"))
                ->populate(
                    preference_value<JsonStore>(js, "/plugin/data_source/shotgun/pipestep"));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
                 {"shot", "shotPresetsModel"},
                 {"shot_tree", "shotTreePresetsModel"},
                 {"playlist", "playlistPresetsModel"},
                 {"reference", "referencePresetsModel"},
                 {"edit", "editPresetsModel"},
                 {"note", "notePresetsModel"},
                 {"note_tree", "noteTreePresetsModel"},
                 {"media_action", "mediaActionPresetsModel"}}) {
            populatePresetModel(
                js,
                "/plugin/data_source/shotgun/presets/" + m,
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(f))),
                purge_old,
                true);
        }

        for (const auto &[m, f] : std::vector<std::tuple<std::string, std::string>>{
                 {"shot", "shotFilterModel"},
                 {"playlist", "playlistFilterModel"},
                 {"reference", "referenceFilterModel"},
                 {"edit", "editFilterModel"},
                 {"note", "noteFilterModel"},
                 {"media_action", "mediaActionFilterModel"}}) {
            populatePresetModel(
                js,
                "/plugin/data_source/shotgun/global_filters/" + m,
                qvariant_cast<ShotgunTreeModel *>(preset_models_->value(QStringFromStd(f))),
                false,
                false);
        }

        disable_flush_ = false;
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotgunDataSourceUI::handleResult(
    const JsonStore &request, const std::string &model, const std::string &name) {

    if (not epoc_map_.count(request.at("context").at("type")) or
        epoc_map_.at(request.at("context").at("type")) < request.at("context").at("epoc")) {
        auto slm =
            qvariant_cast<ShotgunListModel *>(result_models_->value(QStringFromStd(model)));

        slm->populate(request.at("result").at("data"));
        slm->setTruncated(request.at("context").at("truncated").get<bool>());

        source_selection_[name] = std::make_pair(
            request.at("context").at("visual_source").get<std::string>(),
            request.at("context").at("audio_source").get<std::string>());
        flag_selection_[name] = std::make_pair(
            request.at("context").at("flag_text").get<std::string>(),
            request.at("context").at("flag_colour").get<std::string>());
        epoc_map_[request.at("context").at("type")] = request.at("context").at("epoc");
    }
}

void ShotgunDataSourceUI::init(caf::actor_system &system) {
    QMLActor::init(system);

    // access global data store to retrieve stored filters.
    // delay this just in case we're to early..
    delayed_anon_send(as_actor(), std::chrono::seconds(2), shotgun_preferences_atom_v);

    set_message_handler([=](actor_companion *) -> message_handler {
        return {
            [=](shotgun_acquire_authentication_atom, const std::string &message) {
                emit requestSecret(QStringFromStd(message));
            },
            [=](shotgun_preferences_atom) { loadPresets(); },

            [=](shotgun_preferences_atom, const std::string &preset) { flushPreset(preset); },

            // catchall for dealing with results from shotgun
            [=](shotgun_info_atom, const JsonStore &request, const JsonStore &data) {
                try {
                    if (request.at("type") == "project")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("projectModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "user") {
                        qvariant_cast<ShotgunListModel *>(term_models_->value("userModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("User", data.at("data"));
                    } else if (request.at("type") == "department") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("departmentModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Department", data.at("data"));
                    } else if (request.at("type") == "location")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("locationModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "review_location")
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("reviewLocationModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "reference_tag")
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("referenceTagModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "shot_status") {
                        updateQueryValueCache("Exclude Shot Status", data.at("data"));
                        updateQueryValueCache("Shot Status", data.at("data"));
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("shotStatusModel"))
                            ->populate(data.at("data"));
                    } else if (request.at("type") == "playlist_type")
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("playlistTypeModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "custom_entity_24") {
                        custom_entity_24_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Unit", data.at("data"), request.at("id").get<int>());
                    } else if (request.at("type") == "production_status") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("productionStatusModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Production Status", data.at("data"));
                    } else if (request.at("type") == "note_type")
                        qvariant_cast<ShotgunListModel *>(term_models_->value("noteTypeModel"))
                            ->populate(data.at("data"));
                    else if (request.at("type") == "pipeline_status") {
                        qvariant_cast<ShotgunListModel *>(
                            term_models_->value("pipelineStatusModel"))
                            ->populate(data.at("data"));
                        updateQueryValueCache("Pipeline Status", data.at("data"));
                    } else if (request.at("type") == "group")
                        groups_map_[request.at("id")]->populate(data.at("data"));
                    else if (request.at("type") == "sequence") {
                        sequences_map_[request.at("id")]->populate(data.at("data"));
                        sequences_tree_map_[request.at("id")]->setModelData(
                            ShotgunSequenceModel::flatToTree(data.at("data")));
                    } else if (request.at("type") == "shot") {
                        shots_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Shot", data.at("data"), request.at("id").get<int>());
                    } else if (request.at("type") == "playlist") {
                        playlists_map_[request.at("id")]->populate(data.at("data"));
                        updateQueryValueCache(
                            "Playlist", data.at("data"), request.at("id").get<int>());
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            // catchall for dealing with results from shotgun
            [=](shotgun_info_atom, const JsonStore &request) {
                try {
                    auto type = request.at("context").at("type").get<std::string>();

                    if (type == "playlist_result") {
                        handleResult(request, "playlistResultsBaseModel", "Playlists");
                    } else if (type == "shot_result") {
                        handleResult(request, "shotResultsBaseModel", "Versions");
                    } else if (type == "shot_tree_result") {
                        handleResult(request, "shotTreeResultsBaseModel", "Versions Tree");
                    } else if (type == "media_action_result") {
                        handleResult(request, "mediaActionResultsBaseModel", "Menu Setup");
                    } else if (type == "reference_result") {
                        handleResult(request, "referenceResultsBaseModel", "Reference");
                    } else if (type == "note_result") {
                        handleResult(request, "noteResultsBaseModel", "Notes");
                    } else if (type == "note_tree_result") {
                        handleResult(request, "noteTreeResultsBaseModel", "Notes Tree");
                    } else if (type == "edit_result") {
                        handleResult(request, "editResultsBaseModel", "Edits");
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },


            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                //      if(msg.source == store_events)
                // unsubscribe();
            }};
    });
}

void ShotgunDataSourceUI::undo() {
    auto tmp = history_.undo();
    if (tmp) {
        auto current = getPresetData((*tmp).first);
        if (current != (*tmp).second) {
            // spdlog::warn("undo {}", (*tmp).first);
            setPreset((*tmp).first, (*tmp).second);
        } else
            undo();
    }
}

void ShotgunDataSourceUI::redo() {
    auto tmp = history_.redo();
    if (tmp) {
        auto current = getPresetData((*tmp).first);
        if (current != (*tmp).second) {
            // spdlog::warn("redo {}", (*tmp).first);
            setPreset((*tmp).first, (*tmp).second);
        } else
            redo();
    }
}

void ShotgunDataSourceUI::snapshot(const QString &preset) {
    auto tmp  = StdFromQString(preset);
    auto data = getPresetData(tmp);

    // spdlog::warn("snapshot {}", tmp);

    history_.push(std::make_pair(tmp, data));
}

void ShotgunDataSourceUI::setPreset(const std::string &preset, const utility::JsonStore &data) {
    if (PresetModelLookup.count(preset))
        qvariant_cast<ShotgunTreeModel *>(
            preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
            ->populate(data);
}

utility::JsonStore ShotgunDataSourceUI::getPresetData(const std::string &preset) {
    if (PresetModelLookup.count(preset))
        return qvariant_cast<ShotgunTreeModel *>(
                   preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
            ->modelData()
            .at("queries");

    return utility::JsonStore();
}

void ShotgunDataSourceUI::flushPreset(const std::string &preset) {
    if (disable_flush_)
        return;

    preset_update_pending_[preset] = false;
    // flush relevant changes to global dict.
    auto prefs = GlobalStoreHelper(system());

    if (PresetModelLookup.count(preset)) {
        prefs.set_value(
            qvariant_cast<ShotgunTreeModel *>(
                preset_models_->value(QStringFromStd(PresetModelLookup.at(preset))))
                ->modelData()
                .at("queries"),
            "/plugin/data_source/shotgun/" + PresetPreferenceLookup.at(preset));
    }
}

void ShotgunDataSourceUI::setConnected(const bool connected) {
    if (connected_ != connected) {
        connected_ = connected;
        if (connected_) {
            populateCaches();
        }
        emit connectedChanged();
    }
}

void ShotgunDataSourceUI::authenticate(const bool cancel) {
    anon_send(backend_, shotgun_acquire_authentication_atom_v, cancel);
}

void ShotgunDataSourceUI::set_backend(caf::actor backend) {
    backend_ = backend;
    // this forces the use of futures to access datasource
    // if this is not done the application will deadlock if the password
    // is requested, as we'll already be in this class,
    // this is because the UI runs in a single thread.
    // I'm not aware of any solutions to this.
    // we can use timeouts though..
    // but as we're accessing a remote service, we'll need to be careful..

    anon_send(backend_, module::connect_to_ui_atom_v);
    anon_send(backend_, shotgun_authentication_source_atom_v, as_actor());
}

void ShotgunDataSourceUI::setBackendId(const QString &qid) {
    caf::actor_id id = std::stoull(StdFromQString(qid).c_str());

    if (id and id != backend_id_) {
        auto actor = system().registry().template get<caf::actor>(id);
        if (actor) {
            set_backend(actor);
            backend_str_ = QStringFromStd(to_string(backend_));
            backend_id_  = backend_.id();
            emit backendChanged();
            emit backendIdChanged();
        } else {
            spdlog::warn("{} Actor lookup failed", __PRETTY_FUNCTION__);
        }
    }
}

void ShotgunDataSourceUI::populateCaches() {
    getProjectsFuture();
    getUsersFuture();
    getDepartmentsFuture();
    getReferenceTagsFuture();

    getSchemaFieldsFuture("playlist", "sg_location", "location");
    getSchemaFieldsFuture("playlist", "sg_review_location_1", "review_location");
    getSchemaFieldsFuture("playlist", "sg_type", "playlist_type");

    getSchemaFieldsFuture("shot", "sg_status_list", "shot_status");

    getSchemaFieldsFuture("note", "sg_note_type", "note_type");
    getSchemaFieldsFuture("version", "sg_production_status", "production_status");
    getSchemaFieldsFuture("version", "sg_status_list", "pipeline_status");
}

JsonStore ShotgunDataSourceUI::buildDataFromField(const JsonStore &data) {
    auto result = R"({"data": []})"_json;

    std::map<std::string, std::string> entries;

    for (const auto &i : data["data"]["properties"]["valid_values"]["value"]) {
        auto value = i.get<std::string>();
        auto key   = value;
        if (data["data"]["properties"].count("display_values") and
            data["data"]["properties"]["display_values"]["value"].count(value)) {
            key =
                data["data"]["properties"]["display_values"]["value"][value].get<std::string>();
        }

        entries.insert(std::make_pair(key, value));
    }

    for (const auto &i : entries) {
        auto field                  = R"({"id": null, "attributes": {"name": null}})"_json;
        field["attributes"]["name"] = i.first;
        field["id"]                 = i.second;
        result["data"].push_back(field);
    }

    return JsonStore(result);
}

// QFuture<QString> ShotgunDataSourceUI::refreshPlaylistNotesFuture(const QUuid &playlist) {
//     return QtConcurrent::run([=]() {
//         if (backend_) {
//             try {
//                 scoped_actor sys{system()};
//                 auto req             = JsonStore(RefreshPlaylistNotesJSON);
//                 req["playlist_uuid"] = to_string(UuidFromQUuid(playlist));
//                 auto js              = request_receive_wait<JsonStore>(
//                     *sys, backend_, SHOTGUN_TIMEOUT, data_source::use_data_atom_v, req);
//                 return QStringFromStd(js.dump());
//             } catch (const XStudioError &err) {
//                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
//                 auto error                = R"({'error':{})"_json;
//                 error["error"]["source"]  = to_string(err.type());
//                 error["error"]["message"] = err.what();
//                 return QStringFromStd(JsonStore(error).dump());
//             } catch (const std::exception &err) {
//                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
//                 return QStringFromStd(err.what());
//             }
//         }
//         return QString();
//     });
// }


QFuture<QString> ShotgunDataSourceUI::requestFileTransferFuture(
    const QVariantList &uuids,
    const QString &project,
    const QString &src_location,
    const QString &dest_location) {
    return QtConcurrent::run([=]() {
        QString program = "dnenv-do";
        QString result;
        auto show_env = utility::get_env("SHOW");
        auto shot_env = utility::get_env("SHOT");

        QStringList args = {
            QStringFromStd(show_env ? *show_env : "NSFL"),
            QStringFromStd(shot_env ? *shot_env : "ldev_pipe"),
            "--",
            "ft-cp",
            // "-n",
            "-debug",
            "-show",
            project,
            "-e",
            "production",
            "--watchers",
            QStringFromStd(get_login_name()),
            "-priority",
            "medium",
            "-description",
            "File transfer requested by xStudio"};

        std::vector<std::string> dnuuids;
        for (const auto &i : uuids) {
            dnuuids.emplace_back(to_string(UuidFromQUuid(i.toUuid())));
        }
        args.push_back(src_location + ":" + QStringFromStd(join_as_string(dnuuids, ",")));
        args.push_back(dest_location);

        qint64 pid;
        QProcess::startDetached(program, args, "", &pid);

        return result;
    });
}


void ShotgunDataSourceUI::updateModel(const QString &qname) {
    auto name = StdFromQString(qname);

    if (name == "referenceTagModel")
        getReferenceTagsFuture();
}


// ft-cp -n -debug -show MEG2 -e production  b3ae9497-6218-4124-8f8e-07158e3165dd mum --watchers
// al -priority medium -description "File transfer requested by xStudio


//![plugin]
class QShotgunDataSourceQml : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

  public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<ShotgunDataSourceUI>(uri, 1, 0, "ShotgunDataSource");
    }
};
//![plugin]

#include "data_source_shotgun_ui.moc"
