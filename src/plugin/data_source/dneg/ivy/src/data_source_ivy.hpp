// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#ifndef CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

#include "xstudio/data_source/data_source.hpp"
#include "xstudio/utility/json_store.hpp"
#include "xstudio/module/module.hpp"

using namespace xstudio;
using namespace xstudio::data_source;

const std::string ivy_registry{"IVYDATASOURCE"};

class IvyDataSource : public DataSource, public module::Module {
  public:
    IvyDataSource();
    ~IvyDataSource() override = default;

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

    std::string url() const { return "http://pipequery"; }
    std::string path() const { return "/v1/graphql"; }
    std::string show() const { return show_; }
    std::string content_type() const { return "application/graphql"; }
    httplib::Headers get_headers() const;

  private:
    std::string host_;
    std::string user_;
    std::string show_;
    std::string site_;
    std::string app_version_;
    std::string app_name_;
    std::string billing_code_;
};

template <typename T> class IvyDataSourceActor : public caf::event_based_actor {
  public:
    IvyDataSourceActor(
        caf::actor_config &cfg, const utility::JsonStore & = utility::JsonStore());

    caf::behavior make_behavior() override {
        return data_source_.message_handler().or_else(behavior_);
    }
    void on_exit() override;

  private:
    void ivy_load(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const caf::uri &uri,
        const utility::FrameRate &media_rate);

    void ivy_load_version(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const caf::uri &uri,
        const utility::FrameRate &media_rate);

    void ivy_load_file(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const caf::uri &uri,
        const utility::FrameRate &media_rate);

    void ivy_load_version_sources(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const std::string &show,
        const utility::Uuid &stalk_dnuuid,
        const utility::FrameRate &media_rate);

    void ivy_load_audio_sources(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const std::string &show,
        const utility::Uuid &stem_dnuuid,
        const utility::FrameRate &media_rate,
        const utility::UuidActorVector &extend = utility::UuidActorVector());

    void get_show_stalk_uuid(
        caf::typed_response_promise<std::pair<utility::Uuid, std::string>> rp,
        const caf::actor &media);

    void handle_drop(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const utility::JsonStore &jsn,
        const utility::FrameRate &media_rate);

    void update_preferences(const utility::JsonStore &js);

    caf::behavior behavior_;
    T data_source_;
    caf::actor http_;
    caf::actor pool_;
    bool enable_audio_autoload_{true};
    utility::Uuid uuid_ = {utility::Uuid::generate()};
};
