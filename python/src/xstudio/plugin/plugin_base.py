# SPDX-License-Identifier: Apache-2.0
from xstudio.core import spawn_plugin_base_atom
from xstudio.core import add_attribute_atom, connect_to_ui_atom, disconnect_from_ui_atom
from xstudio.core import attribute_role_data_atom, change_attribute_event_atom
from xstudio.core import attribute_value_atom
from xstudio.api.auxiliary import ActorConnection
from xstudio.core import JsonStore
import sys

try:
    import XStudioExtensions
except:
    XStudioExtensions = None

class PluginAttribute:

    """Simple wrapper providing attribute object interface"""
    def __init__(
        self,
        connection,
        parent_remote,
        attribute_name,
        attribute_value,
        attribute_role_data = {}
        ):

        """Create python plugin base class.

        Args:
            connection(Connection): xStudio API connection object
            parent_remote(actor): The parent actor - must derived from Module 
        """       
        self.connection = connection
        self.parent_remote = parent_remote

        self.uuid = connection.request_receive(
            parent_remote,
            add_attribute_atom(),
            attribute_name,
            JsonStore(attribute_value),
            JsonStore(attribute_role_data)
        )[0]

    def expose_in_ui_attrs_group(self, group_name):

        self.connection.send(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            "groups",
            JsonStore(group_name))

    def set_tool_tip(self, tool_tip):

        self.connection.send(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            "tooltip",
            JsonStore(tool_tip))

    def set_redraw_viewport_on_change(self, *args):

       pass

    def value(self):

        return self.connection.request_receive(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid)[0].get()

    def set_value(self, value):

        self.connection.send(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid,
            JsonStore(value))


class PluginBase(ActorConnection):

    """Base class for python plugins"""

    def __init__(self, connection, name, base_class_name="StandardPlugin"):
        """Create python plugin base class.

        Args:
            name(str): Name of pytho plugin
            base_class_name(str): Name of underlying C++ plugin class. This 
        """        
        a = connection.request_receive(
            connection.api.plugin_manager.remote,
            spawn_plugin_base_atom(),
            name,
            base_class_name)
        self.uuid = a[1]
        remote = a[0]

        ActorConnection.__init__(self, connection, remote)

        self.attributes = {}

        if XStudioExtensions:
            XStudioExtensions.add_message_callback(
                (self.remote, self.incoming_msg, name)
                )

        self.__attribute_changed = None


    def connect_to_ui(self):
        
        self.connection.request_receive(
            self.remote,
            connect_to_ui_atom())


    def disconnect_from_ui(self):
        
        self.connection.request_receive(
            self.remote,
            disconnect_from_ui_atom())

    
    def add_attribute(
        self,
        attribute_name,
        attribute_value,
        attribute_role_data = {}
    ):
        """Add an attribute to your plugin class. Attributes provide a flexible 
        way to store data and/or pass data between your plugin and the xStudio
        UI. An attribute can have multiple 'role' values - each role value can
        be made accessible in the QML layer. See demo plugins for more details.

        Args:
            attribute_name(str): Name of the attribute
            attribute_value(int,float,bool,str, list(str)): The value of the attribute
            attribute_role_data(dict): Other role data of the attribute
        """

        new_attr = PluginAttribute(
            self.connection,
            self.remote,
            attribute_name,
            attribute_value,
            attribute_role_data)

        self.attributes[str(new_attr.uuid)] = new_attr

        return new_attr

    def set_attribute_changed_event_handler(self, handler):

        self.__attribute_changed = handler

    def incoming_msg(self, *args):

        message_content = self.connection.caf_message_to_tuple(args[0][0])
        try:
            if not self.__attribute_changed or len(message_content) < 4:
                return
            if type(message_content[0]) == type(change_attribute_event_atom()):
                if str(message_content[2]) in self.attributes:
                    self.__attribute_changed(
                        self.attributes[str(message_content[2])]
                        )
        except Exception as err:
            print (err)
