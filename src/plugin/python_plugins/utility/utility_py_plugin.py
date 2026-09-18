#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from xstudio.core import AttributeRole, Uuid
from xstudio.plugin import PluginBase

# This plugin is intended to allow for various utility functions to be 
# implemented in Python. The first utility function is to allow for the
# setting of media flip/flop.

class UtilityPythonPlugin(PluginBase):
    """Python plugin for various utility functions."""

    def __init__(self, connection):
        """Initialize the utility plugin.

        """
        PluginBase.__init__(
            self,
            connection,
            name="UtilityPythonPlugin"
        )

        # Flip/Flop setting

        self.flip_flop_attr = self.add_attribute(
            attribute_name="Flip/Flop",
            attribute_value="None",
            attribute_role_data={
                "combo_box_options": ["None", "Flop", "Flip", "Flip/Flop"],
                "combo_box_options_enabled": [True, True, True, True],
            }
        )

        self.flip_flop_menu_id = self.insert_menu_item(
            menu_model_name="media_list_menu_",
            menu_text="Set Flip/Flop",
            menu_path="Media Settings",
            menu_item_position=1.0,
            attr_id=self.flip_flop_attr.uuid,
        )

        self.set_submenu_position(
            menu_model_name="media_list_menu_",
            submenu_path="Media Settings|Set Flip/Flop",
            menu_item_position=3.9)

        self.connect_to_ui()
        self.curr_val = "None"

    def menu_item_shown(self, menu_item_data, user_data):
        """Handle menu item shown event to update menu state.
        """
        shown_uuid = Uuid(menu_item_data["uuid"])

        # Always base the Auto label off the main selection
        media = self.connection.api.session.selected_media[0]
        # Update auto labels when menu is shown based on the current media metadata
        if shown_uuid == self.flip_flop_attr.uuid:
            matrix = media.transform_matrix
            flopped = matrix[0][0] < 0
            flipped = matrix[1][1] < 0
            self.curr_val = "None"
            if flopped and flipped:
                self.curr_val = "Flip/Flop"
            elif flopped:
                self.curr_val = "Flop"
            elif flipped:
                self.curr_val = "Flip"
            self.flip_flop_attr.set_value(self.curr_val)

    def attribute_changed(self, attribute, role):
        """Handle attribute changes.

        * Ignore changes that are not value changes.
        * Support batch changes when multiple media are selected
        """
        if role != AttributeRole.Value:
            return

        if attribute == self.flip_flop_attr:
            flip_flop_value = self.flip_flop_attr.value()
            if self.curr_val == flip_flop_value:
                return
            medias = self.connection.api.session.selected_media
            flip = flip_flop_value in ["Flip", "Flip/Flop"]
            flop = flip_flop_value in ["Flop", "Flip/Flop"]

            for media in medias:
                tr = media.transform_matrix
                tr[0] = [-abs(tr[0][0]) if flop else abs(tr[0][0]), tr[0][1], tr[0][2], tr[0][3]]
                tr[1] = [tr[1][0], -abs(tr[1][1]) if flip else abs(tr[1][1]), tr[1][2], tr[1][3]]
                media.transform_matrix = tr

def create_plugin_instance(connection):
    """Create and return a plugin instance."""
    return UtilityPythonPlugin(connection)
