// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/plugin_manager/plugin_base.hpp"
#include "demo_plugin_atoms.hpp"

namespace xstudio {
namespace demo_plugin {

    class DemoPlugin : public plugin::StandardPlugin {
      public:
        // Provide a mapping from DATA_MODEL_ROLE to strings. This is needed to define
        // the names of the 'roleData' elements that our QML Model will expose
        inline static const std::map<DATA_MODEL_ROLE, std::string> data_model_role_names = {
            {JOB, "job"},
            {SEQUENCE, "sequence"},
            {SHOT, "shot"},
            {VERSION_TYPE, "version_type"},
            {VERSION_STREAM, "version_stream"},
            {VERSION_NAME, "version_name"},
            {VERSION, "version"},
            {ARTIST, "artist"},
            {STATUS, "status"},
            {IS_ASSET, "asset"},
            {EXPANDED, "expanded"},
            {COMP_RANGE, "comp_range"},
            {FRAME_RANGE, "frame_range"},
            {MEDIA_PATH, "media_path"},
            {UUID, "uuid"}};

        // Our plugin requires its own static UUID.
        inline static const utility::Uuid PLUGIN_UUID =
            utility::Uuid("28813519-aa6e-42a5-a201-a55f07136565");

        // Argument types for the constructor must follow this pattern
        DemoPlugin(caf::actor_config &cfg, const utility::JsonStore &init_settings);
        virtual ~DemoPlugin() = default;

        // We declare our own message handlers for custom messaging between
        // the UI/plugin/backend components
        caf::message_handler message_handler_extensions() override;

        // Overriding this method gives us a direct was to receive data from the QML layer
        utility::JsonStore qml_item_callback(
            const utility::Uuid &qml_item_id, const utility::JsonStore &callback_data) override;

      protected:
        // Overriding this method provides a callback whenever our plugin attributes
        // change (attributes can drive UI elements, and attribute data can be changed
        // from the UI)
        void attribute_changed(const utility::Uuid &attr_uuid, const int role) override;

      private:
        void add_proxy_media_source(
            caf::actor media, caf::typed_response_promise<utility::UuidActorVector> rp);

        void initialise_database();

        module::StringChoiceAttribute *current_project_;
        std::set<caf::actor> shot_tree_ui_model_actors_;
        std::set<caf::actor> version_list_ui_model_actors_;
        caf::actor python_interpreter_;
        utility::JsonStore versions_data_;
    };

} // namespace demo_plugin
} // namespace xstudio