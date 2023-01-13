#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore
import sys
import time, threading
import fast_calc

class MyPlugin(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(self, connection, "MyPlugin", JsonStore({"ink": "stonk"}))

        fast_calc.add(1, 2)

        self.floaty = self.add_attribute(
            "Giggle Biz",
            0.5,
            {
                "float_scrub_min": 0.0,
                "float_scrub_max": 1.0,
                "float_scrub_step": 0.1,
                "groups": ["main_toolbar"]
            })

        self.connect_to_ui()

        self.set_attribute_changed_event_handler(self.attribute_changed)

    def attribute_changed(self, attr_uuid, attr_role_index, attr_value):

        print("OIOI -- {0} {1}".format(attr_uuid, attr_value.dump()))
    #def process_playhead_events(self):
    #
    #    self.bitch.write("GRONK\n")
    #    sys.stderr.write("GRONK2\n")
    #    self.connection.process_events()

def create_plugin_instance(connection):
    return MyPlugin(connection)

if __name__=="__main__":

    XSTUDIO = Connection(auto_connect=True)
    mask_plugin(XSTUDIO)
    XSTUDIO.process_events_forever()