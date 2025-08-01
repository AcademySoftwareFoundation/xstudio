# SPDX-License-Identifier: Apache-2.0
from xstudio.api.module import ModuleBase
from xstudio.api.session.playhead import Playhead
from xstudio.core import JsonStore, Uuid
from xstudio.api.auxiliary.helpers import get_event_group
from xstudio.core import spawn_plugin_base_atom, viewport_playhead_atom
from xstudio.core import get_global_playhead_events_atom, show_message_box_atom
import sys
import os
import traceback

try:
    import XStudioExtensions
except:
    XStudioExtensions = None

def make_simple_string(string_in):
    import re
    return re.sub('[^0-9a-zA-Z]+', '_', string_in).lower()

class PluginBase(ModuleBase):

    """Base class for python plugins"""

    def __init__(
            self,
            connection,
            name,
            base_class_name="StandardPlugin",
            qml_folder=""
            ):
        """Create python plugin base class.

        Args:
            name(str): Name of pytho plugin
            base_class_name(str): Name of underlying C++ plugin class. This
        """
        a = connection.request_receive(
            connection.api.plugin_manager.remote,
            spawn_plugin_base_atom(),
            name,
            JsonStore())
        self.uuid = a[1]
        remote = a[0]

        ModuleBase.__init__(self, connection, remote)

        self.name = name
        self.qml_folder = qml_folder
        self.plugin_path = os.path.realpath(
            os.path.dirname(
                sys.modules[self.__module__].__file__
            )
        )

    def add_attribute(
        self,
        attribute_name,
        attribute_value,
        attribute_role_data={},
        register_as_preference=None
    ):
        """Add an attribute to your plugin class. Attributes provide a flexible
        way to store data and/or pass data between your plugin and the xStudio
        UI. An attribute can have multiple 'role' values - each role value can
        be made accessible in the QML layer. See demo plugins for more details.

        Args:
            attribute_name(str): Name of the attribute
            attribute_value(int,float,bool,str, list(str)): The value of the
                attribute
            attribute_role_data(dict): Other role data of the attribute
            register_as_preference(bool): If true the attribute value will be
                recorded in the users preferences data when xstudio closes and 
                the value restored next time xstudio starts up.

        """

        if "qml_code" in attribute_role_data and self.qml_folder:
            attribute_role_data["qml_code"] =\
                "import \"file://{0}/{1}\"\n{2}".format(
                    self.plugin_path,
                    self.qml_folder,
                    attribute_role_data["qml_code"]
                )

        preference_path = None
        if register_as_preference:
            preference_path = "/plugin/" + make_simple_string(self.name) + \
                "/" + make_simple_string(attribute_name)

        return super().add_attribute(
            attribute_name,
            attribute_value,
            attribute_role_data,
            preference_path=preference_path)

    def current_playhead(self):

        gphev = self.connection.request_receive(
            self.connection.remote(),
            get_global_playhead_events_atom())[0]

        return Playhead(self.connection, self.connection.request_receive(
            gphev,
            viewport_playhead_atom())[0])

    def popup_message_box(
        self,
        message_title,
        message_body,
        close_button=True,
        autohide_timeout_secs=0,
        ):
        """Pop-up a simple message box dialog in the xstudio GUI with only
        a 'close' button
        Args:
            message_title(str): This goes in the title bar of the dialog
            message_body(str): The body of the text
            close_button(bool): Add a close button to the box
            autohide_timeout_secs(int): Optional timeout to auto-hide the message box
        """
        app = self.connection.api._app
        cp = self.connection.send(
            app.remote,
            show_message_box_atom(),
            message_title,
            message_body,
            close_button,
            int(autohide_timeout_secs)
            )