// SPDX-License-Identifier: Apache-2.0

#include "demo_plugin.hpp"
#include "demo_media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/media/media_actor.hpp"

CAF_PUSH_WARNINGS
#include <QFile>
CAF_POP_WARNINGS

using namespace xstudio;
using namespace xstudio::demo_plugin;

inline int num_json_rows(const nlohmann::json *node) {
    int v = node->contains("rows") && node->at("rows").is_array() ? int(node->at("rows").size())
                                                                  : -1;
    return v;
}

DemoPlugin::DemoPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "DemoPlugin", init_settings) {

    // Necessary to register our custom types and atoms with CAF so that they
    // can be used in message passing
    caf::init_global_meta_objects<caf::id_block::xstudio_demo_plugin>();

    // Here we add an 'Attribute' (from the Module base class) that will both
    // tell us the 'project' (i.e. fake mock production) that the user has
    // selected and also the LIST of available projects. We can use this sort
    // of attribute to drive multi-choice widtgets in the UI.
    current_project_ =
        add_string_choice_attribute("Current Project", "Current Project", "", {}, {});

    // this makes the attribute accessible from the QML code via a named
    // attribute group
    current_project_->expose_in_ui_attrs_group("demo_plugin_attributes");

    // We must make this call in the plugin constructor to initialise the
    // base class
    make_behavior();

    // we require this call so that our plugin instance receives updates from
    // the UI relating to it's attributes
    connect_to_ui();

    // Create a hotkey shotcut (CTRL+SHIFT+D) that will launch the plugin panel
    // in a pop-out window
    auto show_demo_plugin_panel_hotkey = register_hotkey(
        int('D'),
        ui::ControlModifier + ui::ShiftModifier,
        "Show Demo Plugin panel",
        "Shows or hides the pop-out Demo Plugin Panel");

    register_ui_panel_qml(
        "Demo Plugin",
        R"(
            import DemoPlugin 1.0
            DemoPluginPanel {
                anchors.fill: parent
            }
        )",
        100.1f, // position in panels drop-down menu, high number ensures it's last in the list
                // in this case
        "qrc:/demo_plugin_icons/demo_plugin.svg",
        5.0f,
        show_demo_plugin_panel_hotkey);

    // here we send ourselves a message - this will trigger the initialise_database
    // method (which we can't exectute in the constructor. See corresponding
    // message handler below)
    anon_mail(demo_plugin_custom_atom_v).send(caf::actor_cast<caf::actor>(this));
}

utility::JsonStore DemoPlugin::qml_item_callback(
    const utility::Uuid &qml_item_id, const utility::JsonStore &callback_data) {

    try {

        const auto action = callback_data.value("action", "");
        if (action == "LOAD_MEDIA_INTO_PLAYLIST") {

            auto playlist_uuid = callback_data["playlist_id"].get<utility::Uuid>();

            // studio actor - the application object
            auto studiod_actor = system().registry().template get<caf::actor>(studio_registry);

            // Get the current session (actor) from studio - the session is the
            // owner of playlists.
            // We send a message to the studio_actor and when it responds (some time
            // later AFTER we have returned from qml_item_callback) the
            // function_pointer/lambda passed in to 'then' will be executed.
            //
            // The signature for these message requests is
            // mail(args...).request(actor, timeout).then(response_handler(lmabda),
            // error_hander(lambda));
            mail(session::session_atom_v)
                .request(studiod_actor, infinite)
                .then(
                    [=](caf::actor session) mutable {
                        // Now we get the playlist actor. For reference, instead
                        // of using mail(args).request(actor, timeout).then(responsehander,
                        // error_hander) we use a blocking request_receive. This is synchronous,
                        // but has drawbacks as for big complex operations like adding many
                        // media items you lose the advantage of asynchronous parallelism.
                        // request_receive MUST be within a try/catch block as any errors
                        // returned by the caf system are thrown as exceptions. The template
                        // type must match the return type of the message handler that gets
                        // called.
                        caf::scoped_actor sys{system()};
                        try {

                            auto playlist = utility::request_receive<caf::actor>(
                                *sys, session, session::get_playlist_atom_v, playlist_uuid);

                            const auto media_items =
                                callback_data["media_to_add"]; // this is now const
                                                               // nlohmann::json &
                            if (!media_items.is_array()) {
                                throw std::runtime_error(
                                    "media_to_add entry should be a json array.");
                                return;
                            }
                            utility::UuidActor first;
                            auto p = media_items.begin();
                            while (p != media_items.end()) {
                                const nlohmann::json &j = *p;
                                const std::string name  = j["version_name"];
                                const std::string path  = j["media_path"];

                                const std::string fr = j["frame_range"];

                                static const std::regex frame_range("^([0-9]+)\\-([0-9]+)$");
                                int in_frame  = 0;
                                int out_frame = 0;
                                std::cmatch m;
                                if (std::regex_search(fr.c_str(), m, frame_range)) {
                                    try {
                                        in_frame  = std::stoi(m[1].str());
                                        out_frame = std::stoi(m[2].str());
                                    } catch (...) {
                                    }
                                }

                                auto _uri = caf::make_uri(path);
                                if (!_uri) {
                                    spdlog::warn(
                                        "{} : Failed to make a valid URI for path {}",
                                        __PRETTY_FUNCTION__,
                                        path);
                                    p++;
                                    continue;
                                }

                                auto new_media = request_receive<utility::UuidActor>(
                                    *sys,
                                    playlist,
                                    playlist::add_media_atom_v,
                                    name,
                                    *_uri,
                                    utility::FrameList(),
                                    utility::Uuid());

                                if (!first)
                                    first = new_media;

                                // now we can add our media json data as metadata - again, I am
                                // doing this with a blocking call because I want to make sure
                                // the metadata is set before we proceed (the reason is that
                                // I use the metadata in the add_proxy_media_source ... if I
                                // 'lazily' set the metadata then my add_proxy_media_source
                                // could get triggered by xSTUDIO before the metadata I need
                                // is set)
                                request_receive<bool>(
                                    *sys,
                                    new_media.actor(),
                                    json_store::set_json_atom_v,
                                    new_media.uuid(), // Note 1
                                    utility::JsonStore(j),
                                    "/metadata/pipeline/demo_plugin");

                                // Note 1 - when setting metadata on a Media actor, the uuid
                                // should be null if we want to set the metadata on the Media
                                // actor itself. Media Actors contain one or more MediaSource
                                // actors (each with their own uuid - like most objects in
                                // xSTUDIO). You can use the MediaSource uuid to set metadata at
                                // the MediaSource level instead. In our case, we're just going
                                // to add the metadata to the top level Media actor.

                                p++;
                            }

                            if (callback_data.value("put_on_screen", false) && first) {

                                // how do we put a particular piece of media in the viewport?

                                // First, we need to make the Playlist the viewed media
                                // container: Note we can use 'mail' here because the
                                // corresponding message handler in PlaylistActor does not
                                // return a value
                                mail(session::viewport_active_media_container_atom_v, playlist)
                                    .send(session);

                                // Now we need the first media item we added to be selected in
                                // the playlist
                                auto playlis_selectio_actor =
                                    utility::request_receive<caf::actor>(
                                        *sys, playlist, playlist::selection_actor_atom_v);

                                utility::UuidList selected_media_ids({first.uuid()});
                                // Here we use anon_mail because the corresponding message
                                // handler in the PlayheadSelectionActor class returns a value
                                // ... if we use 'mail' the result is sent back to us as a
                                // message and we do not have a message handler for that data.
                                anon_mail(playlist::select_media_atom_v, selected_media_ids)
                                    .send(playlis_selectio_actor);
                            }

                        } catch (std::exception &e) {
                            spdlog::warn(
                                "{} : add media error - {}", __PRETTY_FUNCTION__, e.what());
                        }
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        }

    } catch (std::exception &e) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
    }
    return utility::JsonStore();
}

void DemoPlugin::add_proxy_media_source(
    caf::actor media, caf::typed_response_promise<utility::UuidActorVector> rp) {

    mail(
        json_store::get_json_atom_v,
        utility::Uuid(), // uuid of MediaSource within the media item .. null means we get
                         // metadata from the media item
        "/metadata")
        .request(media, infinite)
        .then(
            [=](const utility::JsonStore &metadata) mutable {
                utility::UuidActorVector result;
                try {

                    std::string media_path = metadata.at(
                        utility::JsonStore::json_pointer("/pipeline/demo_plugin/media_path"));
                    if (utility::ends_with(media_path, ".fake")) {


                        utility::replace_string_in_place(media_path, ".fake", ".proxy.fake");

                        auto _uri       = caf::make_uri(media_path);
                        const auto uuid = utility::Uuid::generate();

                        // this is one of OUR demo media files. We are going to modify the
                        // URI to include .proxy. which will indicate to our custom media
                        // reader that we want a lower-res version of the same images.
                        auto proxy_source = spawn<media::MediaSourceActor>(
                            "FAKE_proxy", // media source name (shows in the 'Source' popup to
                                          // the right of viewport toolbar)
                            *_uri,
                            utility::FrameRate(),
                            uuid);

                        result.emplace_back(uuid, proxy_source);
                    }

                } catch (std::exception &e) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, e.what());
                }

                rp.deliver(result);
            },
            [=](const caf::error &err) mutable { rp.deliver(utility::UuidActorVector()); });
}

caf::message_handler DemoPlugin::message_handler_extensions() {
    return caf::message_handler{

        // Because we are a PF_DATA_SOURCE type plguin, we must provide some
        // message handlers for 'data_source::use_data_atom' with specific type
        // signatures. xSTUDIO will send these messages to us when certain events
        // happen so that we have an opportunity to take action
        [=](data_source::use_data_atom,
            const caf::actor &media,
            const utility::FrameRate &media_rate) -> result<utility::UuidActorVector> {
            // Here, when a new media item is added to xSTUDIO, we can add new
            // MediaSources to the media item. For example, if a pipeline movie is added
            // by some other plugin, say, or by the user or by a python script we could
            // look at the metadata of the media item and go and find the EXRs that the movie
            // was generated from and add them as a media source to the media item
            auto rp = make_response_promise<utility::UuidActorVector>();
            add_proxy_media_source(media, rp);
            return rp;
        },

        [=](data_source::use_data_atom,
            const utility::JsonStore &drag_drop_data,
            const utility::FrameRate &,
            const bool) -> utility::UuidActorVector {
            // drag/drop event has happened somewhere (playlists panel, timeline panel, media
            // list panel)
            return utility::UuidActorVector();
        },

        [=](data_source::use_data_atom atom,
            const caf::uri &uri) -> result<utility::UuidActorVector> {
            // Another drag/drop event, this time a URI has been dropped
            return utility::UuidActorVector();
        },

        [=](data_source::use_data_atom,
            const caf::uri &uri,
            const utility::FrameRate &media_rate,
            const bool create_playlist) -> result<utility::UuidActorVector> {
            // A drag/drop event into the playlist panel
            return utility::UuidActorVector();
        },

        // just use the default with jsonstore ?
        [=](data_source::use_data_atom,
            const utility::JsonStore &js) -> result<utility::JsonStore> {
            // json data drag/drop
            return utility::JsonStore();
        },

        [=](xstudio::broadcast::broadcast_down_atom, caf::actor &ui_data_model_actor) {

        },
        [=](new_database_model_instance_atom,
            caf::actor datamodel_ui_actor,
            const bool shot_tree_model) -> bool {
            // we get this message when a 'DataModel' of 'DemoPluginVersionsModel' class
            // is instanced in the xSTUDIO UI/QML engine. We want to send updates to the
            // DataModel/DemoPluginVersionsModel instances when our data set changes. We
            // therefore need to monitor the actor, so we know when it has been destroyed;
            if (shot_tree_model) {

                shot_tree_ui_model_actors_.insert(datamodel_ui_actor);

                // monitor allows us to run a lambda when some given actor that we are
                // interested in exits (is destroyed). In this case we simply remove the
                // reference to the 'datamodel_ui_actor' from our shot_tree_ui_model_actors_
                // list
                monitor(datamodel_ui_actor, [this, a = datamodel_ui_actor](const error &) {
                    auto it = shot_tree_ui_model_actors_.find(a);
                    if (it != shot_tree_ui_model_actors_.end()) {
                        shot_tree_ui_model_actors_.erase(it);
                    }
                });
                // this will tell the datamode_ui_actor to reset its entire model,
                // if we have a current project set
                if (current_project_->value() != "")
                    mail(database_model_reset_atom_v).send(datamodel_ui_actor);

            } else {

                version_list_ui_model_actors_.insert(datamodel_ui_actor);
                monitor(datamodel_ui_actor, [this, a = datamodel_ui_actor](const error &) {
                    auto it = version_list_ui_model_actors_.find(a);
                    if (it != version_list_ui_model_actors_.end()) {
                        version_list_ui_model_actors_.erase(it);
                    }
                });
                // this will tell the datamode_ui_actor to reset its entire model,
                // if we have a current project set
                if (versions_data_.size())
                    mail(database_model_reset_atom_v, versions_data_).send(datamodel_ui_actor);
            }

            return true;
        },
        [=](demo_plugin_custom_atom) {
            // this message is sent to us from the constructor. This is because
            // we can't run initialise_database in the constuctor as it causes
            // a deadclock in the 'global' actor which instances the plugins in
            // it's own thread and therefore can't respond to a request we
            // would make in our plugin class constructor.
            initialise_database();
        },

        [=](shot_tree_selection_atom, const std::vector<std::string> &selected_rows) {
            utility::JsonStore args(utility::JsonStore::parse("[]"));
            args.push_back(selected_rows);
            mail(
                embedded_python::python_exec_atom_v,
                "DemoPluginPython", // plugin name
                "select_versions",  // method on plugin
                args                // arguments for method
                )
                .request(
                    python_interpreter_,
                    infinite // timeout
                    )
                .then(
                    [=](const utility::JsonStore &result) {
                        versions_data_ = result;
                        for (auto &dmua : version_list_ui_model_actors_) {
                            mail(database_model_reset_atom_v, result).send(dmua);
                        }
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](database_row_count_atom, const std::string json_ptr) -> result<int> {
            /*try {

                const auto &v = data_set_.at(json::json_pointer(json_ptr + "/rows"));    //
            false return int(v.size());

            } catch (std::exception & e) {
                return make_error(xstudio_error::error, e.what());
            }*/
            return 0;
        },

        [=](database_record_from_uuid_atom,
            const utility::Uuid uuid) -> result<utility::JsonStore> {
            // We need to make an a-sync request to another actor before we
            // get a result. For this reason we need a caf::response_promise to
            // pass the result back to the actor that made this request
            auto rp = make_response_promise<utility::JsonStore>();

            utility::JsonStore args(utility::JsonStore::parse("[]"));
            args.push_back(uuid);
            mail(
                embedded_python::python_exec_atom_v,
                "DemoPluginPython",    // plugin name
                "get_version_by_uuid", // method on plugin
                args                   // arguments for method
                )
                .request(
                    python_interpreter_,
                    infinite // timeout
                    )
                .then(
                    [=](const utility::JsonStore &result) mutable { rp.deliver(result); },
                    [=](caf::error &err) mutable {
                        // error in python request - pass back to requester
                        rp.deliver(err);
                    });
            return rp;
        },

        [=](database_entry_atom,
            bool is_version_list,
            const std::string json_ptr,
            const DATA_MODEL_ROLE &role,
            int row,
            size_t index_id) {
            const auto role_database_key_lookup = data_model_role_names.find(role);
            if (role_database_key_lookup == data_model_role_names.end())
                return;

            // we can get the 'actor' that sent us this message (in this case and instance
            // of out DataModel class) thus:
            auto requester = caf::actor_cast<caf::actor>(current_sender());

            // see note above - currently we have to pack our own args to pass to
            // python. We're working on this inconvenience!
            utility::JsonStore args(utility::JsonStore::parse("[]"));
            args.push_back(is_version_list);
            args.push_back(json_ptr + "/" + role_database_key_lookup->second);

            mail(
                embedded_python::python_exec_atom_v,
                "DemoPluginPython", // plugin name
                "get_data",         // method on plugin
                args                // arguments for method
                )
                .request(
                    python_interpreter_,
                    infinite // timeout
                    )
                .then(
                    // Note - the lambda/function pointer immediately below is executed
                    // SOME TIME AFTER mail() is called and returned.
                    [=](const utility::JsonStore &result) {
                        // When executing a method on a python plugin the result
                        // is always encoded as json data
                        mail(database_entry_atom_v, result, role, row, index_id)
                            .send(requester);
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },

        [=](database_row_count_atom,
            bool is_version_list,
            const std::string json_ptr,
            int parent_row,
            size_t index_id) {
            auto requester = caf::actor_cast<caf::actor>(current_sender());

            // Currently awkard - we have to pack the args for the python method in our
            // plugin right here.
            utility::JsonStore args(utility::JsonStore::parse("[]"));
            args.push_back(is_version_list);
            args.push_back(json_ptr + "/rows");

            mail(
                embedded_python::python_exec_atom_v,
                "DemoPluginPython", // plugin name
                "get_row_count",    // method on plugin
                args                // arguments for py method
                )
                .request(
                    python_interpreter_,
                    infinite // timeout
                    )
                .then(
                    [=](const utility::JsonStore &result) {
                        mail(database_row_count_atom_v, result.get<int>(), parent_row, index_id)
                            .send(requester);
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },
        [=](set_database_value_atom,
            const utility::Uuid entry_id,
            const utility::JsonStore &value,
            const std::string &role_name
        ) {

            // Received from DemoPluginVersionsModel instance when 
            // the user wants to set a value in the database.

            utility::JsonStore args(utility::JsonStore::parse("[]"));
            args.push_back(to_string(entry_id));
            args.push_back(role_name);
            args.push_back(value);

            mail(
                embedded_python::python_exec_atom_v,
                "DemoPluginPython", // plugin name
                "set_version_data",    // method on plugin
                args                // arguments for py method
                )
                .request(
                    python_interpreter_,
                    infinite // timeout
                    )
                .then(
                    [=](const utility::JsonStore &result) {
                        // success. The plugin sends us a separate event message to 
                        // tell us if data has changed.
                    },
                    [=](caf::error &err) {
                        spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    });
        },
        [=](utility::event_atom,
            data_source::put_data_atom,
            const std::string &version_uuid, 
            const std::string &role_name,
            const utility::JsonStore &role_value) {
            // this event message comes from the python plugin to tell us that
            // a record in the versions table has changed
            // We simply forward on to any UI models exposing versions data
            for (auto &dmua : version_list_ui_model_actors_) {
                mail(data_source::put_data_atom_v, version_uuid, role_name, role_value).send(dmua);
            }

        }

    };
}

void DemoPlugin::attribute_changed(const utility::Uuid &attr_uuid, const int role) {

    if (attr_uuid == current_project_->uuid() && role == module::Attribute::Value) {

        // project has changed. Tell UI models to reset their models
        for (auto &dmua : shot_tree_ui_model_actors_) {
            mail(database_model_reset_atom_v).send(dmua);
        }
    }
    plugin::StandardPlugin::attribute_changed(attr_uuid, role);
}

void DemoPlugin::initialise_database() {

    // We are utilising an xSTUDIO python plugin in conjunction with this plugin.
    // The Python plugin will serve as our 'database' interface as well as
    // demoing other features of the xSTUDIO python API.

    // To initialise our studio integration plugin we need a list of the production
    // names from our database.

    // Because we are doing this in the constructor, and xSTUDIO might be loading
    // this plugin before the python interpreter has been created, we do an a-sycn
    // request which the global actor will only reply to when it's intialisation
    // is complete and all core components of xSTUDIO have been created.
    //
    // If we need the python interpreter later on we could do this instead:
    //
    // auto python_interp = system().registry().template
    // get<caf::actor>(embedded_python_registry);
    //

    auto global_actor = system().registry().template get<caf::actor>(global_registry);

    // ask global_actor for the python interpreter actor
    mail(global::get_python_atom_v)
        .request(
            global_actor,
            infinite // timeout
            )
        .then(
            [=](caf::actor python_interp) {

                python_interpreter_ = python_interp;

                // Now we can request the python interpreter actor to run a method
                // on our python plugin
                mail(
                    embedded_python::python_exec_atom_v,
                    "DemoPluginPython",        // plugin name
                    "get_list_of_productions", // method on plugin
                    utility::JsonStore()       // arguments for method
                    )
                    .request(
                        python_interpreter_,
                        infinite // timeout
                        )
                    .then(
                        [=](const utility::JsonStore &result) {
                            try {
                                if (result.is_array()) {
                                    std::vector<std::string> job_choices;
                                    for (int i = 0; i < result.size(); ++i) {
                                        job_choices.push_back(result[i].get<std::string>());
                                    }
                                    current_project_->set_role_data(
                                        module::Attribute::StringChoices, job_choices);
                                    if (!job_choices.empty()) {
                                        current_project_->set_value(job_choices[0]);
                                    }
                                }
                            } catch (std::exception &e) {
                                spdlog::critical("{} {}", __PRETTY_FUNCTION__, e.what());
                            }

                            // Now that the database is set-up, tell and UI instances that
                            // the database is ready for interaction - they will refresh
                            // their views of the database
                            for (auto &dmua : shot_tree_ui_model_actors_) {
                                mail(database_model_reset_atom_v).send(dmua);
                            }
                        },
                        [=](caf::error &err) {
                            spdlog::critical("{} {}", __PRETTY_FUNCTION__, to_string(err));
                        });
            },
            [=](caf::error &err) {
                spdlog::critical("{} {}", __PRETTY_FUNCTION__, to_string(err));
            });
}

XSTUDIO_PLUGIN_DECLARE_BEGIN()

XSTUDIO_REGISTER_PLUGIN(
    DemoPlugin,
    DemoPlugin::PLUGIN_UUID,
    DemoPlugin,
    plugin_manager::PluginFlags::PF_DATA_SOURCE,
    true,
    Ted Waine,
    Demo Plugin - Example plugin to show various xSTUDIO API features and
        reference implementation of core UI / backend interaction.,
    1.0.0)

XSTUDIO_REGISTER_MEDIA_READER_PLUGIN(
    ProceduralImageGenReader,
    ProceduralImageGenReader::PLUGIN_UUID,
    ProceduralImageGen,
    xStudio,
    Demo Media Reader(procedural image gen),
    1.0.0)

XSTUDIO_PLUGIN_DECLARE_END()