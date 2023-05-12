#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, change_attribute_event_atom
from xstudio.core import KeyboardModifier

"""

DnSetLoopRange - Demo xSTUDIO Python Plugin

This simple plugin demostrates how to add menu items to xSTUDIO's main menus
which will trigger a callback when the user clicks on the menu item. Creating
a hotkey and associating it with the menu item is also illustrated.

In this case the menu action will modify the loop frame range of the playhead
based on metadata of the current media item on the screen. Thus we also
demostrate how to access the on-screen media item, its metadata and modify
the playhead properties.

Note that the metadata fields that we access in this plugin are specific to
DNEG's pipeline, and the metadata is added to the media item by the DNEG data
source plugin. Therefore this plugin will not work as-is and would need to
be modified to work with your own metadata layout.
"""


def map_render_frame_to_playhead_frame(render_frame, playhead):

    # For a given render frame number for the current on-screen media source,
    # we need to find out where that frame is in the playhead timeline.

    # At DNEG, for a render that starts on frame 1001, say, we embed
    # timecode in quicktime encodings of 00:00:41:17 so we 'know'
    # the first frame is frame 1001. For EXRs (and other image based
    # sequences) xSTUDIO adds phoney timecode, also of 00:00:41:17,
    # to the media reference to keep the approach consistent. (If
    # it's present in the EXR metadata, 'real' timecode - e.g. from
    # the camera or audio, will still be untouched in the metadata
    # dictionary)
    source_first_frame_from_tc = playhead.on_screen_media.media_source(
    ).media_reference.timecode().total_frames()

    # ... however, in xSTUDIO the first logical media frame of a
    # quicktime will always be 1. For EXRs, it corresponds to the
    # first framenumber in the file sequence (so it would be 1001).
    source_first_logical_frame = playhead.on_screen_media.media_source(
    ).media_reference.frame(0)

    # Thus in general, we expect this value to be zero for EXR (or
    # other image based sequences)
    map_media_frame_to_render_frame_num = source_first_frame_from_tc - \
        source_first_logical_frame

    # so this is the (render) frame number of the current on screen image
    current_render_frame_num = playhead.media_frame + \
        map_media_frame_to_render_frame_num

    # Now we can get the simple mapping between the current playhead
    # timeline frame (playhead.position) and the render_frame
    return render_frame + playhead.position - current_render_frame_num


class DnSetLoopRange(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(
            self,
            connection,
            "dnSetLoopRange")

        # Register a hotkey, passing in our callback function
        loop_on_cut_hotkey_uuid = self.register_hotkey(
            self.loop_on_cut_range,  # the callback
            'P',
            default_modifier=KeyboardModifier.ShiftModifier,
            hotkey_name="Set Loop In/Out to Cut Range",
            description="Hit this modifier to loop over the cut range for the given source",
            auto_repeat=False,
            component="Playback",
            context="any")

        # Register a 2nd hotkey
        loop_on_comp_hotkey_uuid = self.register_hotkey(
            self.loop_on_comp_range,
            'P',
            default_modifier=KeyboardModifier.ControlModifier,
            hotkey_name="Set Loop In/Out to Comp Range",
            description="Hit this modifier to loop over the comp range for the given source",
            auto_repeat=False,
            component="Playback",
            context="any")

        # Extend the 'playback' menu - the key here is the menu_path
        # argument. If you wanted to put this menu item in a sub-menu
        # under the Playback menu, you could do menu_path="playback_menu|Pipeline Loop range"
        # for example, i.e. use pipes to append sub menus.
        self.add_menu_item(
            menu_item_name="Set Loop to Cut Range",
            menu_path="playback_menu",
            menu_trigger_callback=self.loop_on_cut_range,
            hotkey_uuid=loop_on_cut_hotkey_uuid)

        self.add_menu_item(
            menu_item_name="Set Loop to Comp Range",
            menu_path="playback_menu",
            menu_trigger_callback=self.loop_on_comp_range,
            hotkey_uuid=loop_on_comp_hotkey_uuid)

        self.connect_to_ui()

    def loop_on_cut_range(self, activated=True):

        # Here we access the metadata propery of the on screen media item.
        playhead = super().current_playhead()
        if activated and playhead.on_screen_media and playhead.on_screen_media.metadata:

            try:
                # Here 'sg_cut_range' is custom metadata added to the media
                # elsewhere (DNEG's data source plugin, in this case, which
                # interfaces with the shotgrid database). The cut range is
                # a string of two numbers separated with a dash - "1008-1040"
                # for example. I've been a bit lazy with error checking
                # here but it should give the gist.
                cut_range = JsonStore(playhead.on_screen_media.metadata).get(
                    '/metadata/shotgun/shot/attributes/sg_cut_range')

                cut_in = int(cut_range.split("-")[0])
                cut_out = int(cut_range.split("-")[1])

                # Map these numbers to the playhead timeline frame of reference
                cut_in = map_render_frame_to_playhead_frame(cut_in, playhead)
                cut_out = map_render_frame_to_playhead_frame(cut_out, playhead)

                # Now we can set the relevant properties on the playhead to
                # apply the loop
                if playhead.loop_in_point and cut_in == playhead.loop_in_point and playhead.loop_out_point == cut_out:
                    # ... toggle loop OFF if we're already looping on the range
                    playhead.use_loop_range = False
                else:
                    playhead.loop_in_point = cut_in
                    playhead.loop_out_point = cut_out
                    playhead.use_loop_range = True
            except Exception as e:
                print (e)

    def loop_on_comp_range(self, activated=True):

        playhead = super().current_playhead()
        if activated and playhead.on_screen_media and playhead.on_screen_media.metadata:

            try:
                import json
                comp_range = JsonStore(playhead.on_screen_media.metadata).get(
                    '/metadata/shotgun/shot/attributes/sg_comp_range')
                comp_in = int(comp_range.split("-")[0])
                comp_out = int(comp_range.split("-")[1])

                comp_in = map_render_frame_to_playhead_frame(comp_in, playhead)
                comp_out = map_render_frame_to_playhead_frame(
                    comp_out,
                    playhead)

                if playhead.loop_in_point and comp_in == playhead.loop_in_point and playhead.loop_out_point == comp_out:
                    playhead.use_loop_range = False
                else:
                    playhead.loop_in_point = comp_in
                    playhead.loop_out_point = comp_out
                    playhead.use_loop_range = True
            except Exception as e:
                print (e)


# This method is always required by python plugins with the given signature
def create_plugin_instance(connection):
    return DnSetLoopRange(connection)


# This code allows you to execute this python script as an independent process
# to xstudio itself
if __name__ == "__main__":

    XSTUDIO = Connection(auto_connect=True)
    create_plugin_instance(XSTUDIO)
    XSTUDIO.process_events_forever()
