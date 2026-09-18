#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import MaskPlugin
from xstudio.core import JsonStore, change_attribute_event_atom
from xstudio.core import Mask, VectorMask, AttributeRole, KeyboardModifier
import sys
import os
from xstudio.core import event_atom, show_atom, viewport_atom
from xstudio.api.session.media import Media, MediaSource
import json

class BasicMaskPlugin(MaskPlugin):

    def __init__(self, connection):

        MaskPlugin.__init__(
            self,
            connection,
            "Mask")

        self.mask_aspect_ratio = self.add_attribute(
            "Mask Aspect Ratio",
            1.78,
            {
                "float_scrub_min": 1.33,
                "float_scrub_max": 2.40,
                "float_scrub_step": 0.01,
                "float_display_decimals": 1
            },
            preference_path="/plugin/basic_masking/mask_aspect_ratio")

        self.mask_aspect_ratio.set_role_data("tooltip", "Sets the mask aspect ratio")
        self.add_hud_settings_attribute(self.mask_aspect_ratio)

        self.mask_aspect_presets = self.add_attribute(
            "Aspect Ratio Presets",
            "...",
            {
                "combo_box_options": ["1.33", "1.78", "2.0", "2.35", "2.39", "2.40"]
            })
        self.add_hud_settings_attribute(self.mask_aspect_presets)

        self.show_mask_label = self.add_attribute(
            "Show Mask Label",
            True,
            preference_path="/plugin/basic_masking/show_mask_label")
        self.add_hud_settings_attribute(self.show_mask_label)

        self.label_size = self.add_attribute(
            "Label Size",
            12.0,
            {
                "float_scrub_min": 5.0,
                "float_scrub_max": 20.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            },
            preference_path="/plugin/basic_masking/mask_label_size")
        self.add_hud_settings_attribute(self.label_size)

        # attribute to hold the current mask safety
        self.line_thickness = self.add_attribute(
            "Mask Line Thickness",
            1.0,
            {
                "float_scrub_min": 0.5,
                "float_scrub_max": 10.0,
                "float_scrub_step": 0.25,
                "float_display_decimals": 2
            },
            preference_path="/plugin/basic_masking/mask_line_thickness")

        self.line_thickness.set_tool_tip("Sets the thickness of the masking lines")
        self.line_thickness.set_redraw_viewport_on_change(True)
        self.add_hud_settings_attribute(self.line_thickness)

        # attribute to hold the current mask safety
        self.line_opacity = self.add_attribute(
            "Mask Line Opacity",
            1.0,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 1.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            },
            preference_path="/plugin/basic_masking/mask_line_opacity")

        self.line_opacity.set_tool_tip(
            "Sets the opacity of the masking lines. Set to zero to hide the lines"
            )
        self.add_hud_settings_attribute(self.line_opacity)

        # attribute to hold the hidden area opacity
        self.hidden_area_opacity = self.add_attribute(
            "Hidden Area Opacity",
            1.0,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 1.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            },
            preference_path="/plugin/basic_masking/mask_opacity")
        self.add_hud_settings_attribute(self.hidden_area_opacity)

        # attribute to hold the hidden area opacity
        self.safety_percent = self.add_attribute(
            "Safety Percent",
            0.0,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 20.0,
                "float_scrub_step": 0.1,
                "float_display_decimals": 1
            },
            preference_path="/plugin/basic_masking/safety_percent")
        self.safety_percent.set_role_data("tooltip", "Sets the percentage of the image that is outside the mask area.")
        self.add_hud_settings_attribute(self.safety_percent)

        # Register a hotkey, passing in our callback function
        self.toggle_mask_hotkey = self.register_hotkey(
            self.toggle_mask_cb,  # the callback
            'M',
            default_modifier=KeyboardModifier.NoModifier,
            hotkey_name="Toggle Mask",
            description="Hit this key to toggle the basic mask overlay visibility",
            auto_repeat=False,
            component="Viewport",
            context="any")

        # expose our attributes in the UI layer
        self.connect_to_ui()

    def toggle_mask_cb(self, activated, context):
        if activated:
            self.toggle_attr.set_value(not self.toggle_attr.value())

    def static_masks(self):
        # This method is called by the base class.
        safety = self.safety_percent.value()/100.0
        aspc = 1.0/self.mask_aspect_ratio.value()
        rt = Mask(left=-1.0+safety,
                        right=1.0-safety,
                        top=aspc-safety*aspc,
                        bottom=safety*aspc-aspc,
                        line_thickness=self.line_thickness.value(),
                        line_opacity=self.line_opacity.value(),
                        mask_opacity=self.hidden_area_opacity.value(),
                        label= "{:.2f}".format(1.0/aspc) if self.show_mask_label.value() else "",
                        label_size=self.label_size.value(),
                        auto_fit=False)
        return rt

    def attribute_changed(self, attr, role):

        if attr == self.mask_aspect_presets and role == AttributeRole.Value:
            self.mask_aspect_ratio.set_value(float(self.mask_aspect_presets.value()))
        MaskPlugin.attribute_changed(self, attr, role)
        

# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return BasicMaskPlugin(connection)
