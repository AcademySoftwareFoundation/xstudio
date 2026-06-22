// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/json_store.hpp"

namespace xstudio {
namespace ui {
    namespace ui_layouts_model {

        class WindowsAndPanelsModel : public JsonStoreActor {

          public:
            WindowsAndPanelsModel(caf::actor_config &cfg, const utility::JsonStore &jsn);

            ~WindowsAndPanelsModel() override = default;

            const char *name() const override { return NAME.c_str(); }

          private:
            inline static const std::string NAME = "MainMenuModel";

            caf::behavior make_behavior() override {
                return behavior_.or_else(JsonStoreActor::make_behavior());
            }

          protected:
        };

        class GlobalFrontEndModelData : public caf::event_based_actor {

          public:
            GlobalFrontEndModelData(caf::actor_config &cfg);
            ~GlobalFrontEndModelData() override = default;

            caf::behavior make_behavior() override;

            const char *name() const override { return NAME.c_str(); }

            void on_exit() override;

          private:
            inline static const std::string NAME = "GlobalFrontEndModelData";
        };

    } // namespace playhead
} // namespace xstudio
