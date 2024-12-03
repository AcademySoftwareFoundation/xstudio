// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <queue>

#include "xstudio/shotgun_client/shotgun_client.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/thumbnail/thumbnail.hpp"

namespace xstudio {
namespace shotgun_client {
    class ShotgunClientActor : public caf::event_based_actor {
      public:
        ShotgunClientActor(caf::actor_config &cfg);
        ~ShotgunClientActor() override = default;

        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "ShotgunClientActor";
        void init();
        caf::behavior make_behavior() override { return behavior_; }

        template <typename T> void authenticate(T rp, std::function<void()> lambda) {
            if (not base_.authenticated()) {
                // spdlog::error("NOT AUTH REQUEST IT {}",
                // to_string(caf::actor_cast<caf::actor>(this)));

                request(actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                    .then(
                        [=](const std::pair<std::string, std::string> &) mutable { lambda(); },
                        [=](error &err) mutable {
                            spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                            rp.deliver(err);
                        });
            } else
                lambda();
        }

        template <typename T>
        bool check_failed_authenticate(
            const utility::JsonStore &jsn, T rp, std::function<void()> lambda) {
            const static auto path = nlohmann::json::json_pointer("/errors/0/status");
            auto result            = false;
            try {
                if (jsn.contains(path) and not jsn.at(path).is_null() and
                    jsn.at(path).get<int>() == 401) {
                    result = true;
                    // spdlog::error("FAILED REQUEST NOT AUTH");

                    // try and authorise..
                    request(
                        actor_cast<caf::actor>(this), infinite, shotgun_acquire_token_atom_v)
                        .then(
                            [=](const std::pair<std::string, std::string> &) mutable {
                                lambda();
                            },
                            [=](error &err) mutable {
                                spdlog::warn("{} {}", __PRETTY_FUNCTION__, to_string(err));
                                rp.deliver(utility::JsonStore(std::move(jsn)));
                            });
                }
            } catch (const std::exception &err) {
                spdlog::warn("{} {} {}", __PRETTY_FUNCTION__, err.what(), jsn.dump(2));
            }

            return result;
        }

        void acquire_token_primary(
            caf::typed_response_promise<std::pair<std::string, std::string>> rp);
        void acquire_token(caf::typed_response_promise<std::pair<std::string, std::string>> rp);

        void request_image(
            const std::string &entity,
            const int record_id,
            const bool thumbnail,
            caf::typed_response_promise<std::string> rp);

        void request_attachment(
            const std::string &entity,
            const int record_id,
            const std::string &property,
            caf::typed_response_promise<std::string> rp);

        void request_text_search(
            const std::string &text,
            const utility::JsonStore &conditions,
            const int page,
            const int page_size,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_schema_entity_fields(
            const std::string &entity,
            const std::string &field,
            const int id,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_entity_filter(
            const std::string &entity,
            const utility::JsonStore &filter,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_entity(
            const std::string &entity,
            const int record_id,
            const std::vector<std::string> &fields,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_entity_search(
            const std::string &entity,
            const utility::JsonStore &conditions,
            const std::vector<std::string> &fields,
            const std::vector<std::string> &sort,
            const int page,
            const int page_size,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_schema_entity(
            const std::string &entity,
            const int record_id,
            caf::typed_response_promise<utility::JsonStore> rp);

        void
        request_schema(const int record_id, caf::typed_response_promise<utility::JsonStore> rp);

        void request_link(
            const std::string &link, caf::typed_response_promise<utility::JsonStore> rp);

        void update_entity(
            const std::string &entity,
            const int record_id,
            const utility::JsonStore &body,
            const std::vector<std::string> &fields,
            caf::typed_response_promise<utility::JsonStore> rp);

        void delete_entity(
            const std::string &entity,
            const int record_id,
            caf::typed_response_promise<utility::JsonStore> rp);

        void create_entity(
            const std::string &entity,
            const utility::JsonStore &body,
            caf::typed_response_promise<utility::JsonStore> rp);

        void request_info(caf::typed_response_promise<utility::JsonStore> rp);

        void request_preference(caf::typed_response_promise<utility::JsonStore> rp);

        void upload(
            const std::string &entity,
            const int record_id,
            const std::string &field,
            const std::string &name,
            const std::vector<std::byte> &data,
            const std::string &content_type,
            caf::typed_response_promise<bool> rp);

        void upload_start(
            const std::string &entity,
            const int record_id,
            const std::string &field,
            const std::string &name,
            caf::typed_response_promise<utility::JsonStore> rp);

        void upload_transfer(
            const std::string &url,
            const std::string &content_type,
            const std::vector<std::byte> &data,
            caf::typed_response_promise<utility::JsonStore> rp);

        void upload_finish(
            const std::string &path,
            const utility::JsonStore &info,
            caf::typed_response_promise<utility::JsonStore> rp);


        void refresh_token(
            const bool force,
            caf::typed_response_promise<std::pair<std::string, std::string>> rp);

        void request_image_buffer(
            const std::string &entity,
            const int record_id,
            const bool thumbnail,
            const bool as_buffer,
            caf::typed_response_promise<thumbnail::ThumbnailBufferPtr> rp);

      private:
        std::queue<caf::typed_response_promise<std::pair<std::string, std::string>>>
            request_refresh_queue_;
        std::queue<caf::typed_response_promise<std::pair<std::string, std::string>>>
            request_acquire_queue_;

        caf::behavior behavior_;
        ShotgunClient base_;
        caf::actor_addr secret_source_;
        caf::actor http_;
        caf::actor event_group_;
    };
} // namespace shotgun_client
} // namespace xstudio
