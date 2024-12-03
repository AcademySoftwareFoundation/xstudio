#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import HUDPlugin
from xstudio.core import JsonStore
from xstudio.core import event_atom, show_atom
from xstudio.core import HUDElementPosition
from xstudio.api.session.media import Media
import json

# path to QML resources relative to this .py file
qml_folder_name = "qml/ViewportFlagIndicator.1"

# QML code necessary to instance a custom settings panel for this HUD plugin.
# This is the dialog that is shown when the user clicks on the cog icon in the
# HUD button in the viewport toolbar.
# Note that if your plugin uses 'plain' attributes (int, string, float, bool)
# then you don't need a custom settings panel, xstudio will create a default 
# one for you with controls for adjusting attributes that you want the user to
# be able to change.
settings_box_qml = """
import QtQuick 2.12
ViewportFlagIndicatorSettingsDialog {
}
"""

# QML code necessary to create the overlay item that is drawn over the xSTUDIO
# viewport.
overlay_qml = """
ViewportFlagIndicatorSettingsOverlay {
}
"""

# Declare our plugin class - we're using the HUDPlugin base class meaning we
# get a toggle under the 'HUD' button in the viewport toolbar to turn our
# hud on and off
class ViewportFlagIndicatorPlugin(HUDPlugin):

    def __init__(self, connection):

        HUDPlugin.__init__(
            self,
            connection,
            "Media Dot", # the name of the HUD item
            qml_folder=qml_folder_name,
            position_in_hud_list=-10.0)
        
        # calling this function sets up our custom settings dialog with the
        # front end. Uncomment this line to use the customised settings dialog.
        # Otherwise the automated settings dialog will show.
        # self.set_custom_settings_qml(settings_box_qml)

        # add an attribute to control the size of the flag indicator
        self.indicator_size = self.add_attribute(
            "Indicator Size",
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
            # The flag below ensures that if the user changes the Indicator Size
            # the value will be stored in the user's preferences files amnd
            register_as_preference=True 
            )

        # here we add the attribute to a named 'group'. In the qml code, we can
        # get to the attribute data using the 'XsModuleData' item and telling
        # it which group we want to attach to.
        self.indicator_size.expose_in_ui_attrs_group("vp_flag_indicator")
        self.indicator_size.set_tool_tip("Set the size of the indicator in pixels")
        self.indicator_size.set_redraw_viewport_on_change(True)

        # this call will mean xstudio can add a slider for the indicator size
        # attribute to the settings panel for the plugin
        self.add_hud_settings_attribute(self.indicator_size)

        # Adding an attribute whose value is a Json dictionary. The entries in
        # the dictionary will be the on-screen media flag colour for each
        # viewport. There will on be one instance of this plugin, but xSTUDIO
        # can have multiple viewports showing different media so we need to 
        # track multiple 'flag' colours
        self.flag_colours_attr = self.add_attribute(
            "Flag Colours",
            JsonStore({}))
        self.flag_colours_attr.expose_in_ui_attrs_group("vp_flag_indicator")

        # here we provide the QML code to instance the item that will draw
        # the overlay graphics
        self.hud_element_qml(
            overlay_qml,
            HUDElementPosition.BottomRight)

        # expose our attributes in the UI layer
        self.connect_to_ui()

        # store the current flag colours (per viewport) and current onscreen
        # media (also per viewport)
        self.flag_colours = {}
        self.onscreen_media_items = {}


# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return ViewportFlagIndicatorPlugin(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin_instance = create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()