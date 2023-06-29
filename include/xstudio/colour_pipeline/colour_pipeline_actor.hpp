// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <caf/policy/select_all.hpp>

#include "colour_pipeline.hpp"
#include "xstudio/broadcast/broadcast_actor.hpp"
#include "xstudio/media/media.hpp"
#include "xstudio/media_reader/pixel_info.hpp"
#include "xstudio/plugin_manager/plugin_factory.hpp"
#include "xstudio/plugin_manager/plugin_manager.hpp"
#include "xstudio/utility/chrono.hpp"
#include "xstudio/utility/helpers.hpp"

namespace xstudio {
namespace colour_pipeline {

    // TODO: This should be pulled from the preferences
    const int colour_pipeline_worker_count{4};

    class GlobalColourPipelineActor : public caf::event_based_actor, public module::Module {
      public:
        GlobalColourPipelineActor(caf::actor_config &cfg);
        ~GlobalColourPipelineActor() override = default;

        caf::behavior make_behavior() override;

        const char *name() const override { return NAME.c_str(); }

        void on_exit() override;

      private:
        void load_colour_pipe_details();

        void make_colour_pipeline(
            const std::string &pipe_name,
            const utility::JsonStore &jsn,
            caf::typed_response_promise<caf::actor> &rp);

        caf::actor
        make_colour_pipeline(const std::string &pipe_name, const utility::JsonStore &jsn);

      private:
        inline static const std::string NAME                = "GlobalColourPipelineActor";
        inline static const std::string BUILTIN_PLUGIN_NAME = "OCIOColourPipeline";

        std::vector<plugin_manager::PluginDetail> colour_pipe_plugin_details_;
        std::string default_plugin_name_;
        utility::JsonStore prefs_jsn_;
        caf::actor viewport0_colour_pipeline_;
    };


} // namespace colour_pipeline
} // namespace xstudio
