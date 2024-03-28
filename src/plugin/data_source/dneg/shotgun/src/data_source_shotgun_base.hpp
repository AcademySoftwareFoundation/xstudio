// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/data_source/data_source.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/module/module.hpp"

using namespace xstudio;
using namespace xstudio::data_source;

namespace xstudio::shotgun_client {
class AuthenticateShotgun;
}

class ShotgunDataSource : public DataSource, public module::Module {
  public:
    ShotgunDataSource() : DataSource("Shotgun"), module::Module("ShotgunDataSource") {
        add_attributes();
    }
    ~ShotgunDataSource() override = default;

    // handled directly in actor.
    utility::JsonStore get_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore put_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore post_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }
    utility::JsonStore use_data(const utility::JsonStore &) override {
        return utility::JsonStore();
    }

    void set_authentication_method(const std::string &value);
    void set_client_id(const std::string &value);
    void set_client_secret(const std::string &value);
    void set_username(const std::string &value);
    void set_password(const std::string &value);
    void set_session_token(const std::string &value);
    void set_authenticated(const bool value);
    void set_timeout(const int value);

    utility::Uuid session_id_;

    module::StringChoiceAttribute *authentication_method_;
    module::StringAttribute *client_id_;
    module::StringAttribute *client_secret_;
    module::StringAttribute *username_;
    module::StringAttribute *password_;
    module::StringAttribute *session_token_;
    module::BooleanAttribute *authenticated_;
    module::FloatAttribute *timeout_;

    module::ActionAttribute *playlist_notes_action_;
    module::ActionAttribute *selected_notes_action_;

    shotgun_client::AuthenticateShotgun get_authentication() const;

    void
    bind_attribute_changed_callback(std::function<void(const utility::Uuid &attr_uuid)> fn) {
        attribute_changed_callback_ = [fn](auto &&PH1) {
            return fn(std::forward<decltype(PH1)>(PH1));
        };
    }
    using module::Module::connect_to_ui;

  protected:
    // void hotkey_pressed(const utility::Uuid &hotkey_uuid, const std::string &context)
    // override;

    void attribute_changed(const utility::Uuid &attr_uuid, const int /*role*/) override;


    void call_attribute_changed(const utility::Uuid &attr_uuid) {
        if (attribute_changed_callback_)
            attribute_changed_callback_(attr_uuid);
    }


  private:
    std::function<void(const utility::Uuid &attr_uuid)> attribute_changed_callback_;

    void add_attributes();
};
