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
        Module(const std::string name, const utility::Uuid &uuid = utility::Uuid::generate());

        virtual ~Module();

        void parent_actor_exiting();

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

        IntegerVecAttribute *
        add_int_vec_attribute(const std::string &title, const std::string &abbr_title = "");

        JsonAttribute *add_json_attribute(
            const std::string &title,
            const std::string &abbr_title = "",
            const nlohmann::json &value   = {});

        BooleanAttribute *add_boolean_attribute(
            const std::string &title, const std::string &abbr_title, const bool value);

        StringAttribute *add_string_attribute(
            const std::string &title, const std::string &abbr_title, const std::string &value);

        QmlCodeAttribute *
        add_qml_code_attribute(const std::string &name, const std::string &qml_code);

        IntegerAttribute *add_integer_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const int64_t value,
            const int64_t int_min = std::numeric_limits<int64_t>::lowest(),
            const int64_t int_max = std::numeric_limits<int64_t>::max());

        ActionAttribute *
        add_action_attribute(const std::string &title, const std::string &abbr_title);

        ColourAttribute *add_colour_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const utility::ColourTriplet &value);

        Vec4fAttribute *add_vec4f_attribute(
            const std::string &title, const std::string &abbr_title, const Imath::V4f &value);

        Vec4fAttribute *add_vec4f_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const Imath::V4f &value,
            const Imath::V4f &min,
            const Imath::V4f &max,
            const Imath::V4f &step);

        FloatVectorAttribute *add_float_vector_attribute(
            const std::string &title,
            const std::string &abbr_title,
            const std::vector<float> &value,
            const std::vector<float> &min,
            const std::vector<float> &max,
            const std::vector<float> &step);

        Attribute *add_attribute(
            const std::string &title,
            const utility::JsonStore &value     = utility::JsonStore(),
            const utility::JsonStore &role_data = utility::JsonStore());

        [[nodiscard]] virtual AttributeSet full_module(
            const std::vector<std::string> &attr_groups = std::vector<std::string>()) const;

        [[nodiscard]] virtual AttributeSet menu_attrs(const std::string &root_menu_name) const;

        virtual void
        update_attr_from_preference(const std::string &path, const utility::JsonStore &change);

        virtual void update_attrs_from_preferences(const utility::JsonStore &);

        virtual void remove_attribute(const utility::Uuid &attribute_uuid);

        [[nodiscard]] virtual utility::JsonStore serialise() const;

        [[nodiscard]] const std::string &name() const { return name_; }

        [[nodiscard]] const utility::Uuid &uuid() const { return module_uuid_; }

        virtual void deserialise(const nlohmann::json &json);

        void set_parent_actor_addr(caf::actor_addr addr);

        void delete_attribute(const utility::Uuid &attribute_uuid);

        /* You can call this function with the actor that is attached to another module.
        Then any local attributes registered with the 'link_attribute' will be
        synced with the other module - changes that happen on the Module will be
        reflected on the other Module and vice-versa. The mechanism relies on
        the Title role data of the attributes to determine which attrs should
        be synced together, so your paired Modules must have attrs with the
        same Title data. More than one attr with the same title will cause issues.*/
        void link_to_module(
            caf::actor other_module,
            bool link_all_attrs,
            const bool both_ways,
            const bool initial_push_sync);

        void unlink_module(caf::actor other_module);

        /* If this Module instance is linked to another Module instance, only
        attributes that have been registered with this function will be synced
        up between this module and the linked module(s). */
        void link_attribute(const utility::Uuid &uuid) { linked_attrs_.insert(uuid); }

        void unlink_attribte(const utility::Uuid &uuid) {
            auto p = linked_attrs_.find(uuid);
            if (p != linked_attrs_.end()) {
                linked_attrs_.erase(p);
            }
        }

        virtual caf::message_handler message_handler();

        virtual void notify_change(
            utility::Uuid attr_uuid,
            const int role,
            const utility::JsonStore &value,
            const bool redraw_viewport = false,
            const bool self_notify     = true);

        utility::Uuid register_hotkey(
            const std::string sequence,
            const std::string &hotkey_name,
            const std::string &description,
            const bool autorepeat        = false,
            const std::string &component = "MODULE_NAME");

        utility::Uuid register_hotkey(
            int default_keycode,
            int default_modifier,
            const std::string &hotkey_name,
            const std::string &description,
            const bool autorepeat        = false,
            const std::string &component = "MODULE_NAME");

        void remove_hotkey(const utility::Uuid &hotkey_uuid);

        // re-implement to handle viewport hotkey events. context argument indicates where the
        // hotkey was pressed (e.g. 'main_viewport', 'popout_viewport' etc.)
        virtual void hotkey_pressed(
            const utility::Uuid & /*hotkey_uuid*/,
            const std::string & /*context*/,
            const std::string & /*window*/) {}

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

        caf::actor self() const { return caf::actor_cast<caf::actor>(parent_actor_addr_); }

        // re-implement to receive callback when the on-screen media changes. To
        virtual void on_screen_media_changed(caf::actor media) {}

        // re-implement to receive callback when the on-screen media changes.
        virtual void on_screen_media_changed(caf::actor media, caf::actor media_source) {}

        // re-implement to receive callbacks when bookmarks in the current playhead
        // have changed
        virtual void bookmark_ranges_changed(
            const std::vector<std::tuple<utility::Uuid, std::string, int, int>>
                &bookmark_frame_ranges) {}

        // re-implement to execute custom code when your module connects to a viewport.
        // For example, exposing certain attributes in a particular named group
        // of attributes for the UI layer (see Playhead.cpp)
        virtual void connect_to_viewport(
            const std::string &viewport_name,
            const std::string &viewport_toolbar_name,
            bool connect,
            caf::actor viewport);

        // re-implement to execute useful state reset for your module when the
        // user hits the 'reset-viewport' hotkey or button. For example, a
        // colour pipeline plugin might reset exposure / gamma values in this
        // function.
        virtual void reset() {}

      protected:
        /* Call this method with your StringChoiceAttribute to expose it in
        one of xSTUDIO's UI menus. The menu_path argument dictates which parent
        menu and submenu(s) within the parent menu it will be placed - each
        level of menu should be separated with a pipe character. For example, to
        put a multi choice in the viewport context menu that lists the choices
        under display, top_level_menu will be 'Viewport0Popup' and menu_path
        would be Display. To put the choices under the 'Colour' menu under an
        OCIO sub-menu, top_level_menu="Colour", menu_path="OCIO|Display".

        Valid values for top_level_menu:
            (File, Edit, Playlists, Media, Timeline, Playback,
            Viewer, Layout, Panels, Publish, Colour, Help,
            MediaListPopup, Viewport{X}PopUp )
        */
        void add_multichoice_attr_to_menu(
            StringChoiceAttribute *attr,
            const std::string top_level_menu,
            const std::string menu_path,
            const std::string before = std::string{});

        void add_boolean_attr_to_menu(
            BooleanAttribute *attr,
            const std::string top_level_menu,
            const std::string before = std::string{});

        utility::Uuid insert_menu_item(
            const std::string &menu_model_name,
            const std::string &menu_text,
            const std::string &menu_path,
            const float menu_item_position,
            Attribute *attr              = nullptr,
            const bool divider           = false,
            const utility::Uuid &hotkey  = utility::Uuid(),
            const std::string &user_data = std::string());

        utility::Uuid insert_hotkey_into_menu(
            const std::string &menu_model_name,
            const std::string &menu_path,
            const float menu_item_position,
            const utility::Uuid &hotkey);

        utility::Uuid insert_menu_divider(
            const std::string &menu_model_name,
            const std::string &menu_path,
            const float menu_item_position);

        void set_submenu_position_in_parent(
            const std::string &menu_model_name,
            const std::string &submenu,
            const float submenu_position);

        void remove_all_menu_items(const std::string &menu_model_name);

        void remove_menu_item(const std::string &menu_model_name, const utility::Uuid item_id);

        virtual void menu_item_activated(
            const utility::JsonStore &menu_item_data, const std::string &user_data) {}

        utility::JsonStore attribute_menu_item_data(Attribute *attr);

        void update_attribute_menu_item_data(Attribute *attr);

        void make_attribute_visible_in_viewport_toolbar(
            Attribute *attr, const bool make_visible = true);

        void expose_attribute_in_model_data(
            Attribute *attr, const std::string &model_name, const bool expose = true);

        void redraw_viewport();

        virtual utility::JsonStore public_state_data();

        // re-implement this function and use it to add custom hotkeys
        virtual void register_hotkeys() {}

        virtual void
        attribute_changed(const utility::Uuid & /*attr_uuid*/, const int /*role_id*/) {}

        Attribute *get_attribute(const utility::Uuid &attr_uuid);

        Attribute *get_attribute(const std::string &attr_title);

        caf::actor_addr parent_actor_addr_;

        void connect_to_ui();
        void grab_mouse_focus();
        void release_mouse_focus();
        void grab_keyboard_focus();
        void release_keyboard_focus();

        virtual void disconnect_from_ui();

        [[nodiscard]] bool connected_to_ui() const { return connected_to_ui_; }
        virtual void connected_to_ui_changed() {}

        void disable_linking() { linking_disabled_ = true; }
        void enable_linking() { linking_disabled_ = false; }

        std::vector<AttributePtr> attributes_;

        // Call this code to expose a UI panel in xSTUDIO's panels menu. See
        // annotations tool for an example. To allow the panel to be shown in
        // a floating window, provide an incon path and a position within
        // the pop-out button shelf (top left of viewport panels) for it to
        // appear in.
        void register_ui_panel_qml(
            const std::string &panel_name,
            const std::string &qml_code,
            const float position_in_menu,
            const std::string &viewport_popout_button_icon_path = "",
            const float &viewport_popout_button_position        = -1.0f,
            const utility::Uuid toggle_hotkey_id                = utility::Uuid());

        // Call this code to expose a widget that can dock to the left/right or
        // top/bottom of the viewport
        Attribute *register_viewport_dockable_widget(
            const std::string &widget_name,
            const std::string &button_icon_qrc_path,
            const std::string &button_tooltip,
            const float button_position,
            const bool enabled,
            const std::string &left_right_dockable_widget_qml,
            const std::string &top_bottom_dockable_widget_qml,
            const utility::Uuid toggle_widget_visible_hotkey = utility::Uuid());

        std::set<utility::Uuid> dock_widget_attributes_;

        // re-implement to get a callback when your dockable widget has been
        // made visible by the user clicking on the toggle button in the
        // viewport action bar
        virtual void viewport_dockable_widget_activated(std::string &widget_name) {}

        // re-implement to get a callback when your dockable widget has been
        // hidden by the user clicking on the toggle button in the
        // viewport action bar. Also this callback is made when the dockable
        // widget is hidden or goes off screen.
        // If you have a tool that captures mouse events, for example, you may
        // want to stop grabbing mouse events when the widget is hidden
        virtual void viewport_dockable_widget_deactivated(std::string &widget_name) {}

        // To instance a single global UI item on startup (which might, for
        // example, insert menu items into the xstudio interface) pass the
        // necessary qml to this function.
        void register_singleton_qml(const std::string &qml_code);

      private:
        void notify_attribute_destroyed(Attribute *);
        void attribute_changed(const utility::Uuid &attr_uuid, const int role_id, bool notify);
        void add_attribute(Attribute *attr);

        caf::actor keypress_monitor_actor_;
        caf::actor keyboard_and_mouse_group_;
        caf::actor attribute_events_group_;

        caf::actor_addr attr_sync_source_adress_;
        std::set<caf::actor_addr> partially_linked_modules_;
        std::set<caf::actor_addr> fully_linked_modules_;
        std::set<utility::Uuid> linked_attrs_;
        std::set<utility::Uuid> attrs_in_toolbar_;
        std::set<std::string> connected_viewport_names_;
        std::set<caf::actor> connected_viewports_;

        bool connected_to_ui_  = {false};
        bool linking_disabled_ = {false};
        utility::Uuid module_uuid_;
        std::string name_;
        std::set<utility::Uuid> attrs_waiting_to_update_prefs_;
        std::map<std::string, std::vector<utility::Uuid>> menu_items_;
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
