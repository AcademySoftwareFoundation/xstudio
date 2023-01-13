// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <caf/all.hpp>

#include "xstudio/module/typed_attributes.hpp"
#include "xstudio/ui/keyboard.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/atoms.hpp"
#include "xstudio/global_store/global_store.hpp"

namespace xstudio {

namespace module {

    typedef std::shared_ptr<const Attribute> ConstAttributePtr;
    typedef std::vector<std::shared_ptr<const Attribute>> AttributeSet;

    class Module {

      protected:
      public:
        Module(const std::string name);

        virtual ~Module();

        FloatAttribute *add_float_attribute(
            const std::string &title,
            const std::string &abbr_title    = "",
            const float value                = 0.0f,
            const float float_min            = std::numeric_limits<float>::lowest(),
            const float float_max            = std::numeric_limits<float>::max(),
            const float float_step           = 1e-3f,
            const int float_display_decimals = 2,
            const float fscrub_sensitivity =
                0.01f // 100 pixels scrub with mouse = value change of 1.0
        );

        StringChoiceAttribute *add_string_choice_attribute(
            const std::string &title,
            const std::string &abbr_title                = "",
            const std::string &value                     = "",
            const std::vector<std::string> &options      = {},
            const std::vector<std::string> &abbr_options = {});

        template <class T>
        StringChoiceAttribute *add_string_choice_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const std::vector<std::tuple<T, std::string, std::string, bool>>
                & // first ignored, second is choice name, third is abbr choice name, fourth is
                  // enabled
        );

        BooleanAttribute *add_boolean_attribute(
            const std::string &title, const std::string &abbr_title, const bool value);

        StringAttribute *add_string_attribute(
            const std::string &title, const std::string &abbr_title, const std::string &value);

        QmlCodeAttribute *
        add_qml_code_attribute(const std::string &name, const std::string &qml_code);

        IntegerAttribute *add_integer_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const int value,
            const int int_min = std::numeric_limits<int>::lowest(),
            const int int_max = std::numeric_limits<int>::max());

        ActionAttribute *
        add_action_attribute(const std::string &title, const std::string &abbr_title);

        ColourAttribute *add_colour_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const utility::ColourTriplet &value);

        Attribute *add_attribute(
            const std::string &title,
            const utility::JsonStore &value,
            const utility::JsonStore &role_data);

        [[nodiscard]] virtual AttributeSet
        full_module(const std::string &attr_group = "") const;

        [[nodiscard]] virtual AttributeSet menu_attrs(const std::string &root_menu_name) const;

        virtual void
        update_attr_from_preference(const std::string &path, const utility::JsonStore &change);

        virtual void update_attrs_from_preferences(const utility::JsonStore &);

        virtual bool remove_attribute(const utility::Uuid &attribute_uuid);

        [[nodiscard]] virtual utility::JsonStore serialise() const;

        [[nodiscard]] const std::string &name() const { return name_; }

        virtual void deserialise(const nlohmann::json &json);

        void set_parent_actor_addr(caf::actor_addr addr);

        void delete_attribute(const utility::Uuid &attribute_uuid);

        virtual caf::message_handler message_handler();

        virtual void notify_change(
            utility::Uuid attr_uuid,
            const int role,
            const utility::JsonStore &value,
            const bool redraw_viewport = false,
            const bool self_notify     = true);


        utility::Uuid register_hotkey(
            int default_keycode,
            int default_modifier,
            const std::string &hotkey_name,
            const std::string &description,
            const bool autorepeat        = false,
            const std::string &component = "MODULE_NAME",
            const std::string &context   = "any" // context "any" means hotkey will be activated
                                                 // regardless of which window it came from
        );

        void remove_hotkey(const utility::Uuid &hotkey_uuid);

        // re-implement to handle viewport hotkey events. context argument indicates where the
        // hotkey was pressed (e.g. 'main_viewport', 'popout_viewport' etc.)
        virtual void
        hotkey_pressed(const utility::Uuid & /*hotkey_uuid*/, const std::string & /*context*/) {
        }

        // re-implement to handle viewport hotkey release events
        virtual void hotkey_released(
            const utility::Uuid & /*hotkey_uuid*/, const std::string & /*context*/) {}

        // re-implement to handle viewport pointer events
        virtual bool pointer_event(const ui::PointerEvent &) { return false; }

        // re-implement to handle text entry from the keyboard
        virtual void
        text_entered(const std::string & /*text*/, const std::string & /*context*/) {}
        // re-implement to handle non-text keyboard event
        virtual void key_pressed(
            const int /*key*/, const std::string & /*context*/, const bool /*auto_repeat*/) {}

        caf::scoped_actor home_system() { return self()->home_system(); }

        caf::actor self() { return caf::actor_cast<caf::actor>(parent_actor_addr_); }

        // re-implement to receive callback when the on-screen media changes. To
        virtual void on_screen_media_changed(caf::actor media) {}


        // re-implement to receive callbacks when bookmarks in the current playhead
        // have changed
        virtual void bookmark_ranges_changed(
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                &bookmark_frame_ranges) {}

      protected:
        void redraw_viewport();

        // re-implement this function and use it to add custom hotkeys
        virtual void register_hotkeys() {}

        virtual void
        attribute_changed(const utility::Uuid & /*attr_uuid*/, const int /*role_id*/) {}

        Attribute *get_attribute(const utility::Uuid &attr_uuid);

        caf::actor_addr parent_actor_addr_;

        void connect_to_ui();
        void disconnect_from_ui();
        void grab_mouse_focus();
        void release_mouse_focus();
        void grab_keyboard_focus();
        void release_keyboard_focus();

        [[nodiscard]] bool connected_to_ui() const { return connected_to_ui_; }
        virtual void connected_to_ui_changed() {}

      private:
        caf::actor global_module_events_actor_;
        caf::actor keypress_monitor_actor_;
        caf::actor keyboard_and_mouse_group_;
        caf::actor ui_attribute_events_group_;
        caf::actor module_events_group_;
        caf::actor attribute_events_group_;


        std::vector<AttributePtr> attributes_;
        void notify_attribute_destroyed(Attribute *);
        bool connected_to_ui_      = {false};
        utility::Uuid module_uuid_ = {utility::Uuid::generate()};
        std::string name_;
        std::set<utility::Uuid> attrs_waiting_to_update_prefs_;

        std::vector<ui::Hotkey> unregistered_hotkeys_;
    };

    template <class T>
    StringChoiceAttribute *Module::add_string_choice_attribute(
        const std::string &title,
        const std::string &abbr_title,
        const std::vector<std::tuple<T, std::string, std::string, bool>>
            &v // first ignored, second is choice name, third is abbr choice name, fourth is
               // enabled
    ) {

        std::vector<std::string> names;
        std::vector<std::string> abbr_names;
        std::vector<bool> is_enabled;
        for (auto opt : v) {
            names.push_back(std::get<1>(opt));
            abbr_names.push_back(std::get<2>(opt));
            is_enabled.push_back(std::get<3>(opt));
        }

        auto *rt(new StringChoiceAttribute(
            title, abbr_title, names.empty() ? "" : names.front(), names, abbr_names));
        rt->set_role_data(Attribute::StringChoicesEnabled, is_enabled);
        rt->set_owner(this);
        attributes_.emplace_back(static_cast<Attribute *>(rt));
        return rt;
    }

} // namespace module
} // namespace xstudio
