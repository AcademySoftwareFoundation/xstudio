#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.plugin import HUDPlugin
from xstudio.core import JsonStore, ColourTriplet
from xstudio.core import AttributeRole
import json
import sys
from os import path
import pathlib

qml_folder_name = "qml/ViewportFieldChartPlugin.1"

settings_box_qml = """
import QtQuick
ViewportFieldChartSettingsDialog {
}
"""

overlay_qml = """
ViewportFieldChart {
}
"""

class ViewportFieldChart(HUDPlugin):

    def __init__(self, connection):

        HUDPlugin.__init__(
            self,
            connection,
            "Field Charts",
            qml_folder=qml_folder_name)

        self.set_custom_settings_qml(settings_box_qml)

        cdir = path.realpath(path.dirname(__file__))

        # A json that is a list of the charts provided with this plugin
        # that can be toggled on or off
        uri = pathlib.Path("{0}/{1}".format(cdir, "qml/mkt_anim_grid.svg")).as_uri()
        self.default_charts = {
            "Anim Grid 1": str(uri)
            }

        # this attr holds the list of charts that has been toggled on to be
        # visible. It is registered as a preference so that the visibility
        # of the charts persists between sessions for the user
        self.charts_visibility = self.add_attribute(
            "Visibile Charts",
            ["Anim Grid 1"],
            register_as_preference=True)
        self.charts_visibility.expose_in_ui_attrs_group(["fieldchart_image_set"])

        # This JSON contains details (file path and visibility) of charts added
        # by the user
        self.user_charts = self.add_attribute(
            "User Charts",
            JsonStore({}),
            register_as_preference=True)
        self.user_charts.expose_in_ui_attrs_group(["fieldchart_image_set"])


        # This JSON is the merged dict of user and default charts. It's jsut
        # for convenience when building the settings widget that lets us
        # toggle the charts on/off
        self.all_charts = self.add_attribute(
            "All Charts",
            JsonStore({}))
        self.all_charts.expose_in_ui_attrs_group(["fieldchart_image_set"])

        # This attr holds merged list of the default charts and user charts that
        # are active and therefore visible. It's just a list of the filepath
        # to each svg file
        self.active_charts = self.add_attribute(
            "Active Charts",
            [])
        self.active_charts.expose_in_ui_attrs_group(["fieldchart_image_set"])


        # attribute to hold the current mask safety
        self.opacity = self.add_attribute(
            "Opacity",
            1.0,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 1.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            },
            register_as_preference=True)

        self.opacity.expose_in_ui_attrs_group(["fieldchart_settings"])
        self.opacity.set_tool_tip(
            "Sets the opacity of the field chart"
            )

        # add an attribute to control the size of the text
        self.colour = self.add_attribute(
            "Colour",
            ColourTriplet(1.0, 1.0, 1.0),
            register_as_preference=True)
        self.colour.expose_in_ui_attrs_group("fieldchart_settings")

        # here we provide the QML code to instance the item that will draw
        # the overlay graphics
        self.hud_element_qml(overlay_qml)

        # expose our attributes in the UI layer
        self.connect_to_ui()

        self.attribute_changed(self.charts_visibility, AttributeRole.Value)

    def attribute_changed(self, attr, role):

        if attr == self.charts_visibility or attr == self.user_charts:

            # here we update the list of svg filenames that are active
            __user_charts = self.user_charts.value()
            # merge user charts and default charts
            m = {**__user_charts, **self.default_charts}
            __active_charts = []
            v = self.charts_visibility.value()
            for c in m:
                if c in v:
                    __active_charts.append(m[c])
            self.active_charts.set_value(__active_charts)
            self.all_charts.set_value(m)



# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return ViewportFieldChart(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin_instance = create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()