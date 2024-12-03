// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/module/module.hpp"
#include "xstudio/utility/json_store_sync.hpp"

namespace xstudio::conform {
class ConformWorkerActor : public caf::event_based_actor {
  public:
    ConformWorkerActor(caf::actor_config &cfg);
    ~ConformWorkerActor() override = default;

    caf::behavior make_behavior() override { return behavior_; }
    const char *name() const override { return NAME.c_str(); }

  private:
    void process_request(
        caf::typed_response_promise<ConformReply> rp, const ConformRequest &request);

    void process_task_request(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        const ConformRequest &request);

    void get_media_sequences(
        caf::typed_response_promise<
            std::vector<std::optional<std::pair<std::string, caf::uri>>>> rp,
        const utility::UuidActorVector &media);

    void prepare_sequence(
        caf::typed_response_promise<bool> rp,
        const utility::UuidActor &timeline,
        const bool only_create_conform_track);

    void conform_to_media(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        const utility::JsonStore &conform_operations,
        const utility::UuidActor &playlist,
        const utility::UuidActor &container,
        const std::string &item_type,
        const utility::UuidActorVector &items,
        const utility::UuidVector &insert_before);

    void conform_tracks_to_sequence(
        caf::typed_response_promise<ConformReply> rp,
        const utility::UuidActor &source_playlist,
        const utility::UuidActor &source_timeline,
        const utility::UuidActorVector &tracks,
        const utility::UuidActor &target_playlist,
        const utility::UuidActor &target_timeline,
        const utility::UuidActor &conform_track);

    void conform_to_timeline(
        caf::typed_response_promise<ConformReply> rp,
        const utility::JsonStore &conform_operations,
        const utility::UuidActor &playlist,
        const utility::UuidActor &timeline,
        const utility::UuidActor &conform_track,
        const utility::UuidActorVector &media);

    void conform_step_get_playlist_json(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        ConformRequest conform_request);

    void conform_step_get_clip_json(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        ConformRequest &conform_request);

    void conform_step_get_clip_media(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        ConformRequest &conform_request);

    void conform_step_get_media_json(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        ConformRequest &conform_request);

    void conform_step_get_media_source(
        caf::typed_response_promise<ConformReply> rp,
        const std::string &conform_task,
        ConformRequest &conform_request);

    void conform_chain(
        caf::typed_response_promise<ConformReply> rp, ConformRequest &conform_request);


    void find_matched(
        caf::typed_response_promise<utility::UuidActorVector> rp,
        const std::string &key,
        const utility::UuidActor &clip,
        const utility::UuidActor &timeline);

    inline static const std::string NAME = "ConformWorkerActor";
    caf::behavior behavior_;
    std::vector<caf::actor> conformers_;
};

class ConformManagerActor : public caf::event_based_actor, public module::Module {
  public:
    ConformManagerActor(
        caf::actor_config &cfg, const utility::Uuid uuid = utility::Uuid::generate());
    ~ConformManagerActor() override = default;

    caf::behavior make_behavior() override {
        set_parent_actor_addr(actor_cast<caf::actor_addr>(this));
        return message_handler_extensions().or_else(
            message_handler_.or_else(module::Module::message_handler()));
    }

    void on_exit() override;
    const char *name() const override { return NAME.c_str(); }

  protected:
    caf::message_handler message_handler_;

    caf::message_handler message_handler_extensions();

  private:
    inline static const std::string NAME = "ConformManagerActor";
    utility::Uuid uuid_;
    caf::actor event_group_;
    caf::actor pool_;
    size_t worker_count_{5};

    // stores information on conforming actions.
    utility::JsonStoreSync data_;
    utility::Uuid data_uuid_{utility::Uuid::generate()};
};

} // namespace xstudio::conform
