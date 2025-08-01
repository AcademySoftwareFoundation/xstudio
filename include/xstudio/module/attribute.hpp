// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "xstudio/module/attribute_role_data.hpp"

namespace xstudio {

namespace module {

    class Module;

    class Attribute {

      public:
        enum ToolbarWidgetType {
            UndefinedAttribute,
            BooleanAttribute,
            FloatAttribute,
            ActionAttribute,
            StringAttribute,
            QmlCodeAttribute,
            IntegerAttribute,
            StringChoiceAttribute,
            ColourAttribute,
            JsonAttribute
        };

        inline static const std::map<int, std::string> type_names = {
            {StringChoiceAttribute, "ComboBox"},
            {BooleanAttribute, "OnOffToggle"},
            {StringAttribute, "LineEdit"},
            {QmlCodeAttribute, "QmlCode"},
            {IntegerAttribute, "IntegerValue"},
            {FloatAttribute, "FloatScrubber"},
            {ActionAttribute, "Action"},
            {ColourAttribute, "ColourAttribute"},
            {JsonAttribute, "JsonAttribute"}};

        enum Role {
            Type,
            Enabled,
            Activated,
            Title,
            AbbrTitle,
            StringChoices,
            AbbrStringChoices,
            StringChoicesEnabled,
            ToolTip,
            CustomMessage,
            IntegerMin,
            IntegerMax,
            FloatScrubMin,
            FloatScrubMax,
            FloatScrubStep,
            FloatScrubSensitivity,
            FloatDisplayDecimals,
            DisabledValue,
            Value,
            DefaultValue,
            AbbrValue,
            UuidRole,
            Groups,
            MenuPaths,
            ToolbarPosition,
            OverrideValue,
            SerializeKey,
            QmlCode,
            PreferencePath, // use this to set a pref path that means the attribute always
                            // tracks the preference
            InitOnlyPreferencePath, // use this to set a pref path that doesn't update the
                                    // attribute when the preference changes later on
            FontSize,
            FontFamily,
            TextAlignment,
            TextContainerBox,
            Colour,
            HotkeyUuid,
            UserData
        };

        inline static const std::map<int, std::string> role_names = {
            {Type, "type"},
            {Enabled, "attr_enabled"},
            {Activated, "activated"},
            {Title, "title"},
            {AbbrTitle, "abbr_title"},
            {StringChoices, "combo_box_options"},
            {AbbrStringChoices, "combo_box_abbr_options"},
            {StringChoicesEnabled, "combo_box_options_enabled"},
            {ToolTip, "tooltip"},
            {CustomMessage, "custom_message"},
            {IntegerMin, "integer_min"},
            {IntegerMax, "integer_max"},
            {FloatScrubMin, "float_scrub_min"},
            {FloatScrubMax, "float_scrub_max"},
            {FloatScrubStep, "float_scrub_step"},
            {FloatScrubSensitivity, "float_scrub_sensitivity"},
            {FloatDisplayDecimals, "float_display_decimals"},
            {Value, "value"},
            {DefaultValue, "default_value"},
            {AbbrValue, "short_value"},
            {DisabledValue, "disabled_value"},
            {UuidRole, "attr_uuid"},
            {Groups, "groups"},
            {MenuPaths, "menu_paths"},
            {ToolbarPosition, "toolbar_position"},
            {OverrideValue, "override_value"},
            {SerializeKey, "serialize_key"},
            {QmlCode, "qml_code"},
            {PreferencePath, "preference_path"},
            {InitOnlyPreferencePath, "init_only_preference_path"},
            {FontSize, "font_size"},
            {FontFamily, "font_family"},
            {TextAlignment, "text_alignment"},
            {TextContainerBox, "text_alignment_box"},
            {Colour, "attr_colour"},
            {HotkeyUuid, "hotkey_uuid"},
            {UserData, "user_data"}};

        ~Attribute() = default;

        void set_owner(Module *owner);

        void set_redraw_viewport_on_change(const bool redraw_viewport_on_change);

        template <typename T> [[nodiscard]] T get_role_data(const int role) const {
            const auto p = role_data_.find(role);
            if (p == role_data_.end()) {
                std::stringstream msg;
                msg << "Attribute " << get_role_data<std::string>(Title) << " of type "
                    << get_role_data<std::string>(Type) << " does not have role data for "
                    << role_name(role) << "\n";
                throw std::runtime_error(msg.str().c_str());
            }
            return p->second.get<T>();
        }

        [[nodiscard]] bool has_role_data(const int role) const {
            return role_data_.find(role) != role_data_.end();
        }

        [[nodiscard]] nlohmann::json role_data_as_json(const int role) const;

        [[nodiscard]] nlohmann::json as_json() const;

        void update_from_json(const nlohmann::json &data, const bool notify);

        void delete_role_data(const int role) {
            if (has_role_data(role)) {
                role_data_.erase(role_data_.find(role));
            }
        }

        void set_role_data(const int role, const char *data) {
            set_role_data(role, std::string(data));
        }

        template <typename T>
        void set_role_data(const int role, const T &data, const bool owner_notify = true) {
            if (role_data_[role].set(data)) {
                Attribute::notify_change(role, data, owner_notify);
            }
        }

        void set_preference_path(const std::string &preference_path);

        void expose_in_ui_attrs_group(const std::string &group_name, bool expose = true);

        void set_tool_tip(const std::string &tool_tip);

        bool operator==(const Attribute &o) const { return uuid() == o.uuid(); }

        bool operator==(const utility::Uuid &o_uuid) const { return uuid() == o_uuid; }

        [[nodiscard]] bool belongs_to_groups(const std::vector<std::string> &group_name) const;

        [[nodiscard]] utility::Uuid uuid() const {
            return get_role_data<utility::Uuid>(UuidRole);
        }

        static std::string role_name(const int role);
        static std::string type_name(const int tp);
        static int role_index(const std::string &role_name);

        Attribute(const Attribute &o) = default;

      protected:
        Attribute(
            const std::string &title,
            const std::string &abbr_title,
            const std::string &type_name);

        void notify_change(const int r, const nlohmann::json &data, const bool owner_notify);

        std::map<int, AttributeData> role_data_;

      private:
        Module *owner_;
        bool redraw_viewport_on_change_;
    };

    typedef std::unique_ptr<Attribute> AttributePtr;

} // namespace module
} // namespace xstudio