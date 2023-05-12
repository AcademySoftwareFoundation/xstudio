#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, change_attribute_event_atom
import sys
import time, threading

qml_folder_name = "qml/DnMask.1"

button_qml = """
DnMaskButton {
    id: control
    anchors.fill: parent
}
"""

overlay_qml = """
DnMaskOverlay {
}
"""

class MaskPlugin(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(
            self,
            connection,
            "MaskPlugin",
            qml_folder=qml_folder_name)

        # Note we pass a json-like dictionary as the third argument when
        # using 'add_attribute'. 
        # This describes the attribute data per 'role'. The role name is the key,
        # and the role data is the corresponding value. The role name must be
        # from the list of valid role names - these can be seen in attribute.hpp
        # file in the xstudio source code. The role data can be string, float,
        # int, bool, list(str) - no restrictions are placed on the type of the
        # role data but for UI elements to work correctly you need to use the
        # type that xSTUDIO expects. So the 'combo_box_options' role data should
        # be a list(str).
        self.mask = self.add_attribute(
            "dnMask", #attr name
            "Off", #attr intial value
            {
                "combo_box_options": ["Off", "1.77", "1.89", "2.0"],
                "groups": ["main_toolbar", "popout_toolbar", "dnmask_values"], #same as calling 'expose_in_ui_attrs_group' with these values later
                "qml_code": button_qml
            })

        # attribute to hold the current mask aspect
        self.current_mask_aspect = self.add_attribute(
            "Mask Aspect",
            1.77)

        self.current_mask_aspect.expose_in_ui_attrs_group(["dnmask_values"]);

        # attribute to hold the current mask safety
        self.current_mask_safety = self.add_attribute(
            "Mask Aspect",
            1.77)

        self.current_mask_safety.expose_in_ui_attrs_group(["dnmask_values"]);

        # attribute to hold the current mask safety
        self.line_thickness = self.add_attribute(
            "Mask Line Thickness",
            1.0,
            {
                "float_scrub_min": 0.5,
                "float_scrub_max": 10.0,
                "float_scrub_step": 0.25,
                "float_display_decimals": 2
            })

        self.line_thickness.expose_in_ui_attrs_group(["dnmask_settings"]);
        self.line_thickness.set_tool_tip("Sets the thickness of the masking lines");
        self.line_thickness.set_redraw_viewport_on_change(True);

        # attribute to hold the current mask safety
        self.line_opacity = self.add_attribute(
            "Mask Line Opacity",
            1.0,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 1.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            })

        self.line_opacity.expose_in_ui_attrs_group(["dnmask_settings"]);
        self.line_opacity.set_tool_tip(
            "Sets the opacity of the masking lines. Set to zero to hide the lines"
            );


        self.show_mask_label = self.add_attribute(
            "Show Mask Label",
            True)
        self.show_mask_label.expose_in_ui_attrs_group(["dnmask_settings"])

        self.mask_overlay = self.add_attribute(
            "Mask Overlay",
            "",
            {
                "qml_code": overlay_qml
            })
        self.mask_overlay.expose_in_ui_attrs_group(["viewport_overlay_plugins"]);

        self.connect_to_ui()

        self.set_attribute_changed_event_handler(self.attribute_changed)

    def attribute_changed(self, attr):

        if attr == self.mask:
            if str(attr.value()) == "1.77":
                self.current_mask_aspect.set_value(1.77)
            elif str(attr.value()) == "1.89":
                self.current_mask_aspect.set_value(1.89)
            elif str(attr.value()) == "2.0":
                self.current_mask_aspect.set_value(2.0)
            else:
                self.current_mask_aspect.set_value(0.0)

# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return MaskPlugin(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin_instance = create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()