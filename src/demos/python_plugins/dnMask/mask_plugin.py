#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, change_attribute_event_atom
import sys
import time, threading

fartoo = """
import "file://user_data/.tmp/xstudio/python_plugins/dnMask/qml/DnMask.1"
DnMaskButton {
    id: control
    anchors.fill: parent
}
"""

pingpong = """
import "file://user_data/.tmp/xstudio/python_plugins/dnMask/qml/DnMask.1"
DnMaskOverlay {
}
"""

class MyPlugin(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(self, connection, "MyPlugin", JsonStore({"ink": "stonk"}))

        self.mask = self.add_attribute(
            "dnMask",
            "Off",
            {
                "combo_box_options": ["Off", "1.77", "1.89", "2.0"],
                "groups": ["main_toolbar", "popout_toolbar", "dnmask_values"],
                "qml_code": fartoo
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
                "qml_code": pingpong
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




def create_plugin_instance(connection):
    return MyPlugin(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin(XSTUDIO)
    XSTUDIO.process_events_forever()