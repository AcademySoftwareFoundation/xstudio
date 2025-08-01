# SPDX-License-Identifier: Apache-2.0
from xstudio.core import add_attribute_atom, connect_to_ui_atom
from xstudio.core import disconnect_from_ui_atom
from xstudio.core import attribute_role_data_atom, change_attribute_event_atom
from xstudio.core import attribute_value_atom, register_hotkey_atom
from xstudio.core import get_global_playhead_events_atom, join_broadcast_atom
from xstudio.core import viewport_playhead_atom, hotkey_event_atom
from xstudio.core import attribute_uuids_atom, request_full_attributes_description_atom
from xstudio.core import AttributeRole, remove_attribute_atom
from xstudio.api.auxiliary import ActorConnection
from xstudio.core import JsonStore, Uuid
from xstudio.api.auxiliary.helpers import get_event_group
import sys
import os
import traceback

try:
    import XStudioExtensions
except:
    XStudioExtensions = None


class ModuleAttribute:

    """Simple wrapper providing attribute object interface"""

    def __init__(
        self,
        connection,
        parent_remote,
        attribute_name=None,
        attribute_value=None,
        attribute_role_data={},
        uuid=None
    ):
        """Create python module base class.

        Args:
            connection(Connection): xStudio API connection object
            parent_remote(actor): The parent actor - must derived from Module
        """
        self.connection = connection
        self.parent_remote = parent_remote

        if uuid:
            self.uuid = uuid
        else:
            self.uuid = connection.request_receive(
                parent_remote,
                add_attribute_atom(),
                attribute_name,
                attribute_value if type(attribute_value) == type(JsonStore()) else JsonStore(attribute_value),
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

    @property
    def name(self):

        return self.connection.request_receive(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid,
            int(AttributeRole.Title)
            )[0].get()

    def set_value(self, value):

        self.connection.send(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid,
            JsonStore(value))

    def set_role_data(self, role_name, data):

        r = self.connection.request_receive(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            role_name,
            JsonStore(data))[0]
        if r != True:
            raise Exception("set_role_data with rolename: {0}, data: {1} failed with error {2}:",
                role_name,
                data,
                r)


class ModuleBase(ActorConnection):

    """Wrapper for Module class instances"""

    def __init__(
            self,
            connection,
            remote
            ):
        """Create python module base class.

        Args:
            connection(Connection): The global API connection object
            remote(Actor): The backend Module 'actor' that we wish to wrap.
        """

        ActorConnection.__init__(self, connection, remote)

        attr_uuids = self.connection.request_receive(
            self.remote,
            attribute_uuids_atom())[0]

        self.attrs_by_name_ = {}
        self.menu_trigger_callbacks = {}
        self.hotkey_callbacks = {}

        for attr_uuid in attr_uuids:
            attr_wrapper = ModuleAttribute(
                connection,
                remote,
                uuid=attr_uuid)
            self.attrs_by_name_[attr_wrapper.name] = attr_wrapper

        remote_event_group = get_event_group(self.connection, remote)
        if XStudioExtensions:
            XStudioExtensions.add_message_callback(
                (remote_event_group, self.incoming_msg)
            )
        else:
            connection.add_handler(remote, self.message_handler)

        self.__attribute_changed = None
        self.__playhead_event_callback = None

    def connect_to_ui(self):
        """Call this method to 'activate' the plugin and forward live data about
        the attributes to the UI layer. QML code that builds widgets and so-on
        based on the state of plugin attributes will be executed and updated
        in real time.
        """
        self.connection.request_receive(
            self.remote,
            connect_to_ui_atom())

    def disconnect_from_ui(self):
        """Call this method to de-activate the plugin and hide or stop updating
        any UI elemnts that were created by the plugin"""

        self.connection.request_receive(
            self.remote,
            disconnect_from_ui_atom())

    def add_attribute(
        self,
        attribute_name,
        attribute_value,
        attribute_role_data={},
        preference_path=None
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
            preference_path(str): If provided the attribute value will be
                recorded in the users preferences data when xstudio closes and 
                the value restored next time xstudio starts up.
        """

        new_attr = ModuleAttribute(
            self.connection,
            self.remote,
            attribute_name,
            attribute_value,
            attribute_role_data)

        self.attrs_by_name_[attribute_name] = new_attr

        if preference_path:
            new_attr.set_role_data("preference_path", preference_path)

        return new_attr

    def remove_attribute(
        self,
        attribute_uuid
    ):
        """Remove (and delete) an attribute from your plugin class..

        Args:
            attribute_uuid(Uuid): Uuid of the attribute to be removed
        """
        return self.connection.request_receive(
                self.remote,
                remove_attribute_atom(),
                attribute_uuid
            )[0]

    def set_attribute(self, attr_name, value):
        """Set the value on the named attribute

        Args:
            attr_name(str): Name of attribute
            value: Value to set
        """
        if attr_name in self.attrs_by_name_:
            self.attrs_by_name_[attr_name].set_value(value)
        else:
            raise Exception("No attribute named {0}".format(attr_name))

    def attributes_digest(self, verbose=False):
        """Get a summary of the attributes that are part of this Module. Use
        verbose=True to get a full description as a JsonStore object. Otherwise
        a list of attribute names is returned.

        Args:
            verbose(bool): Set True to return a full JsonStore dictionary of 
            attributes and their role data.

        Returns:
            attrs_description(list(str) or JsonStore)
        """
        if verbose:
            return self.connection.request_receive(self.remote, request_full_attributes_description_atom())[0]
        else:
            return self.attrs_by_name_.keys()

    def set_attribute_changed_event_handler(self, handler):
        """Set the callback function that receives events from
        the xSTUDIO messaging system

        Args:
            handler(Callable): Call back function
        """
        self.__attribute_changed = handler

    def subscribe_to_playhead_events(self, playhead_event_callback):
        """Set the callback function for receiving events specific to
        the playhead and subscrive to the playheads events broadcast
        group.

        Args:
            playhead_event_callback(Callable): Call back function
        """
        if XStudioExtensions:

            gphev = self.connection.request_receive(
                self.connection.remote(),
                get_global_playhead_events_atom())[0]

            playhead_event_group = get_event_group(self.connection, gphev)

            XStudioExtensions.add_message_callback(
                (gphev, self.__playhead_events)
            )

            self.__playhead_event_callback = playhead_event_callback

    def __playhead_events(self, *args):

        try:
            message_content = self.connection.caf_message_to_tuple(args[0][0])
            self.__playhead_event_callback(message_content)
        except Exception as e:
            pass

    def register_hotkey(self,
                        hotkey_callback,
                        default_keycode,
                        default_modifier,
                        hotkey_name,
                        description,
                        auto_repeat,
                        component,
                        context):
        """Register a hotkey with xSTUDIO's central hotkey manager.

        Args:
            hotkey_callback(Callable): Call back function that is executed
            when the hotkey is pressed
            default_keycode(str): Hotkey key (e.g. 'D' or 'Backspace')
            default_modifier(xstudio.core import KeyboardModifier): Specify
            a modifier key (e.g. KeyboardModifier.ShiftModifier)
            hotkey_name(str): A unique name for the hotkey
            description(str): A full description of what the hotkey does
            auto_repeat(bool): Should the callback be executed on key
                autorepeats?
            component(str): The xSTUDIO component that your hotkey came from.
                Use your plugin name here, for example.
            context(str): When a hotkey is pressed, the name of the UI element
                that was active when the key is pressed is noted as the
                'context'. This might be 'viewport' or 'popout_viewport' for
                example. Your callback is executed only if your context string
                supplied here matches the context of the key press. Use 'any'
                to ensure your hokey callback is executed regardless.
        """

        hotkey_uuid = self.connection.request_receive(
            self.remote,
            register_hotkey_atom(),
            ord(default_keycode) if len(
                default_keycode) == 1 else default_keycode,
            int(default_modifier),
            hotkey_name,
            description,
            auto_repeat,
            component,
            context)[0]

        self.hotkey_callbacks[str(hotkey_uuid)] = hotkey_callback
        return hotkey_uuid

    def add_menu_item(
        self,
        menu_item_name,
        menu_path,
        menu_trigger_callback,
        hotkey_uuid=Uuid()
    ):

        role_data = {
            "menu_paths": [menu_path]
        }

        if not hotkey_uuid.is_null():
            role_data["hotkey_uuid"] = str(hotkey_uuid)

        menu_item_uuid = self.connection.request_receive(
            self.remote,
            add_attribute_atom(),
            menu_item_name,
            JsonStore(),
            JsonStore(role_data),
        )[0]

        self.menu_trigger_callbacks[
            str(menu_item_uuid)] = menu_trigger_callback

    def message_handler(self, sender, req_id, message_content):

        try:
            atom = message_content[0] if len(message_content) else None
            if isinstance(atom, type(change_attribute_event_atom())):
                role = message_content[3]
                if role == AttributeRole.Value:
                    attr_uuid = str(message_content[2]) if len(
                        message_content) > 2 else ""
                    if self.__attribute_changed:                        
                        for attr in self.attrs_by_name_.values():
                            if attr.uuid == Uuid(attr_uuid):
                                self.__attribute_changed(attr)
                    if attr_uuid in self.menu_trigger_callbacks:
                        self.menu_trigger_callbacks[attr_uuid]()

            elif isinstance(atom, type(hotkey_event_atom())):
                hotkey_uuid = str(message_content[1]) if len(
                    message_content) > 1 else ""                
                activated = bool(message_content[2]) if len(
                    message_content) > 2 else False
                context = str(message_content[3]) if len(
                    message_content) > 3 else ""                
                if hotkey_uuid in self.hotkey_callbacks:
                    self.hotkey_callbacks[hotkey_uuid](activated, context)
                
        except Exception as err:
            print (err)
            print (traceback.format_exc())

    def incoming_msg(self, *args):

        try:
            self.message_handler(
                None,
                None,
                self.connection.caf_message_to_tuple(args[0][0])
            )
        except Exception as err:
            pass
