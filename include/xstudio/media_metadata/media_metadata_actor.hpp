// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/media_metadata/media_metadata.hpp"
#include "xstudio/utility/uuid.hpp"

// media actor will be a collection of actors..
// it'll delegate to the correct endpoint..

// this needs to be plugin enbabled
// we also need a way of deciding which plugin should be used..
// make plugins register support
// extension / 16bytes ? buffer match / free function, NATIVE flag to notified prefered plugin
// for collision? use above to decide which plugin to use.. The worker actor should wrap the
// underlying class.

namespace xstudio {
namespace media_metadata {
    class MediaMetadataWorkerActor : public caf::event_based_actor {
      public:
        MediaMetadataWorkerActor(caf::actor_config &cfg);
        ~MediaMetadataWorkerActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }
        const char *name() const override { return NAME.c_str(); }

      private:
        inline static const std::string NAME = "MediaMetadataWorkerActor";
        caf::behavior behavior_;
        std::vector<caf::actor> plugins_;
        std::map<std::string, caf::actor> name_plugin_;
    };

    class GlobalMediaMetadataActor : public caf::event_based_actor {
      public:
        GlobalMediaMetadataActor(caf::actor_config &cfg);

        ~GlobalMediaMetadataActor() override = default;

        caf::behavior make_behavior() override { return behavior_; }

        const char *name() const override { return NAME.c_str(); }
        void on_exit() override;

      private:
        inline static const std::string NAME = "GlobalMediaMetadataActor";
        caf::behavior behavior_;
        utility::Uuid uuid_;
    };
} // namespace media_metadata
} // namespace xstudio
