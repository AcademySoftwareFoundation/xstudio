// SPDX-License-Identifier: Apache-2.0

#include "shotbrowser_plugin.hpp"

using namespace xstudio;
using namespace xstudio::shotbrowser;

// Note the 'StandardPlugin' class is a Module AND a caf::event_based_actor.
// So we are a full actor we don't have to do that 'set_parent_actor_addr' call
ShotBrowser::ShotBrowser(caf::actor_config &cfg, const utility::JsonStore &init_settings)
    : plugin::StandardPlugin(cfg, "ShotBrowser", init_settings) {

    // You might not want or need to use attributes in your plugin as you'll
    // be doing a QAbstractItemModel ... however, they can be useful for
    // creating menus and holding values with preferences etc

    // here's a colour attribute we can use in the interface
    some_colour_attribute_ = add_colour_attribute(
        "ShotBrowser Colour", "ShotBrowser Colour", utility::ColourTriplet(0.5f, 0.4f, 1.0f));

    // and here's a multichoice attribute we will use in the interface
    some_multichoice_attribute_ = add_string_choice_attribute(
        "ShotBrowser Multichoice",
        "ShotBrowser Multichoice",
        "Elephant",
        {"Elephant", "Griaffe", "Lion", "Zebra"});

    // a toggle attribute
    some_bool_attribute_ = add_boolean_attribute("Some Toggle", "Some Toggle", false);

    // this call is essential to set-up the base class
    make_behavior();

    // this call is required for attribute 'groups', menus etc. to be exposed
    // in the Qt/QML layer ... if you're not using attrs or backend menu
    // generation at all then you don't need this.
    connect_to_ui();

    // here's the code where we register our interface as an xSTUDIO 'panel'
    // that can be selected in the UI
    register_ui_panel_qml(
        "ShotBrowser Menus Demo",
        R"(
            import ShotBrowser 1.0
            ShotBrowserPanel {
                anchors.fill: parent
            }
        )");

    register_ui_panel_qml(
        "Shot History",
        R"(
            import ShotBrowser 1.0
            ShotHistory {
                anchors.fill: parent
            }
        )");

    register_ui_panel_qml(
        "Notes History",
        R"(
            import ShotBrowser 1.0
            NotesHistory {
                anchors.fill: parent
            }
        )");

    register_ui_panel_qml(
        "Shot Browser",
        R"(
            import ShotBrowser 1.0
            ShotBrowserRoot {
                anchors.fill: parent
            }
        )");

    demo_hotkey_ = register_hotkey(
        int('L'), // TODO: keyboard key enumerator shoould be used here
        ui::ControlModifier,
        "Do Something In ShotBrowser",        // short description
        "This hotkey does something or other" // full description
    );

    setup_menus();
}

void ShotBrowser::menu_item_activated(const utility::Uuid &menu_item_uuid) {
    if (menu_item_uuid == my_menu_item_) {
        std::cerr << "My menu item was clicked.\n";
    }
}

void ShotBrowser::setup_menus() {

    // You can easily add items to a menu model here in C++ ... you might not
    // need this but here are examples in case you do.

    // If you do use this note the asterix at the end of the "shotbrowser_menu_1*"
    // which is the menu model name. This inserts the menu item into ANY/ALL menu
    // model whose name starts with  "shotbrowser_menu_1". We need this because
    // we only have one instance of this ShotBrowser class but there may be
    // multiple instances of the ShotBrowserPanel UI - each UI instance has its
    // own menu model with a unique name but that name starts with "shotbrowser_menu_1".
    // There's more explanaition in the QML.

    // here we can add a simple menu item - we get a callback with the uuid
    // in menu_item_activated when the user clicks on it
    my_menu_item_ = insert_menu_item(
        "shotbrowser_menu_1*",
        "Backend Simple Menu Item",
        "Backend Menu Example|Submenu 1|Submenu 2",
        0.0f);

    // here we can add our string choice attr to a menu
    insert_menu_item(
        "shotbrowser_menu_1*",
        "Backend Multichoice",
        "Backend Menu Example|Submenu 1",
        1.0f,
        some_multichoice_attribute_);

    // here we can add a divider - you can use the uuid to remove it
    // with Module::remove_menu_item
    utility::Uuid divider =
        insert_menu_divider("shotbrowser_menu_1*", "Backend Menu Example|Submenu 1", 2.0f);

    // Add a toggle menu
    insert_menu_item(
        "shotbrowser_menu_1*",
        "Backend Toggle",
        "Backend Menu Example|Submenu 1",
        3.0f,
        some_bool_attribute_);
}

ShotBrowser::~ShotBrowser() {}

caf::message_handler ShotBrowser::message_handler_extensions() {

    // Here's where our own message handler is declared

    return caf::message_handler([=](utility::event_atom, std::string, bool) {});
}

void ShotBrowser::attribute_changed(const utility::Uuid &attribute_uuid, const int /*role*/) {

    /*std::cerr << "attribute_uuid " << to_string(attribute_uuid) << "\n";
    std::cerr << get_attribute(attribute_uuid)->as_json().dump(2) << "\n";*/

    if (attribute_uuid == some_colour_attribute_->uuid()) {


    } else if (attribute_uuid == some_multichoice_attribute_->uuid()) {


    } else if (attribute_uuid == some_bool_attribute_->uuid()) {

        // let's mess with the multichoice choices
        if (some_bool_attribute_->value()) {
            some_multichoice_attribute_->set_role_data(
                module::Attribute::StringChoices,
                std::vector<std::string>({"Horse", "Sheep", "Cow", "Goat"}));
            some_multichoice_attribute_->set_value("Cow");
        } else {
            some_multichoice_attribute_->set_role_data(
                module::Attribute::StringChoices,
                std::vector<std::string>({"Elephant", "Griaffe", "Lion", "Zebra"}));
            some_multichoice_attribute_->set_value("Elephant");
        }
    }
}


void ShotBrowser::register_hotkeys() {

    demo_hotkey_ = register_hotkey(
        int('B'),
        ui::ControlModifier,
        "Toggle Something Tool",
        "This toggles the menu toggle on and off.");
}

void ShotBrowser::hotkey_pressed(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    if (hotkey_uuid == demo_hotkey_) {

        some_bool_attribute_->set_value(!some_bool_attribute_->value());
    }
}

void ShotBrowser::hotkey_released(
    const utility::Uuid &hotkey_uuid, const std::string & /*context*/) {
    // if we want, we can do stuff when the user releases a hotkey
}

void ShotBrowser::images_going_on_screen(
    const media_reader::ImageBufDisplaySetPtr &images,
    const std::string viewport_name,
    const bool playhead_playing) {

    // each viewport will call this function shortly before it refreshes to
    // draw the image data of 'images'. Note that current 'images' contains
    // just a single image because our viewport doesn't do multiple images
    // yet but assume that images will have more than 1 element in the future.
    // The 1st image will always be from the 'primary' media item, though, so
    // you can probably ignore the others anyway.
}

extern "C" {
plugin_manager::PluginFactoryCollection *plugin_factory_collection_ptr() {
    return new plugin_manager::PluginFactoryCollection(
        std::vector<std::shared_ptr<plugin_manager::PluginFactory>>(
            {std::make_shared<plugin_manager::PluginFactoryTemplate<ShotBrowser>>(
                ShotBrowser::PLUGIN_UUID,
                "ShotBrowser",
                plugin_manager::PluginFlags::PF_DATA_SOURCE,
                true, // resident
                "Al Crate",
                "DNEG Shotgrid & Ivy Integration Plugin")}));
}
}