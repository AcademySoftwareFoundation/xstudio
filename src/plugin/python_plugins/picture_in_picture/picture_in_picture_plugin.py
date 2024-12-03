#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import ViewportLayoutPlugin
from xstudio.core import JsonStore
from xstudio.core import event_atom, show_atom
from xstudio.core import AssemblyMode
from xstudio.api.session.media import Media
import json


# Declare our plugin class - we're using the HUDPlugin base class meaning we
# get a toggle under the 'HUD' button in the viewport toolbar to turn our
# hud on and off
class PictureInPicturePlugin(ViewportLayoutPlugin):

    def __init__(self, connection):

        ViewportLayoutPlugin.__init__(
            self,
            connection,
            "Picture In Picture")
        
        # declare our mode
        self.add_layout_mode("PiP", AssemblyMode.AM_ALL)

        # add an attribute to control the inset border for the small images
        self.inset = self.add_attribute(
            "PiP Inset",
            0.0, # default inset
            # additional attribute role data is provided as a dictionary
            # as follows. The keys must be valid role names. See attribute.hpp
            # for a list of the attribute role data names
            {
                "float_scrub_min": -50.0, 
                "float_scrub_max": 50.0,
                "float_scrub_step": 0.5,
                "float_display_decimals": 2
            },
            # The flag below ensures that if the user changes the Indicator Size
            # the value will be stored in the user's preferences files amnd
            register_as_preference=True 
            )
        self.inset.set_tool_tip("Set the border inset size for images that are placed in the picture")

        # add an attribute to control the size of the inset pictures
        self.sizing = self.add_attribute(
            "PiP Size Percent",
            15.0, # default sizing
            {
                "float_scrub_min": 0.0, 
                "float_scrub_max": 100.0,
                "float_scrub_step": 2.0,
                "float_display_decimals": 2
            },
            register_as_preference=True 
            )
        self.sizing.set_tool_tip("Set the percent scaling of the inset pictures.")

        # add an attribute to control the size of the inset pictures
        self.start_position = self.add_attribute(
            "Start Position",
            "Top Left", # default sizing
            {
                "combo_box_options": ["Top Left", "Top Right", "Bottom Left", "Bottom Right"]
            },
            register_as_preference=True 
            )
        self.start_position.set_tool_tip("The position of the first inset image.")

        # add an attribute to control the layout positioning of subsequent images
        self.layout_direction = self.add_attribute(
            "Layout Direction",
            "Horizontal", # default sizing
            {
                "combo_box_options": ["Horizontal", "Vertical"]
            },
            register_as_preference=True 
            )
        self.layout_direction.set_tool_tip("The position of the first inset image.")

        self.add_layout_settings_attribute(self.inset, "PiP")
        self.add_layout_settings_attribute(self.sizing, "PiP")
        self.add_layout_settings_attribute(self.start_position, "PiP")
        self.add_layout_settings_attribute(self.layout_direction, "PiP")

    def do_layout(self, layout_name, image_set_data):
        
        # we draw all images in order
        image_draw_order = [i for i in range(0, len(image_set_data["image_info"]))]

        # first image is not moved or scaled
        image_transforms = [(0.0, 0.0, 1.0)]

        inset = self.inset.value()/100.0
        scaling = self.sizing.value()/100.0
        start = self.start_position.value()
        horiz = self.layout_direction.value() == "Horizontal"

        hero_image_idx = image_set_data["hero_image_index"]
        hero_image_size = image_set_data["image_info"][hero_image_idx]['image_size_in_pixels']
        hero_pixel_aspect = image_set_data["image_info"][hero_image_idx]['pixel_aspect']
        aspect = hero_pixel_aspect*hero_image_size[2]/hero_image_size[3]

        if "Left" in start:
            if horiz:
                positioning = [(-1.0, -1.0), (1.0, -1.0), (-1.0, 1.0), (1.0, 1.0)]
            else:
                positioning = [(-1.0, -1.0), (-1.0, 1.0), (1.0, -1.0), (1.0, 1.0)]
        else:
            if horiz:
                positioning = [(1.0, -1.0), (-1.0, -1.0), (1.0, 1.0), (-1.0, 1.0)]
            else:
                positioning = [(1.0, -1.0), (1.0, 1.0), (-1.0, -1.0), (-1.0, 1.0)]

        if "Bottom" in start:
            positioning = [(r[0], -r[1]) for r in positioning]

        i = 0
        for d in range(len(image_set_data["image_info"])-1):

            r = positioning[i%len(positioning)]
            n = (int(i/len(positioning))*2)+1
            dx = scaling*n + inset
            dy = scaling + inset
            xpos = r[0] - dx if r[0] > 0.0 else r[0] + dx
            ypos = r[1] - dy if r[1] > 0.0 else r[1] + dy
            image_transforms.append((xpos, ypos/aspect, scaling))
            i += 1

        return (image_transforms, image_draw_order, 16.0/9.0)




# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return PictureInPicturePlugin(connection)


if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin_instance = create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()