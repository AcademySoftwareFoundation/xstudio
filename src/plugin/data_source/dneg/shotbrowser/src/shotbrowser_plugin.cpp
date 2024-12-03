// SPDX-License-Identifier: Apache-2.0

#include "xstudio/global_store/global_store.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/playlist/playlist_actor.hpp"
#include "xstudio/history/history_actor.hpp"
#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/shotgun_client/shotgun_client_actor.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/notification_handler.hpp"

#include "shotbrowser_plugin.hpp"

using namespace xstudio;
using namespace xstudio::utility;
using namespace xstudio::shotgun_client;
using namespace xstudio::shotbrowser;
using namespace xstudio::global_store;
using namespace xstudio::data_source;

using namespace std::chrono_literals;

#define JOB_DISPATCH_DELAY std::chrono::milliseconds(10)

// Note the 'StandardPlugin' class is a Module AND a caf::event_based_actor.
// So we are a full actor we don't have to do that 'set_parent_actor_addr' call
ShotBrowser::ShotBrowser(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "ShotBrowser", init_settings) {

    // You might not want or need to use attributes in your plugin as you'll
    // be doing a QAbstractItemModel ... however, they can be useful for
    // creating menus and holding values with preferences etc

    add_attributes();

    bind_attribute_changed_callback(
        [this](auto &&PH1) { attribute_changed(std::forward<decltype(PH1)>(PH1)); });

    // print_on_exit(this, "MediaHookActor");
    secret_source_ = actor_cast<caf::actor_addr>(this);

    shotgun_ = spawn<ShotgunClientActor>();
    link_to(shotgun_);

    // we need to recieve authentication updates.
    join_event_group(this, shotgun_);

    history_uuid_ = Uuid::generate();
    history_      = spawn<history::HistoryMapActor<sys_time_point, JsonStore>>(history_uuid_);
    link_to(history_);

    // disable history until we init
    anon_send(history_, plugin_manager::enable_atom_v, false);

    event_group_ = spawn<broadcast::BroadcastActor>(this);
    link_to(event_group_);
    // we are the source of the secret..
    anon_send(shotgun_, shotgun_authentication_source_atom_v, actor_cast<caf::actor>(this));


    try {
        auto prefs = GlobalStoreHelper(system());
        JsonStore j;
        join_broadcast(this, prefs.get_group(j));
        update_preferences(j);
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }

    if (not disable_integration_)
        system().registry().put(
            shotbrowser_datasource_registry, caf::actor_cast<caf::actor>(this));

    pool_ = caf::actor_pool::make(
        system().dummy_execution_unit(),
        worker_count_,
        [&] { return system().template spawn<MediaWorker>(actor_cast<caf::actor_addr>(this)); },
        caf::actor_pool::round_robin());
    link_to(pool_);


    // set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
    // delayed_anon_send(
    //     caf::actor_cast<caf::actor>(this),
    //     std::chrono::milliseconds(500),
    //     module::connect_to_ui_atom_v);

    // set json store as origin
    // we rebroadcast all changes.
    engine().user_presets().set_origin(true);
    engine().user_presets().bind_send_event_func([&](auto &&PH1, auto &&PH2) {
        auto event     = JsonStore(std::forward<decltype(PH1)>(PH1));
        auto undo_redo = std::forward<decltype(PH2)>(PH2);

        // spdlog::warn("user_presets bind_send_event_func {} {}", event.dump(2), undo_redo);

        if (not undo_redo)
            anon_send(history_, history::log_atom_v, sysclock::now(), event);

        send(
            event_group_,
            utility::event_atom_v,
            json_store::sync_atom_v,
            engine().user_uuid(),
            event);
    });

    engine().system_presets().set_origin(true);
    engine().system_presets().bind_send_event_func([&](auto &&PH1, auto &&PH2) {
        auto event = JsonStore(std::forward<decltype(PH1)>(PH1));
        send(
            event_group_,
            utility::event_atom_v,
            json_store::sync_atom_v,
            engine().system_uuid(),
            event);
    });

    // this call is essential to set-up the base class
    make_behavior();

    // this call is required for attribute 'groups', menus etc. to be exposed
    // in the Qt/QML layer ... if you're not using attrs or backend menu
    // generation at all then you don't need this.
    connect_to_ui();

    // here's the code where we register our interface as an xSTUDIO 'panel'
    // that can be selected in the UI


    auto show_shotbrowser = register_hotkey(
        int('S'),
        ui::NoModifier,
        "Show ShotBrowser panel",
        "Shows or hides the pop-out Shotbrowser Plugin Panel");

    register_ui_panel_qml(
        "Shot Browser",
        R"(
            import ShotBrowser 1.0
            ShotBrowserRoot {
                anchors.fill: parent
            }
        )",
        9.1f, // position in panels drop-down menu
        "qrc:/icons/cloud-download.svg",
        5.0f,
        show_shotbrowser);

    register_ui_panel_qml(
        "Notes History",
        R"(
            import ShotBrowser 1.0
            NotesHistory {
                anchors.fill: parent
            }
        )",
        9.2f, // position in panels drop-down menu
        "qrc:/shotbrowser_icons/note_history.svg",
        6.0f);

    register_ui_panel_qml(
        "Shot History",
        R"(
            import ShotBrowser 1.0
            ShotHistory {
                anchors.fill: parent
            }
        )",
        9.3f, // position in panels drop-down menu
        "qrc:/shotbrowser_icons/shot_history.svg",
        7.0f);

    // this causes a divider to be inserted in the panels drop down menu
    register_ui_panel_qml("divider", "divider", 9.4f);

    // new method for instantiating a 'singleton' qml item which can
    // do a one-time insertion of menu items into any menu model
    register_singleton_qml(
        R"(
            import ShotBrowser 1.0
            ShotBrowserMediaListMenu {}
        )");
}

void ShotBrowser::on_exit() {
    // maybe on timer.. ?
    for (auto &i : waiting_)
        i.deliver(make_error(xstudio_error::error, "Password request cancelled."));
    waiting_.clear();
    system().registry().erase(shotbrowser_datasource_registry);
}


void ShotBrowser::set_authentication_method(const std::string &value) {
    if (authentication_method_->value() != value)
        authentication_method_->set_value(value);
}
void ShotBrowser::set_client_id(const std::string &value) {
    if (client_id_->value() != value)
        client_id_->set_value(value);
}
void ShotBrowser::set_client_secret(const std::string &value) {
    if (client_secret_->value() != value)
        client_secret_->set_value(value);
}
void ShotBrowser::set_username(const std::string &value) {
    if (username_->value() != value)
        username_->set_value(value);
}
void ShotBrowser::set_password(const std::string &value) {
    if (password_->value() != value)
        password_->set_value(value);
}
void ShotBrowser::set_session_token(const std::string &value) {
    if (session_token_->value() != value)
        session_token_->set_value(value);
}
void ShotBrowser::set_authenticated(const bool value) {
    if (authenticated_->value() != value)
        authenticated_->set_value(value);
}
void ShotBrowser::set_timeout(const int value) {
    if (timeout_->value() != value)
        timeout_->set_value(value);
}

shotgun_client::AuthenticateShotgun ShotBrowser::get_authentication() const {
    AuthenticateShotgun auth;

    auth.set_session_uuid(to_string(session_id_));

    auth.set_authentication_method(authentication_method_->value());
    switch (*(auth.authentication_method())) {
    case AM_SCRIPT:
        auth.set_client_id(client_id_->value());
        auth.set_client_secret(client_secret_->value());
        break;
    case AM_SESSION:
        auth.set_session_token(session_token_->value());
        break;
    case AM_LOGIN:
        auth.set_username(expand_envvars(username_->value()));
        auth.set_password(password_->value());
        break;
    case AM_UNDEFINED:
    default:
        break;
    }

    return auth;
}

void ShotBrowser::add_attributes() {

    std::vector<std::string> auth_method_names = {
        "client_credentials", "password", "session_token"};

    //     module::QmlCodeAttribute *button = add_qml_code_attribute(
    //         "MyCode",
    //         R"(
    // import Shotgun 1.0
    // ShotgunButton {}
    // )");

    //     button->set_role_data(module::Attribute::ToolbarPosition, 1010.0);
    //     button->expose_in_ui_attrs_group("media_tools_buttons");


    authentication_method_ = add_string_choice_attribute(
        "authentication_method",
        "authentication_method",
        "password",
        auth_method_names,
        auth_method_names);

    // playlist_notes_action_ =
    //     add_action_attribute("playlist_notes_to_shotgun", "playlist_notes_to_shotgun");
    // selected_notes_action_ =
    //     add_action_attribute("selected_notes_to_shotgun", "selected_notes_to_shotgun");

    client_id_     = add_string_attribute("client_id", "client_id", "");
    client_secret_ = add_string_attribute("client_secret", "client_secret", "");
    username_      = add_string_attribute("username", "username", "");
    password_      = add_string_attribute("password", "password", "");
    session_token_ = add_string_attribute("session_token", "session_token", "");
    authenticated_ = add_boolean_attribute("authenticated", "authenticated", false);
    // should be int..
    timeout_ = add_float_attribute("timeout", "timeout", 120.0, 10.0, 600.0, 1.0, 0);


    // by setting static UUIDs on these module we only create them once in the UI
    // playlist_notes_action_->set_role_data(
    //     module::Attribute::UuidRole, "92c780be-d0bc-462a-b09f-643e8986e2a1");
    // playlist_notes_action_->set_role_data(
    //     module::Attribute::Title, "Publish Playlist Notes...");
    // playlist_notes_action_->set_role_data(
    //     module::Attribute::UIDataModels, nlohmann::json{"shotgun_datasource_menu"});
    // playlist_notes_action_->set_role_data(
    //     module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|ShotGrid"}));

    // selected_notes_action_->set_role_data(
    //     module::Attribute::UuidRole, "7583a4d0-35d8-4f00-bc32-ae8c2bddc30a");
    // selected_notes_action_->set_role_data(
    //     module::Attribute::Title, "Publish Selected Notes...");
    // selected_notes_action_->set_role_data(
    //     module::Attribute::UIDataModels, nlohmann::json{"shotgun_datasource_menu"});
    // selected_notes_action_->set_role_data(
    //     module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|ShotGrid"}));

    authentication_method_->set_role_data(
        module::Attribute::UuidRole, "e512646d-08c0-4049-91fb-04dac38f75f2");
    client_id_->set_role_data(
        module::Attribute::UuidRole, "bb0a6e65-efd5-4115-89ca-b81652440b27");
    client_secret_->set_role_data(
        module::Attribute::UuidRole, "89b549af-c44d-48ee-9874-f628ce4c2112");
    username_->set_role_data(
        module::Attribute::UuidRole, "bad9c1a5-0f3f-4ccf-91e2-93817a04c6f1");
    password_->set_role_data(
        module::Attribute::UuidRole, "ebf90da7-b2bb-46a3-8a85-dbd28c454ea6");
    session_token_->set_role_data(
        module::Attribute::UuidRole, "c1f92a57-53c1-48f9-ac22-5387e06fee97");
    authenticated_->set_role_data(
        module::Attribute::UuidRole, "cf9ed35c-e82c-4665-bedd-7785a767cff6");
    timeout_->set_role_data(
        module::Attribute::UuidRole, "fff5d3e7-06c1-432b-b89d-f78f695f84e7");

    authentication_method_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    client_id_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    client_secret_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    username_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    password_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    session_token_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    authenticated_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});
    timeout_->set_role_data(
        module::Attribute::UIDataModels, nlohmann::json{"shotbrowser_datasource_preference"});

    authentication_method_->set_role_data(
        module::Attribute::ToolTip, "ShotGrid authentication method.");

    client_id_->set_role_data(module::Attribute::ToolTip, "ShotGrid script key.");
    client_secret_->set_role_data(module::Attribute::ToolTip, "ShotGrid script secret.");
    username_->set_role_data(module::Attribute::ToolTip, "ShotGrid username.");
    password_->set_role_data(module::Attribute::ToolTip, "ShotGrid password.");
    session_token_->set_role_data(module::Attribute::ToolTip, "ShotGrid session token.");
    authenticated_->set_role_data(module::Attribute::ToolTip, "Authenticated.");
    timeout_->set_role_data(module::Attribute::ToolTip, "ShotGrid server timeout.");
}

void ShotBrowser::attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) {
    // pass upto actor..
    call_attribute_changed(attr_uuid);
}


caf::message_handler ShotBrowser::message_handler_extensions() {

    // Here's where our own message handler is declared

    return caf::message_handler(
        {make_get_event_group_handler(event_group_),
         [=](utility::name_atom) -> std::string { return "ShotBrowser"; },
         [=](xstudio::broadcast::broadcast_down_atom, const caf::actor_addr &) {},

         [=](utility::event_atom,
             json_store::sync_atom,
             const Uuid &uuid,
             const JsonStore &event) {
             if (uuid == engine().user_uuid()) {
                 // spdlog::warn("user json_store::sync_atom {}", event.dump(2));
                 engine().user_presets().process_event(event, true, false, false);

                 // trigger sync to preferences
                 if (not pending_preference_update_) {
                     pending_preference_update_ = true;
                     delayed_anon_send(
                         actor_cast<caf::actor>(this),
                         std::chrono::seconds(5),
                         json_store::sync_atom_v,
                         true);
                 }
             } else if (uuid == engine().system_uuid()) {
                 // spdlog::warn("system json_store::sync_atom {}", event.dump(2));
                 engine().system_presets().process_event(event, true, false, false);
             }
         },

         [=](json_store::sync_atom, const Uuid &uuid) -> JsonStore {
             if (uuid == engine().user_uuid())
                 return engine().user_presets().as_json();
             else if (uuid == engine().system_uuid())
                 return engine().system_presets().as_json();

             return JsonStore();
         },

         [=](json_store::sync_atom, const bool) {
             pending_preference_update_ = false;
             auto prefs                 = GlobalStoreHelper(system());
             prefs.set_value(
                 engine().user_presets().as_json().at("children"),
                 "/plugin/data_source/shotbrowser/user_presets",
                 false);
             prefs.save("APPLICATION");
         },

         [=](json_store::sync_atom) -> UuidVector {
             return UuidVector({engine().user_uuid(), engine().system_uuid()});
         },

         [=](shotgun_projects_atom atom) { delegate(shotgun_, atom); },

         [=](shotgun_groups_atom atom, const int project_id) {
             delegate(shotgun_, atom, project_id);
         },

         [=](shotgun_schema_atom atom, const int project_id) {
             delegate(shotgun_, atom, project_id);
         },

         [=](shotgun_authentication_source_atom, caf::actor source) {
             secret_source_ = actor_cast<caf::actor_addr>(source);
         },

         [=](shotgun_authentication_source_atom) -> caf::actor {
             return actor_cast<caf::actor>(secret_source_);
         },

         [=](shotgun_update_entity_atom atom,
             const std::string &entity,
             const int record_id,
             const JsonStore &body) { delegate(shotgun_, atom, entity, record_id, body); },

         [=](shotgun_image_atom atom,
             const std::string &entity,
             const int record_id,
             const bool thumbnail) { delegate(shotgun_, atom, entity, record_id, thumbnail); },

         [=](shotgun_delete_entity_atom atom, const std::string &entity, const int record_id) {
             delegate(shotgun_, atom, entity, record_id);
         },

         [=](shotgun_image_atom atom,
             const std::string &entity,
             const int record_id,
             const bool thumbnail,
             const bool as_buffer) {
             delegate(shotgun_, atom, entity, record_id, thumbnail, as_buffer);
         },

         [=](shotgun_upload_atom atom,
             const std::string &entity,
             const int record_id,
             const std::string &field,
             const std::string &name,
             const std::vector<std::byte> &data,
             const std::string &content_type) {
             delegate(shotgun_, atom, entity, record_id, field, name, data, content_type);
         },

         // just use the default with jsonstore ?
         [=](put_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
             auto rp = make_response_promise<JsonStore>();
             put_action(rp, js);
             return rp;
         },

         [=](data_source::use_data_atom, const caf::actor &media, const FrameRate &media_rate)
             -> result<UuidActorVector> { return UuidActorVector(); },

         // no drop support..
         [=](data_source::use_data_atom, const JsonStore &, const FrameRate &, const bool)
             -> UuidActorVector { return UuidActorVector(); },

         // do we need the UI to have spun up before we can issue calls to shotgun...
         // erm...
         [=](use_data_atom atom, const caf::uri &uri) -> result<UuidActorVector> {
             auto rp = make_response_promise<UuidActorVector>();
             use_action(rp, uri, FrameRate(), true);
             return rp;
         },

         [=](use_data_atom,
             const caf::uri &uri,
             const FrameRate &media_rate,
             const bool create_playlist) -> result<UuidActorVector> {
             auto rp = make_response_promise<UuidActorVector>();
             use_action(rp, uri, media_rate, create_playlist);
             return rp;
         },

         [=](use_data_atom, const utility::JsonStore &js, const caf::actor &session)
             -> result<UuidActor> {
             auto rp = make_response_promise<UuidActor>();
             use_action(rp, js, session);
             return rp;
         },

         // just use the default with jsonstore ?
         [=](use_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
             auto rp = make_response_promise<JsonStore>();
             use_action(rp, js);
             return rp;
         },

         // just use the default with jsonstore ?

         [=](post_data_atom, const utility::JsonStore &js) -> result<JsonStore> {
             auto rp = make_response_promise<JsonStore>();
             post_action(rp, js);
             return rp;
         },

         [=](shotgun_entity_atom atom,
             const std::string &entity,
             const int record_id,
             const std::vector<std::string> &fields) {
             delegate(shotgun_, atom, entity, record_id, extend_fields(entity, fields));
         },

         [=](shotgun_entity_filter_atom atom,
             const std::string &entity,
             const JsonStore &filter,
             const std::vector<std::string> &fields,
             const std::vector<std::string> &sort) {
             delegate(shotgun_, atom, entity, filter, extend_fields(entity, fields), sort);
         },

         [=](shotgun_entity_filter_atom atom,
             const std::string &entity,
             const JsonStore &filter,
             const std::vector<std::string> &fields,
             const std::vector<std::string> &sort,
             const int page,
             const int page_size) {
             delegate(
                 shotgun_,
                 atom,
                 entity,
                 filter,
                 extend_fields(entity, fields),
                 sort,
                 page,
                 page_size);
         },

         [=](shotgun_schema_entity_fields_atom atom,
             const std::string &entity,
             const std::string &field,
             const int id) { delegate(shotgun_, atom, entity, field, id); },

         [=](shotgun_entity_search_atom atom,
             const std::string &entity,
             const JsonStore &conditions,
             const std::vector<std::string> &fields,
             const std::vector<std::string> &sort,
             const int page,
             const int page_size) {
             delegate(
                 shotgun_,
                 atom,
                 entity,
                 conditions,
                 extend_fields(entity, fields),
                 sort,
                 page,
                 page_size);
         },

         [=](shotgun_text_search_atom atom,
             const std::string &text,
             const JsonStore &conditions,
             const int page,
             const int page_size) {
             delegate(shotgun_, atom, text, conditions, page, page_size);
         },

         // can't reply via qt mixin.. this is a work around..
         [=](shotgun_acquire_authentication_atom, const bool cancelled) {
             if (cancelled) {
                 set_authenticated(false);
                 for (auto &i : waiting_)
                     i.deliver(
                         make_error(xstudio_error::error, "Authentication request cancelled."));
             } else {
                 auto auth = get_authentication();
                 if (waiting_.empty()) {
                     anon_send(shotgun_, shotgun_authenticate_atom_v, auth);
                 } else {
                     for (auto &i : waiting_)
                         i.deliver(auth);
                 }
             }
             waiting_.clear();
         },

         [=](shotgun_acquire_authentication_atom atom,
             const std::string &message) -> result<shotgun_client::AuthenticateShotgun> {
             if (secret_source_ == actor_cast<caf::actor_addr>(this))
                 return make_error(xstudio_error::error, "No authentication source.");

             auto rp = make_response_promise<shotgun_client::AuthenticateShotgun>();
             waiting_.push_back(rp);
             set_authenticated(false);
             anon_send(actor_cast<caf::actor>(secret_source_), atom, message);
             return rp;
         },

         [=](utility::event_atom,
             shotgun_acquire_token_atom,
             const std::pair<std::string, std::string> &tokens) {
             auto prefs = GlobalStoreHelper(system());
             prefs.set_value(
                 tokens.second,
                 "/plugin/data_source/shotbrowser/authentication/refresh_token",
                 false);
             prefs.save("APPLICATION");
             set_authenticated(true);
         },

         [=](playlist::add_media_atom,
             const utility::JsonStore &data,
             const utility::Uuid &playlist_uuid,
             const caf::actor &playlist,
             const utility::Uuid &before) -> result<std::vector<UuidActor>> {
             auto rp = make_response_promise<std::vector<UuidActor>>();
             add_media_to_playlist(
                 rp,
                 data,
                 playlist ? false : true,
                 playlist_uuid,
                 playlist,
                 before,
                 FrameRate());
             return rp;
         },

         [=](playlist::add_media_atom,
             const utility::JsonStore &data,
             const bool create_playlist,
             const FrameRate &media_rate) -> result<std::vector<UuidActor>> {
             auto rp = make_response_promise<std::vector<UuidActor>>();
             add_media_to_playlist(
                 rp, data, create_playlist, Uuid(), caf::actor(), Uuid(), media_rate);
             return rp;
         },

         [=](playlist::move_media_atom, caf::actor playlist, const JsonStore &sg_playlist)
             -> result<bool> {
             auto rp = make_response_promise<bool>();
             reorder_playlist(rp, playlist, sg_playlist);
             return rp;
         },

         [=](playlist::add_media_atom) {
             // this message handler is called in a loop until all build media
             // tasks in the queue are exhausted

             bool is_ivy_build_task;

             auto build_media_task_data = get_next_build_task(is_ivy_build_task);
             while (build_media_task_data) {

                 if (is_ivy_build_task) {

                     do_add_media_sources_from_ivy(build_media_task_data);

                 } else {

                     do_add_media_sources_from_shotgun(build_media_task_data);
                 }

                 // N.B. we only get a new build task if the number of incomplete tasks
                 // already dispatched is less than the number of actors in our
                 // worker pool
                 build_media_task_data = get_next_build_task(is_ivy_build_task);
             }
         },

         [=](get_data_atom, const utility::JsonStore &js) -> result<utility::JsonStore> {
             auto rp = make_response_promise<JsonStore>();
             get_action(rp, js);
             return rp;
         },

         [=](history::history_atom) -> UuidActor { return UuidActor(history_uuid_, history_); },

         // loop with timepoint
         [=](history::undo_atom, const sys_time_point &key) -> result<bool> {
             auto rp = make_response_promise<bool>();

             request(history_, infinite, history::undo_atom_v, key)
                 .then(
                     [=](const JsonStore &hist) mutable {
                         rp.delegate(
                             caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         // loop with timepoint
         [=](history::redo_atom, const sys_time_point &key) -> result<bool> {
             auto rp = make_response_promise<bool>();

             request(history_, infinite, history::redo_atom_v, key)
                 .then(
                     [=](const JsonStore &hist) mutable {
                         rp.delegate(
                             caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         [=](history::undo_atom, const utility::sys_time_duration &duration) -> result<bool> {
             auto rp = make_response_promise<bool>();
             request(history_, infinite, history::undo_atom_v, duration)
                 .then(
                     [=](const std::vector<JsonStore> &hist) mutable {
                         auto count = std::make_shared<size_t>(0);
                         for (const auto &h : hist) {
                             request(
                                 caf::actor_cast<caf::actor>(this),
                                 infinite,
                                 history::undo_atom_v,
                                 h)
                                 .then(
                                     [=](const bool) mutable {
                                         (*count)++;
                                         if (*count == hist.size())
                                             rp.deliver(true);
                                     },
                                     [=](const error &err) mutable {
                                         (*count)++;
                                         if (*count == hist.size())
                                             rp.deliver(true);
                                     });
                         }
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         [=](history::redo_atom, const utility::sys_time_duration &duration) -> result<bool> {
             auto rp = make_response_promise<bool>();
             request(history_, infinite, history::redo_atom_v, duration)
                 .then(
                     [=](const std::vector<JsonStore> &hist) mutable {
                         auto count = std::make_shared<size_t>(0);
                         for (const auto &h : hist) {
                             request(
                                 caf::actor_cast<caf::actor>(this),
                                 infinite,
                                 history::redo_atom_v,
                                 h)
                                 .then(
                                     [=](const bool) mutable {
                                         (*count)++;
                                         if (*count == hist.size())
                                             rp.deliver(true);
                                     },
                                     [=](const error &err) mutable {
                                         (*count)++;
                                         if (*count == hist.size())
                                             rp.deliver(true);
                                     });
                         }
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         [=](history::undo_atom) -> result<bool> {
             auto rp = make_response_promise<bool>();
             request(history_, infinite, history::undo_atom_v)
                 .then(
                     [=](const JsonStore &hist) mutable {
                         rp.delegate(
                             caf::actor_cast<caf::actor>(this), history::undo_atom_v, hist);
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         [=](history::redo_atom) -> result<bool> {
             auto rp = make_response_promise<bool>();
             request(history_, infinite, history::redo_atom_v)
                 .then(
                     [=](const JsonStore &hist) mutable {
                         rp.delegate(
                             caf::actor_cast<caf::actor>(this), history::redo_atom_v, hist);
                     },
                     [=](error &err) mutable { rp.deliver(std::move(err)); });
             return rp;
         },

         [=](history::undo_atom, const JsonStore &hist) -> result<bool> {
             auto rp = make_response_promise<bool>();

             // spdlog::warn("undo {}", hist.dump(2));

             // apply undo to state.
             // don't store state change in undo redo...

             engine().user_presets().process_event(hist, false, true, true);


             rp.deliver(true);
             // base_.item().undo(hist);

             // auto inverted = R"([])"_json;
             // for (const auto &i : hist) {
             //     auto ev    = R"({})"_json;
             //     ev["redo"] = i.at("undo");
             //     ev["undo"] = i.at("redo");
             //     inverted.emplace_back(ev);
             // }

             // send(event_group_, event_atom_v, item_atom_v, JsonStore(inverted), true);
             // if (not actors_.empty()) {
             //     // push to children..
             //     fan_out_request<policy::select_all>(
             //         map_value_to_vec(actors_), infinite, history::undo_atom_v, hist)
             //         .await(
             //             [=](std::vector<bool> updated) mutable {
             //                 anon_send(this, link_media_atom_v, media_actors_);
             //                 send(
             //                     event_group_,
             //                     event_atom_v,
             //                     item_atom_v,
             //                     JsonStore(inverted),
             //                     true);
             //                 rp.deliver(true);
             //             },
             //             [=](error &err) mutable { rp.deliver(std::move(err)); });
             // } else {
             //     send(event_group_, event_atom_v, item_atom_v, JsonStore(inverted), true);
             //     rp.deliver(true);
             // }
             return rp;
         },

         [=](history::redo_atom, const JsonStore &hist) -> result<bool> {
             auto rp = make_response_promise<bool>();
             // spdlog::warn("redo {}", hist.dump(2));


             engine().user_presets().process_event(hist, true, true, true);

             rp.deliver(true);

             // base_.item().redo(hist);

             // // send(event_group_, event_atom_v, item_atom_v, hist, true);

             // if (not actors_.empty()) {
             //     // push to children..
             //     fan_out_request<policy::select_all>(
             //         map_value_to_vec(actors_), infinite, history::redo_atom_v, hist)
             //         .await(
             //             [=](std::vector<bool> updated) mutable {
             //                 rp.deliver(true);
             //                 anon_send(this, link_media_atom_v, media_actors_);

             //                 send(event_group_, event_atom_v, item_atom_v, hist, true);
             //             },
             //             [=](error &err) mutable { rp.deliver(std::move(err)); });
             // } else {
             //     send(event_group_, event_atom_v, item_atom_v, hist, true);
             //     rp.deliver(true);
             // }

             return rp;
         },

         [=](json_store::update_atom,
             const JsonStore & /*change*/,
             const std::string & /*path*/,
             const JsonStore &full) {
             delegate(actor_cast<caf::actor>(this), json_store::update_atom_v, full);
         },

         [=](json_store::update_atom, const JsonStore &js) {
             try {
                 update_preferences(js);
             } catch (const std::exception &err) {
                 spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
             }
         }});
}


void ShotBrowser::attribute_changed(const utility::Uuid &attr_uuid) {
    // properties changed somewhere.
    // update loop ?
    if (attr_uuid == authentication_method_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            authentication_method_->value(),
            "/plugin/data_source/shotbrowser/authentication/grant_type");
    }
    if (attr_uuid == client_id_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            client_id_->value(), "/plugin/data_source/shotbrowser/authentication/client_id");
    }
    // if (attr_uuid == client_secret_->uuid()) {
    //     auto prefs = GlobalStoreHelper(system());
    //     prefs.set_value(client_secret_->value(),
    //     "/plugin/data_source/shotbrowser/authentication/client_secret");
    // }
    if (attr_uuid == timeout_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(timeout_->value(), "/plugin/data_source/shotbrowser/server/timeout");
    }

    if (attr_uuid == username_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            username_->value(), "/plugin/data_source/shotbrowser/authentication/username");
    }
    // if (attr_uuid == password_->uuid()) {
    //     auto prefs = GlobalStoreHelper(system());
    //     prefs.set_value(password_->value(),
    //     "/plugin/data_source/shotbrowser/authentication/password");
    // }
    if (attr_uuid == session_token_->uuid()) {
        auto prefs = GlobalStoreHelper(system());
        prefs.set_value(
            session_token_->value(),
            "/plugin/data_source/shotbrowser/authentication/session_token");
    }
}

void ShotBrowser::update_preferences(const JsonStore &js) {
    try {
        auto grant = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/grant_type");

        auto client_id = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/client_id");
        auto client_secret = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/client_secret");
        auto username = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/username");
        auto password = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/password");
        auto session_token = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/session_token");

        auto refresh_token = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/authentication/refresh_token");

        disable_integration_ =
            preference_value<bool>(js, "/plugin/data_source/shotbrowser/disable_integration");

        auto host =
            preference_value<std::string>(js, "/plugin/data_source/shotbrowser/server/host");
        auto port = preference_value<int>(js, "/plugin/data_source/shotbrowser/server/port");
        auto protocol = preference_value<std::string>(
            js, "/plugin/data_source/shotbrowser/server/protocol");
        auto timeout =
            preference_value<int>(js, "/plugin/data_source/shotbrowser/server/timeout");

        auto cache_dir = expand_envvars(
            preference_value<std::string>(js, "/plugin/data_source/shotbrowser/download/path"));
        auto cache_size =
            preference_value<size_t>(js, "/plugin/data_source/shotbrowser/download/size");

        download_cache_.prune_on_exit(true);
        download_cache_.target(cache_dir, true);
        download_cache_.max_size(cache_size * 1024 * 1024 * 1024);

        auto version_fields =
            preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/fields/version");
        auto note_fields =
            preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/fields/note");
        auto shot_fields =
            preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/fields/shot");
        auto playlist_fields =
            preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/fields/playlist");

        version_fields_.clear();
        note_fields_.clear();
        shot_fields_.clear();
        playlist_fields_.clear();

        version_fields_.insert(
            version_fields_.end(), version_fields.begin(), version_fields.end());
        note_fields_.insert(note_fields_.end(), note_fields.begin(), note_fields.end());
        shot_fields_.insert(shot_fields_.end(), shot_fields.begin(), shot_fields.end());
        playlist_fields_.insert(
            playlist_fields_.end(), playlist_fields.begin(), playlist_fields.end());

        auto category = preference_value<JsonStore>(js, "/core/bookmark/category");
        category_colours_.clear();
        if (category.is_array()) {
            for (const auto &i : category) {
                category_colours_[i.value("value", "default")] = i.value("colour", "");
            }
        }

        auto pipe_step =
            preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/browser/pipestep");

        if (auto ps = engine().get_cache(QueryEngine::cache_name("Pipeline Step")); not ps) {
            engine().set_cache(QueryEngine::cache_name("Pipeline Step"), pipe_step);
        }

        // no op ?
        set_authentication_method(grant);
        set_client_id(client_id);
        set_client_secret(client_secret);
        set_username(expand_envvars(username));
        set_password(password);
        set_session_token(session_token);
        set_timeout(timeout);

        // we ignore after initial setup..
        if (engine().user_presets().at("children").empty()) {
            auto project_presets = preference_value<JsonStore>(
                js, "/plugin/data_source/shotbrowser/project_presets");
            auto site_presets =
                preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/site_presets");
            auto user_presets =
                preference_value<JsonStore>(js, "/plugin/data_source/shotbrowser/user_presets");

            engine().merge_presets(site_presets, project_presets);
            engine().set_presets(user_presets, site_presets);

            // turn on undo redo
            anon_send(history_, plugin_manager::enable_atom_v, true);
        }

        // what hppens if we get a sequence of changes... should this be on a timed event ?
        // watch out for multiple instances.
        auto new_hash = std::hash<std::string>{}(
            grant + username + client_id + host + std::to_string(port) + protocol);

        if (new_hash != changed_hash_) {
            changed_hash_ = new_hash;
            // set server
            anon_send(
                shotgun_,
                shotgun_host_atom_v,
                std::string(fmt::format(
                    "{}://{}{}", protocol, host, (port ? ":" + std::to_string(port) : ""))));

            auto auth = get_authentication();
            if (not refresh_token.empty())
                auth.set_refresh_token(refresh_token);

            anon_send(shotgun_, shotgun_credential_atom_v, auth);
        }
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
    }
}

void ShotBrowser::refresh_playlist_versions(
    caf::typed_response_promise<JsonStore> rp,
    const utility::Uuid &playlist_uuid,
    const bool match_order) {
    // grab playlist id, get versions compare/load into playlist

    auto notification_uuid = Uuid();
    auto playlist          = caf::actor();

    auto failed = [=](const caf::actor &dest, const Uuid &uuid) mutable {
        if (dest and not uuid.is_null()) {
            auto notify = Notification::WarnNotification("Reloading Playlist Failed");
            notify.uuid(uuid);
            anon_send(dest, utility::notification_atom_v, notify);
        }
    };

    auto succeeded = [=](const caf::actor &dest, const Uuid &uuid) mutable {
        if (dest and not uuid.is_null()) {
            auto notify = Notification::InfoNotification(
                "Reloading Playlist Succeeded", std::chrono::seconds(5));
            notify.uuid(uuid);
            anon_send(dest, utility::notification_atom_v, notify);
        }
    };


    try {

        scoped_actor sys{system()};

        auto session = request_receive<caf::actor>(
            *sys,
            system().registry().template get<caf::actor>(global_registry),
            session::session_atom_v);

        playlist = request_receive<caf::actor>(
            *sys, session, session::get_playlist_atom_v, playlist_uuid);

        if (playlist) {
            auto notify       = Notification::ProcessingNotification("Reloading Playlist");
            notification_uuid = notify.uuid();
            anon_send(playlist, utility::notification_atom_v, notify);
        }

        auto plsg = request_receive<JsonStore>(
            *sys, playlist, json_store::get_json_atom_v, ShotgunMetadataPath + "/playlist");

        auto pl_id = plsg["id"].template get<int>();

        // this is a list of the media..
        auto media =
            request_receive<std::vector<UuidActor>>(*sys, playlist, playlist::get_media_atom_v);

        // foreach media actor get it's shotgun metadata.
        std::set<int> current_version_ids;

        for (const auto &i : media) {
            try {
                auto mjson = request_receive<JsonStore>(
                    *sys,
                    i.actor(),
                    json_store::get_json_atom_v,
                    utility::Uuid(),
                    ShotgunMetadataPath + "/version");
                current_version_ids.insert(mjson["id"].template get<int>());
            } catch (const std::exception &err) {
                // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            }
        }

        // we got media shotgun ids, plus playlist id
        // get current shotgun playlist/versions
        request(
            caf::actor_cast<caf::actor>(this),
            infinite,
            shotgun_entity_atom_v,
            "Playlists",
            pl_id,
            std::vector<std::string>())
            .then(
                [=](const JsonStore &result) mutable {
                    try {
                        scoped_actor sys{system()};
                        // update playlist
                        anon_send(
                            playlist,
                            json_store::set_json_atom_v,
                            JsonStore(result["data"]),
                            ShotgunMetadataPath + "/playlist");

                        //  gather versions, to get more detail..
                        std::vector<std::string> version_ids;
                        for (const auto &i :
                             result.at("data").at("relationships").at("versions").at("data")) {
                            if (not current_version_ids.count(i.at("id").template get<int>()))
                                version_ids.emplace_back(
                                    std::to_string(i.at("id").template get<int>()));
                        }

                        if (version_ids.empty()) {
                            if (match_order) {
                                // still need to match ordering..
                                // which is in another table :(
                                anon_send(
                                    caf::actor_cast<caf::actor>(this),
                                    playlist::move_media_atom_v,
                                    playlist,
                                    JsonStore(result["data"]));
                            }

                            succeeded(playlist, notification_uuid);
                            rp.deliver(result);
                            return;
                        }

                        auto query  = R"({})"_json;
                        query["id"] = join_as_string(version_ids, ",");

                        // get details..
                        request(
                            caf::actor_cast<caf::actor>(this),
                            infinite,
                            shotgun_entity_filter_atom_v,
                            "Versions",
                            JsonStore(query),
                            VersionFields,
                            std::vector<std::string>(),
                            1,
                            4999)
                            .then(
                                [=](const JsonStore &result2) mutable {
                                    try {
                                        // got version details.
                                        // we can now just call add versions to playlist..
                                        request(
                                            caf::actor_cast<caf::actor>(this),
                                            infinite,
                                            playlist::add_media_atom_v,
                                            result2,
                                            playlist_uuid,
                                            playlist,
                                            utility::Uuid())
                                            .then(
                                                [=](const std::vector<UuidActor>
                                                        &media) mutable {
                                                    if (match_order)
                                                        anon_send(
                                                            caf::actor_cast<caf::actor>(this),
                                                            playlist::move_media_atom_v,
                                                            playlist,
                                                            JsonStore(result["data"]));
                                                },
                                                [=](error &err) mutable {
                                                    spdlog::warn(
                                                        "{} {}",
                                                        __PRETTY_FUNCTION__,
                                                        to_string(err));
                                                });

                                        // return this as the result.
                                        rp.deliver(result);
                                        succeeded(playlist, notification_uuid);
                                    } catch (const std::exception &err) {
                                        failed(playlist, notification_uuid);
                                        rp.deliver(
                                            make_error(xstudio_error::error, err.what()));
                                    }
                                },

                                [=](error &err) mutable {
                                    failed(playlist, notification_uuid);
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                    rp.deliver(err);
                                });
                    } catch (const std::exception &err) {
                        failed(playlist, notification_uuid);
                        rp.deliver(make_error(xstudio_error::error, err.what()));
                    }
                },
                [=](error &err) mutable {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    failed(playlist, notification_uuid);
                    rp.deliver(err);
                });


    } catch (const std::exception &err) {
        failed(playlist, notification_uuid);
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::add_media_to_playlist(
    caf::typed_response_promise<UuidActorVector> rp,
    const utility::JsonStore &data,
    const bool create_playlist,
    utility::Uuid playlist_uuid,
    caf::actor playlist,
    const utility::Uuid &before,
    const utility::FrameRate &media_rate_) {
    // data can be in multiple forms..

    auto sys = caf::scoped_actor(system());

    nlohmann::json versions;

    try {
        versions = data.at("data").at("relationships").at("versions").at("data");
    } catch (...) {
        try {
            versions = data.at("data");
        } catch (...) {
            try {
                versions = data.at("result").at("data");
            } catch (...) {
                return rp.deliver(make_error(xstudio_error::error, "Invalid JSON"));
            }
        }
    }

    if (versions.empty())
        return rp.deliver(std::vector<UuidActor>());

    // auto event_msg = std::shared_ptr<event::Event>();


    // get uuid for playlist
    if (playlist and playlist_uuid.is_null()) {
        try {
            playlist_uuid =
                request_receive<utility::Uuid>(*sys, playlist, utility::uuid_atom_v);
        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            playlist      = caf::actor();
            playlist_uuid = Uuid();
        }
    }

    // get playlist for uuid
    if (not playlist and not playlist_uuid.is_null()) {
        try {
            auto session = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(global_registry),
                session::session_atom_v);

            playlist = request_receive<caf::actor>(
                *sys, session, session::get_playlist_atom_v, playlist_uuid);

        } catch (const std::exception &err) {
            spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
            playlist_uuid = utility::Uuid();
        }
    }

    // create playlist..
    if (create_playlist and not playlist and playlist_uuid.is_null()) {
        try {
            auto session = request_receive<caf::actor>(
                *sys,
                system().registry().template get<caf::actor>(global_registry),
                session::session_atom_v);

            auto tmp = request_receive<std::pair<utility::Uuid, UuidActor>>(
                *sys, session, session::add_playlist_atom_v, "ShotGrid Media");

            playlist_uuid = tmp.second.uuid();
            playlist      = tmp.second.actor();
        } catch (const std::exception &err) {
            spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
        }
    }

    // if (not playlist_uuid.is_null()) {
    //     event_msg = std::make_shared<event::Event>(
    //         "Loading ShotGrid Playlist Media {}",
    //         0,
    //         0,
    //         versions.size(), // we increment progress once per version loaded - ivy leafs are
    //                          // added after progress hits 100%
    //         std::set<utility::Uuid>({playlist_uuid}));
    //     event::send_event(this, *event_msg);
    // }

    try {
        auto media_rate = media_rate_;
        if (playlist)
            media_rate = request_receive<FrameRate>(*sys, playlist, session::media_rate_atom_v);

        std::string flag_text, flag_colour;
        if (data.contains(json::json_pointer("/context/flag_text")) and
            not data.at("context").value("flag_text", "").empty() and
            not data.at("context").value("flag_colour", "").empty()) {
            flag_colour = data.at("context").value("flag_colour", "");
            flag_text   = data.at("context").value("flag_text", "");
        }

        std::vector<std::string> visual_sources;
        if (data.contains(json::json_pointer("/context/visual_source"))) {
            auto tmp = data.at("context").value("visual_source", R"([])"_json);
            for (const auto &i : tmp)
                visual_sources.push_back(i);
        }

        std::vector<std::string> audio_sources;
        if (data.contains(json::json_pointer("/context/audio_source"))) {
            auto tmp = data.at("context").value("audio_source", R"([])"_json);
            for (const auto &i : tmp)
                audio_sources.push_back(i);
        }

        // we need to ensure that media are added to playlist IN ORDER - this
        // is a bit fiddly because media are created out of order by the worker
        // pool so we use this utility::UuidList to ensure that the playlist builds
        // with media in order
        auto ordered_uuids = std::make_shared<utility::UuidList>();
        auto result        = std::make_shared<UuidActorVector>();
        auto result_count  = std::make_shared<int>(0);

        // get a new media item created for each of the names in our list
        for (const auto &i : versions) {
            // create a task data item, with the raw shotgun data that
            // can be used to build the media sources for each media
            // item in the playlist
            ordered_uuids->push_back(utility::Uuid::generate());
            build_playlist_media_tasks_.emplace_back(std::make_shared<BuildPlaylistMediaJob>(
                playlist,
                ordered_uuids->back(),
                i.at("attributes").at("code").get<std::string>(), // name for the media
                JsonStore(i),
                media_rate,
                visual_sources,
                audio_sources,
                ordered_uuids,
                before,
                flag_colour,
                flag_text,
                rp,
                result,
                result_count));
        }

        // this call starts the work of building the media and consuming
        // the jobs in the 'build_playlist_media_tasks_' queue
        send(this, playlist::add_media_atom_v);

    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        // if (not playlist_uuid.is_null()) {
        //     event_msg->set_complete();
        //     event::send_event(this, *event_msg);
        // }
        rp.deliver(make_error(xstudio_error::error, err.what()));
    }
}

void ShotBrowser::load_playlist(
    caf::typed_response_promise<UuidActor> rp,
    const int playlist_id,
    const caf::actor &session) {

    // this is going to get nesty :()

    // get playlist from shotgun
    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        shotgun_entity_atom_v,
        "Playlists",
        playlist_id,
        std::vector<std::string>())
        .then(
            [=](JsonStore pljs) mutable {
                // got playlist.
                // we can create an new xstudio playlist actor at this point..
                auto playlist = UuidActor();
                try {
                    if (session) {
                        scoped_actor sys{system()};

                        auto tmp = request_receive<std::pair<utility::Uuid, UuidActor>>(
                            *sys,
                            session,
                            session::add_playlist_atom_v,
                            pljs["data"]["attributes"]["code"].get<std::string>(),
                            utility::Uuid(),
                            false);

                        playlist = tmp.second;

                    } else {
                        auto uuid = utility::Uuid::generate();
                        auto tmp  = spawn<playlist::PlaylistActor>(
                            pljs["data"]["attributes"]["code"].get<std::string>(), uuid);
                        playlist = UuidActor(uuid, tmp);
                    }

                    // place holder for shotgun decorators.
                    anon_send(
                        playlist.actor(),
                        json_store::set_json_atom_v,
                        JsonStore(),
                        "/metadata/shotgun");
                    // should really be driven from back end not UI..
                } catch (const std::exception &err) {
                    spdlog::error("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }

                // get version order
                auto order_filter = R"(
            {
                "logical_operator": "and",
                "conditions": [
                    ["playlist", "is", {"type":"Playlist", "id":0}]
                ]
            })"_json;

                order_filter["conditions"][0][2]["id"] = playlist_id;

                request(
                    caf::actor_cast<caf::actor>(this),
                    infinite,
                    shotgun_entity_search_atom_v,
                    "PlaylistVersionConnection",
                    JsonStore(order_filter),
                    std::vector<std::string>({"sg_sort_order", "version"}),
                    std::vector<std::string>({"sg_sort_order"}),
                    1,
                    4999)
                    .then(
                        [=](const JsonStore &order) mutable {
                            std::vector<std::string> version_ids;
                            for (const auto &i : order["data"])
                                version_ids.emplace_back(std::to_string(
                                    i["relationships"]["version"]["data"].at("id").get<int>()));

                            if (version_ids.empty())
                                return rp.deliver(
                                    make_error(xstudio_error::error, "No Versions found"));

                            // get versions
                            auto query  = R"({})"_json;
                            query["id"] = join_as_string(version_ids, ",");

                            // get versions ordered by playlist.
                            request(
                                caf::actor_cast<caf::actor>(this),
                                infinite,
                                shotgun_entity_filter_atom_v,
                                "Versions",
                                JsonStore(query),
                                VersionFields,
                                std::vector<std::string>(),
                                1,
                                4999)
                                .then(
                                    [=](JsonStore &js) mutable {
                                        // munge it..
                                        auto data = R"([])"_json;

                                        for (const auto &i : version_ids) {
                                            for (auto &j : js["data"]) {

                                                // spdlog::warn("{} {}",
                                                // std::to_string(j["id"].get<int>()), i);
                                                if (std::to_string(j["id"].get<int>()) == i) {
                                                    data.push_back(j);
                                                    break;
                                                }
                                            }
                                        }

                                        js["data"] = data;

                                        // add back in
                                        pljs["data"]["relationships"]["versions"] = js;

                                        // spdlog::warn("{}",pljs.dump(2));
                                        // now we have a playlist json struct with the versions
                                        // corrrecly ordered, set metadata on playlist..
                                        anon_send(
                                            playlist.actor(),
                                            json_store::set_json_atom_v,
                                            JsonStore(pljs["data"]),
                                            ShotgunMetadataPath + "/playlist");

                                        anon_send(
                                            playlist.actor(),
                                            json_store::set_json_atom_v,
                                            JsonStore(
                                                R"({"icon": "qrc:/shotbrowser_icons/shot_grid.svg", "tooltip": "ShotGrid Playlist"})"_json),
                                            "/ui/decorators/shotgrid");

                                        anon_send(
                                            caf::actor_cast<caf::actor>(this),
                                            playlist::add_media_atom_v,
                                            pljs,
                                            playlist.uuid(),
                                            playlist.actor(),
                                            utility::Uuid());

                                        rp.deliver(playlist);
                                    },
                                    [=](error &err) mutable {
                                        spdlog::error(
                                            "{} {}", __PRETTY_FUNCTION__, to_string(err));
                                        rp.deliver(
                                            make_error(xstudio_error::error, to_string(err)));
                                    });
                        },
                        [=](error &err) mutable {
                            spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(make_error(xstudio_error::error, to_string(err)));
                        });
            },
            [=](error &err) mutable {
                spdlog::error("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(make_error(xstudio_error::error, to_string(err)));
            });
}

std::shared_ptr<BuildPlaylistMediaJob>
ShotBrowser::get_next_build_task(bool &is_ivy_build_task) {

    std::shared_ptr<BuildPlaylistMediaJob> job_info;
    // if we already have popped N jobs off the queue that haven't completed
    // and N >= worker_count_ we don't pop a job off and instead return a null
    //
    if (build_tasks_in_flight_ < worker_count_) {
        if (!build_playlist_media_tasks_.empty()) {
            is_ivy_build_task = false;
            job_info          = build_playlist_media_tasks_.front();
            build_playlist_media_tasks_.pop_front();
        } else if (!extend_media_with_ivy_tasks_.empty()) {
            is_ivy_build_task = true;
            job_info          = extend_media_with_ivy_tasks_.front();
            extend_media_with_ivy_tasks_.pop_front();
        }
    }
    return job_info;
}

void ShotBrowser::do_add_media_sources_from_shotgun(
    std::shared_ptr<BuildPlaylistMediaJob> build_media_task_data) {

    // now 'build' the MediaActor via our worker pool to create
    // MediaSources and add them
    build_tasks_in_flight_++;

    // spawn a media actor
    build_media_task_data->media_actor_ = spawn<media::MediaActor>(
        build_media_task_data->media_name_,
        build_media_task_data->media_uuid_,
        UuidActorVector());

    auto ua =
        UuidActor(build_media_task_data->media_uuid_, build_media_task_data->media_actor_);

    // this is called when we get a result back - keeps track of the number
    // of jobs being processed and sends a message to self to continue working
    // through the queue
    auto continue_processing_job_queue = [=]() {
        build_tasks_in_flight_--;
        delayed_send(this, JOB_DISPATCH_DELAY, playlist::add_media_atom_v);
        // if (build_media_task_data->event_msg_) {
        //     build_media_task_data->event_msg_->increment_progress();
        //     event::send_event(this, *(build_media_task_data->event_msg_));
        // }
    };

    // now we get our worker pool to build media sources and add them to the
    // parent MediaActor using the shotgun query data
    request(
        pool_,
        caf::infinite,
        playlist::add_media_atom_v,
        build_media_task_data->media_actor_,
        build_media_task_data->sg_data_,
        build_media_task_data->media_rate_)
        .then(

            [=](bool) {
                // media sources were constructed successfully - now we can add to
                // the playlist, we pass in the overall ordered list of uuids that
                // we are building so the playlist can ensure everything is added
                // in order, even if they aren't created in the correct order
                if (build_media_task_data->playlist_actor_) {
                    request(
                        build_media_task_data->playlist_actor_,
                        caf::infinite,
                        playlist::add_media_atom_v,
                        ua,
                        *(build_media_task_data->ordererd_uuids_),
                        build_media_task_data->before_)
                        .then(

                            [=](const UuidActor &) {
                                if (!build_media_task_data->flag_colour_.empty()) {
                                    anon_send(
                                        build_media_task_data->media_actor_,
                                        playlist::reflag_container_atom_v,
                                        std::make_tuple(
                                            std::optional<std::string>(
                                                build_media_task_data->flag_colour_),
                                            std::optional<std::string>(
                                                build_media_task_data->flag_text_)));
                                }

                                extend_media_with_ivy_tasks_.emplace_back(
                                    build_media_task_data);
                                continue_processing_job_queue();
                            },
                            [=](const caf::error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                continue_processing_job_queue();
                            });
                } else {
                    // not adding to playlist..
                    if (!build_media_task_data->flag_colour_.empty()) {
                        anon_send(
                            build_media_task_data->media_actor_,
                            playlist::reflag_container_atom_v,
                            std::make_tuple(
                                std::optional<std::string>(build_media_task_data->flag_colour_),
                                std::optional<std::string>(build_media_task_data->flag_text_)));
                    }

                    extend_media_with_ivy_tasks_.emplace_back(build_media_task_data);
                    continue_processing_job_queue();
                }
            },
            [=](const caf::error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                continue_processing_job_queue();
            });
}

void ShotBrowser::do_add_media_sources_from_ivy(
    std::shared_ptr<BuildPlaylistMediaJob> ivy_media_task_data) {

    auto ivy = system().registry().template get<caf::actor>("IVYDATASOURCE");
    build_tasks_in_flight_++;

    // this is called when we get a result back - keeps track of the number
    // of jobs being processed and sends a message to self to continue working
    // through the queue
    auto continue_processing_job_queue = [=]() {
        build_tasks_in_flight_--;
        delayed_send(this, JOB_DISPATCH_DELAY, playlist::add_media_atom_v);
        /* Commented out bevause we're not including ivy leaf addition
        in progress indicator now.
        if (ivy_media_task_data->event_msg) {
            ivy_media_task_data->event_msg->increment_progress();
            event::send_event(this, *(ivy_media_task_data->event_msg));
        }*/
    };


    auto good_sources = std::make_shared<utility::UuidActorVector>();
    auto count        = std::make_shared<int>(0);

    // this function adds the sources that are 'good' (i.e. were able
    // to acquire MediaDetail) to the MediaActor - we only call it
    // when we've fully 'built' each MediaSourceActor in our 'sources'
    // list -0 see the request/then handler below where it is used
    auto finalise = [=]() {
        request(
            ivy_media_task_data->media_actor_,
            infinite,
            media::add_media_source_atom_v,
            *good_sources)
            .then(
                [=](const bool) {
                    // media sources all in media actor.
                    // we can now select the ones we want..
                    //
                    // get list of valid sources for media..
                    request(
                        ivy_media_task_data->media_actor_,
                        infinite,
                        media::get_media_source_names_atom_v,
                        media::MT_IMAGE)
                        .then(
                            [=](const std::vector<std::pair<utility::Uuid, std::string>>
                                    &names) {
                                auto name = std::string();

                                for (const auto &pref :
                                     ivy_media_task_data->preferred_visual_sources_) {
                                    for (const auto &i : names) {
                                        if (i.second == pref) {
                                            name = pref;
                                            break;
                                        }
                                    }
                                    if (not name.empty())
                                        break;
                                }

                                if (name.empty()) {
                                    for (const auto &i : names) {
                                        if (i.second == "movie_dneg") {
                                            name = i.second;
                                            break;
                                        }
                                    }
                                }

                                if (name.empty()) {
                                    for (const auto &i : names) {
                                        if (i.second == "SG Movie") {
                                            name = i.second;
                                            break;
                                        }
                                    }
                                }

                                if (not name.empty())
                                    anon_send(
                                        ivy_media_task_data->media_actor_,
                                        playhead::media_source_atom_v,
                                        name,
                                        media::MT_IMAGE,
                                        true);
                            },
                            [=](error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });

                    request(
                        ivy_media_task_data->media_actor_,
                        infinite,
                        media::get_media_source_names_atom_v,
                        media::MT_AUDIO)
                        .then(
                            [=](const std::vector<std::pair<utility::Uuid, std::string>>
                                    &names) {
                                auto name = std::string();

                                for (const auto &pref :
                                     ivy_media_task_data->preferred_audio_sources_) {
                                    for (const auto &i : names) {
                                        if (i.second == pref) {
                                            name = pref;
                                            break;
                                        }
                                    }
                                    if (not name.empty())
                                        break;
                                }

                                if (name.empty()) {
                                    for (const auto &i : names) {
                                        if (i.second == "movie_dneg") {
                                            name = i.second;
                                            break;
                                        }
                                    }
                                }

                                if (name.empty()) {
                                    for (const auto &i : names) {
                                        if (i.second == "SG Movie") {
                                            name = i.second;
                                            break;
                                        }
                                    }
                                }

                                if (not name.empty())
                                    anon_send(
                                        ivy_media_task_data->media_actor_,
                                        playhead::media_source_atom_v,
                                        name,
                                        media::MT_AUDIO,
                                        true);
                            },
                            [=](error &err) {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            });
                    continue_processing_job_queue();
                },
                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    continue_processing_job_queue();
                });
    };

    // here we get the ivy data source to fetch sources (ivy leafs) using the
    // ivy dnuuid for the MediaActor already created from shotgun data
    try {
        request(
            ivy,
            infinite,
            use_data_atom_v,
            ivy_media_task_data->sg_data_.at("attributes")
                .at("sg_project_name")
                .get<std::string>(),
            utility::Uuid(ivy_media_task_data->sg_data_.at("attributes")
                              .at("sg_ivy_dnuuid")
                              .get<std::string>()),
            ivy_media_task_data->media_rate_)
            .then(
                [=](const utility::UuidActorVector &sources) {
                    // we want to make sure the 'MediaDetail' has been fetched on the
                    // sources before adding to the parent MediaActor - this means we
                    // don't build up a massive queue of IO heavy MediaDetail fetches
                    // but instead deal with them sequentially as each media item is
                    // added to the playlist

                    if (sources.empty()) {
                        finalise();
                    } else {
                        *count = sources.size();
                    }

                    for (auto source : sources) {

                        // we need to get each source to get its detail to ensure that
                        // it is readable/valid
                        request(
                            source.actor(),
                            infinite,
                            media::acquire_media_detail_atom_v,
                            ivy_media_task_data->media_rate_)
                            .then(
                                [=](bool got_media_detail) mutable {
                                    if (got_media_detail)
                                        good_sources->push_back(source);
                                    else
                                        send_exit(
                                            source.actor(), caf::exit_reason::user_shutdown);

                                    (*count)--;
                                    if (!(*count))
                                        finalise();
                                },
                                [=](error &err) {
                                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));

                                    // kill bad source.
                                    send_exit(source.actor(), caf::exit_reason::user_shutdown);

                                    (*count)--;
                                    if (!(*count))
                                        finalise();
                                });
                    }
                },

                [=](error &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                    continue_processing_job_queue();
                });
    } catch (const std::exception &err) {
        spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
        continue_processing_job_queue();
    }
}

std::vector<std::string> ShotBrowser::extend_fields(
    const std::string &entity, const std::vector<std::string> &fields) const {

    if (entity == "Notes" and fields == NoteFields)
        return concatenate_vector(fields, note_fields_);
    else if (entity == "Versions" and fields == VersionFields)
        return concatenate_vector(fields, version_fields_);
    else if (entity == "Shots" and fields == ShotFields)
        return concatenate_vector(fields, shot_fields_);
    else if (entity == "Playlists" and fields == PlaylistFields)
        return concatenate_vector(fields, playlist_fields_);

    return fields;
}

void ShotBrowser::reorder_playlist(
    caf::typed_response_promise<bool> rp,
    const caf::actor &playlist,
    const utility::JsonStore &sg_playlist) {
    // get version order
    const static auto id_jpointer =
        nlohmann::json::json_pointer("/relationships/version/data/id");

    auto order_filter = R"(
    {
        "logical_operator": "and",
        "conditions": [
            ["playlist", "is", {"type":"Playlist", "id":0}]
        ]
    })"_json;

    order_filter["conditions"][0][2]["id"] = sg_playlist.at("id");

    request(
        caf::actor_cast<caf::actor>(this),
        infinite,
        shotgun_entity_search_atom_v,
        "PlaylistVersionConnection",
        JsonStore(order_filter),
        std::vector<std::string>({"sg_sort_order", "version"}),
        std::vector<std::string>({"sg_sort_order"}),
        1,
        4999)
        .then(
            [=](const JsonStore &order) mutable {
                try {
                    auto version_ids = std::vector<int>();

                    for (const auto &i : order.at("data"))
                        version_ids.push_back(i.at(id_jpointer).get<int>());

                    // get media from playlist..
                    scoped_actor sys{system()};
                    auto media = request_receive<std::vector<UuidActor>>(
                        *sys, playlist, playlist::get_media_atom_v);

                    auto media_id     = std::map<int, Uuid>();
                    auto unused_media = std::list<Uuid>();

                    // get media current version id..
                    for (const auto &i : media) {
                        unused_media.push_back(i.uuid());
                        try {
                            auto mvid = request_receive<JsonStore>(
                                *sys,
                                i.actor(),
                                json_store::get_json_atom_v,
                                utility::Uuid(),
                                ShotgunMetadataPath + "/version/id");
                            media_id[mvid.get<int>()] = i.uuid();
                        } catch (const std::exception &) {
                            // spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                        }
                    }

                    // build list of media uuids, ordered by version_ids.
                    auto new_media_order = std::vector<Uuid>();

                    for (const auto &i : version_ids)
                        if (media_id.count(i)) {
                            new_media_order.push_back(media_id.at(i));
                            unused_media.erase(std::find(
                                unused_media.begin(), unused_media.end(), media_id.at(i)));
                        }

                    new_media_order.insert(
                        new_media_order.end(), unused_media.begin(), unused_media.end());

                    // update playlist.
                    request(
                        playlist,
                        infinite,
                        playlist::move_media_atom_v,
                        new_media_order,
                        Uuid())
                        .then(
                            [=](const bool) mutable { rp.deliver(true); },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(err);
                            });

                } catch (const std::exception &err) {
                    spdlog::warn("{} {}", __PRETTY_FUNCTION__, err.what());
                    rp.deliver(make_error(xstudio_error::error, err.what()));
                }
            },
            [=](error &err) mutable {
                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                rp.deliver(err);
            });
}


extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<ShotBrowser>>(
                ShotBrowser::PLUGIN_UUID,
                "ShotBrowser",
                plugin_manager::PluginFlags::PF_DATA_SOURCE,
                true, // resident
                "Al Crate",
                "DNEG Shotgrid & Ivy Integration Plugin")}));
}
}