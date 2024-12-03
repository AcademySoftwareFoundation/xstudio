// SPDX-License-Identifier: Apache-2.0
#include "xstudio/atoms.hpp"

#include "shotbrowser_engine_ui.hpp"
#include "shotgrid_provider_ui.hpp"
#include "model_ui.hpp"
#include "preset_model_ui.hpp"
#include "definitions.hpp"
#include "async_request.hpp"
#include "result_model_ui.hpp"

#include <QProcess>
#include <QQmlExtensionPlugin>
#include <qdebug.h>

using namespace std::chrono_literals;
using namespace xstudio::global_store;
using namespace xstudio::shotgun_client;
using namespace xstudio::data_source;
using namespace xstudio::ui::qml;
using namespace xstudio::utility;
using namespace xstudio;

ShotBrowserEngine *ShotBrowserEngine::this_ = nullptr;

ShotBrowserEngine::ShotBrowserEngine(QObject *parent) : QMLActor(parent) {

    presets_model_ = new ShotBrowserPresetModel(query_engine_, this);
    presets_model_->bindEventFunc(
        [this](auto &&PH1) { JSONTreeSendEventFunc(std::forward<decltype(PH1)>(PH1)); });

    query_engine_.bindLookupChangedFunc([this](auto &&PH1) {
        updateQueryEngineTermModel(std::forward<decltype(PH1)>(PH1), false);
    });
    query_engine_.bindCacheChangedFunc([this](auto &&PH1) {
        updateQueryEngineTermModel(std::forward<decltype(PH1)>(PH1), true);
    });

    updateQueryValueCache("Completion Location", locationsJSON);

    init(CafSystemObject::get_actor_system());
    set_backend(system().registry().template get<caf::actor>(shotbrowser_datasource_registry));
}

void ShotBrowserEngine::updateQueryEngineTermModel(const std::string &key, const bool cache) {
    presets_model_->updateTermModel(key, cache);
}

void ShotBrowserEngine::checkReady() {
    bool ready = true;

    if (not query_engine_.get_cache(QueryEngine::cache_name("Project")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("User")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Department")))
        ready = false;

    if (not query_engine_.get_cache(QueryEngine::cache_name("Pipeline Step")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Site")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Review Location")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Shot Status")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Playlist Type")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Production Status")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Note Type")))
        ready = false;

    if (not query_engine_.get_lookup(QueryEngine::cache_name("Pipeline Status")))
        ready = false;

    if (not presets_model_->rowCount())
        ready = false;

    if (ready) {
        ready_ = ready;
        emit readyChanged();
    }
}


ShotBrowserEngine *ShotBrowserEngine::instance() {
    // avoid creation of new instances
    if (this_ == nullptr) {
        this_ = new ShotBrowserEngine;
    }
    return this_;
}

QObject *ShotBrowserEngine::qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    // C++ and QML instance they are the same instance
    return ShotBrowserEngine::instance();
}

QObject *ShotBrowserEngine::presetsModel() { return dynamic_cast<QObject *>(presets_model_); }

void ShotBrowserEngine::JSONTreeSendEventFunc(const utility::JsonStore &event) {
    // spdlog::warn("{} {}", __PRETTY_FUNCTION__, event.dump(2));
    if (backend_) {
        anon_send(
            backend_,
            utility::event_atom_v,
            json_store::sync_atom_v,
            user_preset_event_id_,
            event);
    }
}

void ShotBrowserEngine::createGroupModel(const int project_id) {
    const auto key = query_engine_.cache_name("Group", project_id);

    if (not query_engine_.get_cache(key)) {
        query_engine_.set_cache(key, R"([])"_json);
        getGroupsFuture(project_id);
    }
}

void ShotBrowserEngine::createUserCache(const int project_id) {
    const auto key = query_engine_.cache_name("User", project_id);

    if (not query_engine_.get_cache(key)) {
        query_engine_.set_cache(key, R"([])"_json);
        getUsersFuture(project_id);
    }
}

void ShotBrowserEngine::createSequenceModels(const int project_id) {
    if (not sequences_tree_map_.count(project_id)) {
        sequences_tree_map_[project_id] = new ShotBrowserSequenceModel(this);
        getSequencesFuture(project_id);
    }
}

QObject *ShotBrowserEngine::sequenceTreeModel(const int project_id) {
    createSequenceModels(project_id);
    return sequences_tree_map_[project_id];
}

void ShotBrowserEngine::createShotCache(const int project_id) {
    const auto key = query_engine_.cache_name("Shot", project_id);

    if (not query_engine_.get_lookup(key)) {
        query_engine_.set_lookup(key, R"([])"_json);
        getShotsFuture(project_id);
    }
}

void ShotBrowserEngine::createStageCache(const int project_id) {
    const auto key = query_engine_.cache_name("Stage", project_id);

    if (not query_engine_.get_cache(key)) {
        query_engine_.set_cache(key, R"([])"_json);
        getStageFuture(project_id);
    }
}

void ShotBrowserEngine::createUnitCache(const int project_id) {
    const auto key = query_engine_.cache_name("Unit", project_id);

    if (not query_engine_.get_cache(key)) {
        query_engine_.set_cache(key, R"([])"_json);
        getCustomEntity24Future(project_id);
    }
}

void ShotBrowserEngine::createPlaylistCache(const int project_id) {
    const auto key = query_engine_.cache_name("Playlist", project_id);

    if (not query_engine_.get_lookup(key)) {
        query_engine_.set_lookup(key, R"([])"_json);
        getPlaylistsFuture(project_id);
    }
}

void ShotBrowserEngine::cacheProject(const int project_id) {
    // spdlog::warn("{} {}", __PRETTY_FUNCTION__, project_id);
    if (not QueryEngine::precache_needed(project_id, query_engine_.lookup()).empty()) {
        createGroupModel(project_id);
        createUserCache(project_id);
        createSequenceModels(project_id);
        createShotCache(project_id);
        createPlaylistCache(project_id);
        createUnitCache(project_id);
        createStageCache(project_id);
    }
}

QString ShotBrowserEngine::getProjectFromMetadata() const {
    auto result = QString();
    try {
        auto jp =
            json::json_pointer("/metadata/shotgun/version/relationships/project/data/name");

        if (live_link_metadata_.contains(jp)) {
            result = QStringFromStd(live_link_metadata_.at(jp));
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}

QString ShotBrowserEngine::getShotSequenceFromMetadata() const {
    auto result = QString();
    try {
        auto jp =
            json::json_pointer("/metadata/shotgun/version/relationships/entity/data/name");

        if (live_link_metadata_.contains(jp)) {
            result = QStringFromStd(live_link_metadata_.at(jp));
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
    return result;
}


void ShotBrowserEngine::setLiveLinkKey(const QVariant &key) {
    if (live_link_key_ != key) {
        live_link_key_ = key;
        emit liveLinkKeyChanged();
    }
}

void ShotBrowserEngine::setLiveLinkMetadata(QString &data) {
    if (data == "null" or data == "")
        data = "{}";

    if (data != live_link_metadata_string_) {
        try {
            live_link_metadata_        = JsonStore(nlohmann::json::parse(StdFromQString(data)));
            live_link_metadata_string_ = data;

            auto project    = query_engine_.get_project_name(live_link_metadata_);
            auto project_id = query_engine_.get_project_id(live_link_metadata_);

            // update model caches.
            cacheProject(project_id);

            emit projectChanged(project_id, QStringFromStd(project));
        } catch (const std::exception &err) {
            spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), StdFromQString(data));
            live_link_metadata_        = JsonStore(R"({})"_json);
            live_link_metadata_string_ = "{}";
        }

        emit liveLinkMetadataChanged();
    }
}

QString ShotBrowserEngine::getShotgunUserName() {
    QString result; // = QString(get_user_name());

    // auto ind =
    //     qvariant_cast<ShotBrowserListModel *>(term_models_->value("userModel"))
    //         ->search(QVariant::fromValue(QString(get_login_name().c_str())), "loginRole");
    // if (ind != -1)
    //     result = qvariant_cast<ShotBrowserListModel *>(term_models_->value("userModel"))
    //                  ->get(ind)
    //                  .toString();

    return result;
}

void ShotBrowserEngine::init(caf::actor_system &system) {
    QMLActor::init(system);

    // access global data store to retrieve stored filters.
    // delay this just in case we're to early..
    delayed_anon_send(as_actor(), std::chrono::seconds(2), shotgun_preferences_atom_v);

    // request data for presets.


    set_message_handler([=](actor_companion *) -> message_handler {
        return {
            [=](shotgun_acquire_authentication_atom, const std::string &message) {
                emit requestSecret(QStringFromStd(message));
            },
            [=](shotgun_preferences_atom) {
                // loadPresets();
                loadPresetModelFuture();
            },

            [=](shotgun_preferences_atom, const std::string &preset) {
                // flushPreset(preset);
            },
            // [=](utility::event_atom,
            //     put_data_atom,
            //     const std::string &path,
            //     const JsonStore &data) {
            //     spdlog::warn("setModelData {}", data.dump(2));
            //     presets_model_->setModelPathData(path, data);
            // },

            [=](utility::event_atom,
                json_store::sync_atom,
                const Uuid &uuid,
                const JsonStore &event) {
                if (uuid == user_preset_event_id_) {
                    presets_model_->receiveEvent(event);
                    if (not ready_)
                        checkReady();
                } else if (uuid == system_preset_event_id_) {
                    // spdlog::warn("json_store::sync_atom {} system_preset_event_id_",
                    // event.dump(2));
                    query_engine_.system_presets().process_event(event);
                }
            },

            // catchall for dealing with results from shotgun
            [=](shotgun_info_atom, const JsonStore &request, const JsonStore &data) {
                try {
                    if (request.at("type") == "project") {
                        query_engine_.set_cache(query_engine_.cache_name("Project"), data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "user_preset_model") {
                        user_preset_event_id_ = data.at("uuid");
                        presets_model_->setModelPathData("", data.at("data"));
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "system_preset_model") {
                        system_preset_event_id_ = data.at("uuid");
                        query_engine_.system_presets().reset_data(data.at("data"));
                    } else if (request.at("type") == "user") {
                        const auto project_id = request.at("project_id").get<int>();
                        if (project_id != -1) {
                            updateQueryValueCache("User", data, project_id);
                            query_engine_.set_cache(
                                query_engine_.cache_name("User", project_id), data);
                        } else {
                            updateQueryValueCache("User", data);
                            if (not ready_)
                                checkReady();
                        }
                    } else if (request.at("type") == "department") {
                        updateQueryValueCache("Department", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "location") {
                        updateQueryValueCache("Site", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "review_location") {
                        updateQueryValueCache("Review Location", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "reference_tag") {
                        // qvariant_cast<ShotBrowserListModel *>(
                        //     term_models_->value("referenceTagModel"))
                        //     ->setModelData(data.at("data"));
                    } else if (request.at("type") == "shot_status") {
                        updateQueryValueCache("Exclude Shot Status", data);
                        updateQueryValueCache("Shot Status", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "playlist_type") {
                        updateQueryValueCache("Playlist Type", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "unit") {
                        const auto project_id = request.at("project_id").get<int>();
                        updateQueryValueCache("Unit", data, project_id);
                        query_engine_.set_cache(
                            query_engine_.cache_name("Unit", project_id), data);
                    } else if (request.at("type") == "stage") {
                        const auto project_id = request.at("project_id").get<int>();
                        updateQueryValueCache("Stage", data, project_id);
                        query_engine_.set_cache(
                            query_engine_.cache_name("Stage", project_id), data);
                    } else if (request.at("type") == "production_status") {
                        updateQueryValueCache("Production Status", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "pipe_step") {
                        query_engine_.set_cache(
                            query_engine_.cache_name("Pipeline Step"), data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "note_type") {
                        updateQueryValueCache("Note Type", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "pipeline_status") {
                        updateQueryValueCache("Pipeline Status", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "sequence_status") {
                        updateQueryValueCache("Sequence Status", data);
                        if (not ready_)
                            checkReady();
                    } else if (request.at("type") == "group") {
                        const auto project_id = request.at("project_id").get<int>();
                        query_engine_.set_cache(
                            query_engine_.cache_name("Group", project_id), data);
                    } else if (request.at("type") == "sequence") {
                        updateQueryValueCache(
                            "Sequence", data, request.at("project_id").get<int>());

                        sequences_tree_map_[request.at("project_id")]->setModelData(
                            ShotBrowserSequenceModel::flatToTree(data));
                    } else if (request.at("type") == "shot") {
                        updateQueryValueCache(
                            "Shot", data, request.at("project_id").get<int>());
                    } else if (request.at("type") == "playlist") {
                        updateQueryValueCache(
                            "Playlist", data, request.at("project_id").get<int>());
                    }
                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                }
            },

            // catchall for dealing with results from shotgun
            [=](shotgun_info_atom, const JsonStore &request) {},


            [=](broadcast::broadcast_down_atom, const caf::actor_addr &) {},
            [=](const group_down_msg & /*msg*/) {
                //      if(msg.source == store_events)
                // unsubscribe();
            }};
    });
}

void ShotBrowserEngine::setConnected(const bool connected) {
    if (connected_ != connected) {
        connected_ = connected;
        if (connected_) {
            populateCaches();
        }
        emit connectedChanged();
    }
}

void ShotBrowserEngine::authenticate(const bool cancel) {
    anon_send(backend_, shotgun_acquire_authentication_atom_v, cancel);
}

void ShotBrowserEngine::set_backend(caf::actor backend) {
    backend_ = backend;
    // this forces the use of futures to access datasource
    // if this is not done the application will deadlock if the password
    // is requested, as we'll already be in this class,
    // this is because the UI runs in a single thread.
    // I'm not aware of any solutions to this.
    // we can use timeouts though..
    // but as we're accessing a remote service, we'll need to be careful..

    scoped_actor sys{system()};

    if (backend_events_) {
        try {
            request_receive<bool>(
                *sys, backend_events_, broadcast::leave_broadcast_atom_v, as_actor());
        } catch (const std::exception &) {
        }
        // self()->demonitor(backend_events_);
        backend_events_ = caf::actor();
    }

    if (backend_) {
        try {
            backend_events_ =
                request_receive<caf::actor>(*sys, backend_, get_event_group_atom_v);
            request_receive<bool>(
                *sys, backend_events_, broadcast::join_broadcast_atom_v, as_actor());
        } catch (const std::exception &) {
        }
        anon_send(backend_, module::connect_to_ui_atom_v);
        anon_send(backend_, shotgun_authentication_source_atom_v, as_actor());
    }
}

void ShotBrowserEngine::setBackendId(const QString &qid) {
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

void ShotBrowserEngine::populateCaches() {
    getProjectsFuture();
    getUsersFuture();
    getPipeStepFuture();
    getDepartmentsFuture();
    getReferenceTagsFuture();

    getSchemaFieldsFuture("location");
    getSchemaFieldsFuture("review_location");
    getSchemaFieldsFuture("playlist_type");
    getSchemaFieldsFuture("shot_status");
    getSchemaFieldsFuture("note_type");
    getSchemaFieldsFuture("production_status");
    getSchemaFieldsFuture("pipeline_status");
    getSchemaFieldsFuture("sequence_status");
}


QFuture<QString> ShotBrowserEngine::requestFileTransferFuture(
    const QVariantList &qitems,
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

        std::vector<std::string> items;
        for (const auto &i : qitems) {
            if (i.userType() == QMetaType::QUrl)
                items.emplace_back(uri_to_posix_path(UriFromQUrl(i.toUrl())));
            else
                items.emplace_back(to_string(UuidFromQUuid(i.toUuid())));
        }

        if (src_location.isEmpty())
            args.push_back(QStringFromStd(join_as_string(items, ",")));
        else
            args.push_back(src_location + ":" + QStringFromStd(join_as_string(items, ",")));

        args.push_back(dest_location);

        qint64 pid;
        QProcess::startDetached(program, args, "", &pid);

        return result;
    });
}


// void ShotBrowserEngine::updateModel(const QString &qname) {
//     auto name = StdFromQString(qname);

//     if (name == "referenceTagModel")
//         getReferenceTagsFuture();
// }

void ShotBrowserEngine::receivedDataSlot(
    const QPersistentModelIndex &index, const int role, const QString &result) {
    if (index.isValid()) {
        // this might be bad..
        auto model = const_cast<QAbstractItemModel *>(index.model());
        model->setData(
            index, mapFromValue(nlohmann::json::parse(StdFromQString(result))), role);
    }
}


void ShotBrowserEngine::requestData(
    const QPersistentModelIndex &index, const int role, const nlohmann::json &request) const {

    auto tmp = new ShotBrowserResponse(index, role, request, backend_);

    connect(tmp, &ShotBrowserResponse::received, this, &ShotBrowserEngine::receivedDataSlot);
}

QFuture<bool> ShotBrowserEngine::undoFuture() {
    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            scoped_actor sys{system()};
            result = request_receive<bool>(
                *sys,
                backend_,
                history::undo_atom_v,
                utility::sys_time_duration(std::chrono::milliseconds(500)));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}

QFuture<bool> ShotBrowserEngine::redoFuture() {
    return QtConcurrent::run([=]() {
        auto result = false;
        try {
            scoped_actor sys{system()};
            result = request_receive<bool>(
                *sys,
                backend_,
                history::redo_atom_v,
                utility::sys_time_duration(std::chrono::milliseconds(500)));
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        }

        return result;
    });
}


//![plugin]
class ShotBrowserUIQml : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

  public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<ShotBrowserSequenceFilterModel>(
            uri, 1, 0, "ShotBrowserSequenceFilterModel");
        qmlRegisterType<ShotBrowserPresetTreeFilterModel>(
            uri, 1, 0, "ShotBrowserPresetTreeFilterModel");
        qmlRegisterType<ShotBrowserResultModel>(uri, 1, 0, "ShotBrowserResultModel");
        qmlRegisterType<ShotBrowserFilterModel>(uri, 1, 0, "ShotBrowserFilterModel");
        qmlRegisterType<ShotBrowserResultFilterModel>(
            uri, 1, 0, "ShotBrowserResultFilterModel");
        qmlRegisterType<ShotBrowserPresetFilterModel>(
            uri, 1, 0, "ShotBrowserPresetFilterModel");
        qmlRegisterSingletonType<ShotBrowserEngine>(
            uri, 1, 0, "ShotBrowserEngine", &ShotBrowserEngine::qmlInstance);
    }
    void initializeEngine(QQmlEngine *engine, const char *uri) override {
        engine->addImageProvider(QLatin1String("shotgrid"), new ShotgridProvider);
    }
};
//![plugin]

#include "shotbrowser_engine_ui.moc"
