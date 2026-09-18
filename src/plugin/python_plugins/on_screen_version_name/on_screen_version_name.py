#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import HUDPlugin
from xstudio.core import JsonStore
from xstudio.core import event_atom, show_atom
from xstudio.core import HUDElementPosition, ColourTriplet
from xstudio.api.session.media import Media, MediaSource

import json

# path to QML resources relative to this .py file
qml_folder_name = "qml/OnScreenVersionName.1"

# QML code necessary to create the overlay item that is drawn over the xSTUDIO
# viewport.
overlay_qml = """
OnScreenVersionNameOverlay {
}
"""

# Declare our plugin class - we're using the HUDPlugin base class meaning we
# get a toggle under the 'HUD' button in the viewport toolbar to turn our
# hud on and off
class OnScreenVersionName(HUDPlugin):

    def __init__(self, connection):

        HUDPlugin.__init__(
            self,
            connection,
            "On Screen Version Name", # the name of the HUD item
            qml_folder=qml_folder_name,
            position_in_hud_list=-9.0)

        # add an attribute to control the size of the text
        self.font_size = self.add_attribute(
            "Font Size",
            30.0, # default size
            # additional attribute role data is provided as a dictionary
            # as follows. The keys must be valid role names. See attribute.hpp
            # for a list of the attribute role data names
            {
                "float_scrub_min": 5.0,
                "float_scrub_max": 100.0,
                "float_scrub_step": 2.0,
                "float_display_decimals": 2
            },
            register_as_preference=True
            )
        # adding to a ui attrs group means we can access the attribute in our
        # QML code by referencing the group name and the attribute title
        self.font_size.expose_in_ui_attrs_group("on_screen_version_name")
        self.font_size.set_tool_tip("Set the size of the font for displaying the version name")
        self.font_size.set_redraw_viewport_on_change(True)

        # add an attribute to control the size of the text
        self.font_colour = self.add_attribute(
            "Font Colour",
            ColourTriplet(1.0, 1.0, 1.0)
            )
        self.font_colour.expose_in_ui_attrs_group("on_screen_version_name")

        self.auto_hide = self.add_attribute(
            "Auto Hide",
            True,
            register_as_preference=True
            )
        self.auto_hide.expose_in_ui_attrs_group("on_screen_version_name")

        self.hide_timeout = self.add_attribute(
            "Auto Hide Timeout (seconds)",
            10.0, # default number of seconds to hide the version
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 20.0,
                "float_scrub_step": 0.5,
                "float_display_decimals": 1
            },
            register_as_preference=True
            )
        self.hide_timeout.expose_in_ui_attrs_group("on_screen_version_name")

        self.index_into_data = self.add_attribute(
            "Index of MediaList Column to Show",
            3, # default number of seconds to hide the version
            {
                "integer_min": 0,
                "integer_max": 10
            },
            register_as_preference=True
            )
        self.hide_timeout.expose_in_ui_attrs_group("on_screen_version_name")

        # this attr holds the actual text to be displayed on screen
        self.display_text = self.add_attribute(
            "Display Text",
            "",
            {}
            ) # default size
        self.display_text.expose_in_ui_attrs_group("on_screen_version_name")

        # the following calls mean that these attributes get controls in
        # the settings dialog for thie HUD, accessed via the cog icon next to
        # the item in the HUD pop-up menu
        self.add_hud_settings_attribute(self.font_size)
        self.add_hud_settings_attribute(self.hide_timeout)
        self.add_hud_settings_attribute(self.auto_hide)
        self.add_hud_settings_attribute(self.font_colour)

        # here we provide the QML code to instance the item that will draw
        # the overlay graphics
        self.hud_element_qml(
            overlay_qml,
            HUDElementPosition.TopCenter)

        # expose our attributes in the UI layer
        self.connect_to_ui()

    def attribute_changed(self, attr, role):
        super(OnScreenVersionName, self).attribute_changed(attr, role)

    def media_item_hud_data(self, media_item = None):

        # This method is called by the base class. media_item is an instance
        # of the Media class, and is the media object for some image that is
        # going on-screen. We use it to build data that we return which will
        # subsequently be available in the property 'media_item_hud_data'
        # in our QML item that draws the HUD graphics.
        if media_item:
            idx = self.index_into_data.value()
            # display_info is an array of values corresponding to the columns
            # of the Media List Panel in the xSTUDIO UI (the columns are fully
            # user-configurable, by the way)
            return media_item.display_info[idx]
        else:
            return ""


# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return OnScreenVersionName(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin_instance = create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()