// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/actor_config.hpp>
#include <caf/behavior.hpp>
#include <caf/event_based_actor.hpp>

#include "xstudio/audio/audio_output_actor.hpp"
#include "xstudio/global/enums.hpp"
#include "xstudio/utility/exports.hpp"
#include "xstudio/utility/remote_session_file.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace global {

    class DLL_PUBLIC GlobalActor : public caf::event_based_actor {
      public:
        GlobalActor(
            caf::actor_config &cfg, const utility::JsonStore &prefs = utility::JsonStore());
        ~GlobalActor() override = default;
        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

        caf::behavior make_behavior() override { return behavior_; }

      private:
        int publish_port(
            const int minimum,
            const int maximum,
            const std::string &bind_address,
            caf::actor a);
        void init(const utility::JsonStore &prefs = utility::JsonStore());

        void connect_api(const caf::actor &embedded_python);
        void connect_sync_api();
        void disconnect_api(const caf::actor &embedded_python, const bool force = false);
        void disconnect_sync_api(const bool force = false);

        template <class AudioOutputDev>
        caf::actor spawn_audio_output_actor(const utility::JsonStore &prefs) {
            static_assert(
                std::is_base_of<audio::AudioOutputDevice, AudioOutputDev>::value,
                "Not derived from audio::AudioOutputDevice");
            return spawn<audio::AudioOutputActor>(
                std::shared_ptr<audio::AudioOutputDevice>(new AudioOutputDev(prefs)));
        }

        inline static const std::string NAME = "GlobalActor";
        caf::behavior behavior_;
        caf::actor studio_;
        caf::actor ui_studio_;
        caf::actor event_group_;

        bool python_enabled_;
        bool api_enabled_;
        int port_;
        int port_minimum_;
        int port_maximum_;
        std::string bind_address_;
        bool connected_;

        bool sync_api_enabled_;
        int sync_port_minimum_;
        int sync_port_;
        int sync_port_maximum_;
        std::string sync_bind_address_;
        bool sync_connected_;
        std::string remote_api_session_name_;
        std::string remote_sync_session_name_;
        utility::RemoteSessionManager rsm_;

        bool session_autosave_{false};
        caf::uri session_autosave_path_{};
        int session_autosave_interval_{300};
        size_t session_autosave_hash_{0};
        StatusType status_{StatusType::ST_NONE};
        std::set<caf::actor_addr> busy_;
    };
} // namespace global
} // namespace xstudio
