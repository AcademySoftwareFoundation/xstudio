// SPDX-License-Identifier: Apache-2.0

#include "data_source_shotgun.hpp"

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/helpers.hpp"

using namespace xstudio::shotgun_client;
using namespace xstudio::utility;
using namespace xstudio;

void ShotgunDataSource::set_authentication_method(const std::string &value) {
    if (authentication_method_->value() != value)
        authentication_method_->set_value(value);
}
void ShotgunDataSource::set_client_id(const std::string &value) {
    if (client_id_->value() != value)
        client_id_->set_value(value);
}
void ShotgunDataSource::set_client_secret(const std::string &value) {
    if (client_secret_->value() != value)
        client_secret_->set_value(value);
}
void ShotgunDataSource::set_username(const std::string &value) {
    if (username_->value() != value)
        username_->set_value(value);
}
void ShotgunDataSource::set_password(const std::string &value) {
    if (password_->value() != value)
        password_->set_value(value);
}
void ShotgunDataSource::set_session_token(const std::string &value) {
    if (session_token_->value() != value)
        session_token_->set_value(value);
}
void ShotgunDataSource::set_authenticated(const bool value) {
    if (authenticated_->value() != value)
        authenticated_->set_value(value);
}
void ShotgunDataSource::set_timeout(const int value) {
    if (timeout_->value() != value)
        timeout_->set_value(value);
}

shotgun_client::AuthenticateShotgun ShotgunDataSource::get_authentication() const {
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

void ShotgunDataSource::add_attributes() {

    std::vector<std::string> auth_method_names = {
        "client_credentials", "password", "session_token"};

    module::QmlCodeAttribute *button = add_qml_code_attribute(
        "MyCode",
        R"(
import Shotgun 1.0
ShotgunButton {}
)");

    button->set_role_data(module::Attribute::ToolbarPosition, 1010.0);
    button->expose_in_ui_attrs_group("media_tools_buttons");


    authentication_method_ = add_string_choice_attribute(
        "authentication_method",
        "authentication_method",
        "password",
        auth_method_names,
        auth_method_names);

    playlist_notes_action_ =
        add_action_attribute("playlist_notes_to_shotgun", "playlist_notes_to_shotgun");
    selected_notes_action_ =
        add_action_attribute("selected_notes_to_shotgun", "selected_notes_to_shotgun");

    client_id_     = add_string_attribute("client_id", "client_id", "");
    client_secret_ = add_string_attribute("client_secret", "client_secret", "");
    username_      = add_string_attribute("username", "username", "");
    password_      = add_string_attribute("password", "password", "");
    session_token_ = add_string_attribute("session_token", "session_token", "");

    authenticated_ = add_boolean_attribute("authenticated", "authenticated", false);

    // should be int..
    timeout_ = add_float_attribute("timeout", "timeout", 120.0, 10.0, 600.0, 1.0, 0);


    // by setting static UUIDs on these module we only create them once in the UI
    playlist_notes_action_->set_role_data(
        module::Attribute::UuidRole, "92c780be-d0bc-462a-b09f-643e8986e2a1");
    playlist_notes_action_->set_role_data(
        module::Attribute::Title, "Publish Playlist Notes...");
    playlist_notes_action_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_menu"});
    playlist_notes_action_->set_role_data(
        module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|ShotGrid"}));

    selected_notes_action_->set_role_data(
        module::Attribute::UuidRole, "7583a4d0-35d8-4f00-bc32-ae8c2bddc30a");
    selected_notes_action_->set_role_data(
        module::Attribute::Title, "Publish Selected Notes...");
    selected_notes_action_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_menu"});
    selected_notes_action_->set_role_data(
        module::Attribute::MenuPaths, std::vector<std::string>({"publish_menu|ShotGrid"}));

    authentication_method_->set_role_data(
        module::Attribute::UuidRole, "ea7c47b8-a851-4f44-b9f1-3f5b38c11d96");
    client_id_->set_role_data(
        module::Attribute::UuidRole, "31925e29-674f-4f03-a861-502a2bc92f78");
    client_secret_->set_role_data(
        module::Attribute::UuidRole, "05d18793-ef4c-4753-8b55-1d98788eb727");
    username_->set_role_data(
        module::Attribute::UuidRole, "a012c508-a8a7-4438-97ff-05fc707331d0");
    password_->set_role_data(
        module::Attribute::UuidRole, "55982b32-3273-4f1c-8164-251d8af83365");
    session_token_->set_role_data(
        module::Attribute::UuidRole, "d6fac6a6-a6c9-4ac3-b961-499d9862a886");
    authenticated_->set_role_data(
        module::Attribute::UuidRole, "ce708287-222f-46b6-820c-f6dfda592ba9");
    timeout_->set_role_data(
        module::Attribute::UuidRole, "9947a178-b5bb-4370-905e-c6687b2d7f41");

    authentication_method_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    client_id_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    client_secret_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    username_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    password_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    session_token_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    authenticated_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});
    timeout_->set_role_data(
        module::Attribute::Groups, nlohmann::json{"shotgun_datasource_preference"});

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

void ShotgunDataSource::attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) {
    // pass upto actor..
    call_attribute_changed(attr_uuid);
}
