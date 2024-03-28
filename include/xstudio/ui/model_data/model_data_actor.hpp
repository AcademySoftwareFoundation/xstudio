// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>
#include <chrono>

// #include "xstudio/colour_pipeline/colour_pipeline.hpp"
#include "xstudio/utility/uuid.hpp"
#include "xstudio/utility/tree.hpp"

namespace xstudio {
namespace ui {
    namespace model_data {

        /*
        This class maintains central 'model data' in a flexible json dictionary
        format. Why central? Because we can have multiple UI elements that
        provide separate views to the same bits of data. If the underlying data
        is changed we want the change to be reflected in all views. Examples
        could include the viewport toolbar or pop-up menus.

        The model data can be exposed in the UI as QAbstractItemModel, allowing
        for data in a tree structure to be iterated over in the UI. For example,
        menus, sub-menus, sub-sub-menus going to multiple levels can be
        maintained this way. A backend actor can add aslo data to the model or
        change data causing a refresh to the UI or add new menus at run time.

        Model data can also be retrieved and written to preferences files. In
        this way we model the layout of the UI panels and maintain it in the
        user's preferences.
        */
        class GlobalUIModelData : public caf::event_based_actor {

          public:
            GlobalUIModelData(caf::actor_config &cfg);
            ~GlobalUIModelData() override = default;

            caf::behavior make_behavior() override { return behavior_; }

            const char *name() const override { return NAME.c_str(); }

            void on_exit() override;

          private:
            inline static const std::string NAME = "GlobalUIModelData";

            utility::JsonStore model_data_as_json(const std::string &model_name) const;

            void set_data(
                const std::string &model_name,
                const std::string path,
                const utility::JsonStore &data,
                const std::string &role = std::string());

            void set_data(
                const std::string &model_name,
                const utility::Uuid &item_uuid,
                const std::string &role,
                const utility::JsonStore &data,
                caf::actor setter);

            void insert_attribute_data_into_model(
                const std::string &model_name,
                const utility::Uuid &attribute_uuid,
                const utility::JsonStore &attribute_data,
                const std::string &sort_role,
                caf::actor client);

            void remove_attribute_data_from_model(
                const std::string &model_name,
                const utility::Uuid &attribute_uuid,
                caf::actor client);

            void register_model(
                const std::string &model_name,
                const utility::JsonStore &model_data,
                caf::actor client,
                const std::string &preference_path = std::string());

            void check_model_is_registered(
                const std::string &model_name, const bool auto_register = false);

            void insert_rows(
                const std::string &model_name,
                const std::string &path,
                const int row,
                int count,
                const utility::JsonStore &data,
                caf::actor requester = caf::actor());

            void remove_rows(
                const std::string &model_name,
                const std::string &path,
                const int row,
                int count,
                caf::actor requester = caf::actor());

            void push_to_prefs(const std::string &model_name, const bool actually_push = false);

            void remove_attribute_from_model(const std::string &model_name, const utility::Uuid &attr_uuid);

            void node_activated(const std::string &model_name, const std::string &path);

            void insert_into_menu_model(
                const std::string &model_name,
                const std::string &menu_path,
                const utility::JsonStore &menu_data,
                caf::actor watcher);

            void remove_node(const std::string &model_name, const utility::Uuid &model_item_id);

            void broadcast_whole_model_data(const std::string &model_name);

            struct ModelData {
                ModelData()                   = default;
                ModelData(const ModelData &o) = default;
                ModelData(
                    const std::string name,
                    const utility::JsonStore &data,
                    caf::actor client                  = caf::actor(),
                    const std::string &preference_path = std::string())
                    : name_(name),
                      data_(utility::json_to_tree(data, "children")),
                      preference_path_(preference_path) {
                    clients_.push_back(client);
                }
                std::string name_;
                utility::JsonTree data_;
                std::string preference_path_;
                std::vector<caf::actor> clients_;
                std::map<utility::Uuid, std::vector<caf::actor>> menu_watchers_;
                bool pending_prefs_update_ = {false};
            };

            typedef std::shared_ptr<ModelData> ModelDataPtr;

            std::map<std::string, ModelDataPtr> models_;
            std::set<std::string> models_to_be_fully_broadcasted_;

            caf::behavior behavior_;
        };
    } // namespace model_data
} // namespace ui
} // namespace xstudio
