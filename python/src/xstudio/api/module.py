# SPDX-License-Identifier: Apache-2.0
from xstudio.core import add_attribute_atom, connect_to_ui_atom, remove_attribute_atom
from xstudio.core import disconnect_from_ui_atom
from xstudio.core import attribute_role_data_atom, change_attribute_event_atom
from xstudio.core import attribute_value_atom, register_hotkey_atom
from xstudio.core import get_global_playhead_events_atom, join_broadcast_atom
from xstudio.core import viewport_playhead_atom, hotkey_event_atom
from xstudio.core import attribute_uuids_atom
from xstudio.core import AttributeRole, get_event_group_atom
from xstudio.core import event_atom, show_atom, module_add_menu_item_atom
from xstudio.core import module_remove_menu_item_atom, uuid_atom
from xstudio.core import menu_node_activated_atom, set_node_data_atom
from xstudio.core import ColourTriplet
from xstudio.api.auxiliary import ActorConnection
from xstudio.core import JsonStore, Uuid
from xstudio.api.auxiliary.helpers import get_event_group
from xstudio.api.session.media import Media, MediaSource
import json
import os
import sys
import traceback

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
            # as yet, I haven't worked out how to allow type conversion from
            # custom python type (like ColourTriplet) and JsonStore !!
            if isinstance(attribute_value, ColourTriplet):
                # backend knows to interpret this back into a ColourTriplet
                attribute_value = ["colour", 1, attribute_value.red, attribute_value.green, attribute_value.blue]

            converted_val = JsonStore()
            if type(attribute_value) == dict or type(attribute_value) == list:
                converted_val.parse_string(json.dumps(attribute_value)) 
            elif type(attribute_value) == type(JsonStore()):
                converted_val = attribute_value                
            else:
                converted_val = JsonStore(attribute_value)                

            self.uuid = connection.request_receive(
                parent_remote,
                add_attribute_atom(),
                attribute_name,
                converted_val,
                JsonStore(attribute_role_data)
            )[0]

    def role_data(self, role_name):

        return json.loads(self.connection.request_receive(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            role_name)[0].dump())

    def expose_in_ui_attrs_group(self, group_name):

        try:
            group_list = self.role_data("groups")
            if type(group_name) == str:
                group_list.append(group_name)
            elif type(group_name) == list:
                group_list = group_list+group_name
        except:
            if type(group_name) == str:
                group_list = [group_name]
            elif type(group_name) == list:
                group_list = group_name

        self.connection.request_receive(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            "groups",
            JsonStore(group_list))

    def set_tool_tip(self, tool_tip):

        self.connection.send(
            self.parent_remote,
            attribute_role_data_atom(),
            self.uuid,
            "tooltip",
            JsonStore(tool_tip))

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

        if isinstance(value, list):
            v = JsonStore()
            v.parse_string(json.dumps(value))
            value = v
        elif not isinstance(value, JsonStore):
            value = JsonStore(value)

        self.connection.send(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid,
            value)

    def add_to_preferences(self):

        self.connection.send(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid)

    def as_json(self):

        return self.connection.request_receive(
            self.parent_remote,
            attribute_value_atom(),
            self.uuid,
            True
            )[0].get()

class ModuleMeta(type):
    def __call__(cls, *args, **kwargs):
        instance = super().__call__(*args, **kwargs)
        instance.setup_message_handler()
        return instance

class ModuleBase(ActorConnection, metaclass=ModuleMeta):

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
        self.attrs_by_uuid_ = {}
        self.menu_trigger_callbacks = {}
        self.menu_item_ids = []
        self.hotkey_callbacks = {}
        self._uuid = None

        for attr_uuid in attr_uuids:
            attr_wrapper = ModuleAttribute(
                connection,
                remote,
                uuid=attr_uuid)
            self.attrs_by_name_[attr_wrapper.name] = attr_wrapper
            self.attrs_by_uuid_[attr_uuid] = attr_wrapper

        self.__attribute_changed = None

    def setup_message_handler(self):

        # this call gets the event group for Module attribute change events
        remote_event_group = self.connection.request_receive(self.remote, get_event_group_atom(), True)[0]

        self.connection.link.add_message_callback(
            remote_event_group, self.message_handler
            )

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

    def get_attribute(
        self,
        attribute_name
        ):
        """Get an attribute with the given name. Throws and exception if the
        attribute does not exist

        Args:
            attribute_name(str): Name of attribute
        Returns:

        """
        if attribute_name in self.attrs_by_name_:
            return self.attrs_by_name_[attribute_name]
        raise Exception("No such attribute {}".format(attribute_name))

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
        self.attrs_by_uuid_[new_attr.uuid] = new_attr

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

    def attribute_changed(self, attr, role):
        """Override this method to get callbacks when an attribute's role
        data has changed

        Args:
            attr(ModuleAttribute): The attribute that has changed
            role(int): The index for the role data that has changed
        """
        pass

    def subscribe_to_playhead_events(self, playhead_event_callback):
        """Set the callback function for receiving events specific to
        the playhead and subscrive to the playheads events broadcast
        group.

        Args:
            playhead_event_callback(Callable): Call back function
        """

        gphev = self.connection.request_receive(
            self.connection.remote(),
            get_global_playhead_events_atom())[0]

        self.connection.link.add_message_callback(
            gphev, playhead_event_callback
            )

    def subscribe_to_event_group(self, event_source, callback_method):
        """Set the callback function for receiving events specific to
        the playhead and subscrive to the playheads events broadcast
        group.

        Args:
            event_source(ActorConnection): The actor whose event group
            we want to join.
            callback_method(Callable): The function which will be called
            with event. Must take a single argument (a tuple of the event
            data)
        """

        event_group = self.connection.request_receive(
            event_source.remote,
            get_event_group_atom())[0]

        if not event_group:
            raise Exception("Actor has no event group.")

        self.connection.link.add_message_callback(
            event_group, callback_method
            )

    def menu_item_activated(self, menu_item_data, user_data):
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
            component)[0]

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

        self.menu_item_ids.append(menu_item_uuid)

        self.menu_trigger_callbacks[
            menu_item_uuid] = menu_trigger_callback

    def insert_menu_item(
        self,
        menu_model_name,
        menu_text,
        menu_path,
        menu_item_position,
        attr_id=Uuid(),
        divider=False,
        hotkey_uuid=Uuid(),
        callback=None,
        user_data=""
    ):

        menu_item_uuid = self.connection.request_receive(
            self.remote,
            module_add_menu_item_atom(),
            menu_model_name,
            menu_text,
            menu_path,
            menu_item_position,
            attr_id,
            divider,
            hotkey_uuid,
            user_data)[0]

        self.menu_item_ids.append(menu_item_uuid)

        if callback:
            self.menu_trigger_callbacks[
                menu_item_uuid] = callback

        return menu_item_uuid

    def insert_hotkey_into_menu(
        self,
        menu_model_name,
        menu_path,
        menu_item_position,
        hotkey_uuid
    ):

        menu_item_uuid = self.connection.request_receive(
            self.remote,
            module_add_menu_item_atom(),
            menu_model_name,
            menu_path,
            menu_item_position,
            hotkey_uuid)[0]

        self.menu_item_ids.append(menu_item_uuid)

        return menu_item_uuid

    def set_submenu_position(
        self,
        menu_model_name,
        submenu_path,
        menu_item_position
    ):
        """When adding menu items that are placed under a submenu, the position
        of the submenu in the parent menu is not set by default. Use this method
        to ensure the submenu appears in the parent menu at the position you
        prefer

        Args:
            menu_model_name(str): The name of the menu model
            submenu_path(str): The submenu name (preceded by parent submenus
                separated by a pipe symbol e.g. 'Publish|Notes' if you wish to
                set the position of the Notes submenu within the Publish submenu.
            menu_item_position(float): The relative position in the parent menu.
        """

        self.connection.request_receive(
            self.remote,
            set_node_data_atom(),
            menu_model_name,
            submenu_path,
            menu_item_position)

    def remove_menu_item(
        self,
        menu_uuid
    ):
        """Remove a menu item from the xSTUDIO menu models

        Args:
            menu_uuid(Uuid): Id of menu item

        Returns:
            None
        """

        self.connection.request_receive(
            self.remote,
            module_remove_menu_item_atom(),
            menu_uuid)

    def remove_menu_items(
        self,
        menu_uuids
    ):
        """Remove menu items from the xSTUDIO menu models

        Args:
            menu_uuid(list(Uuid)): list of ids of menu items

        Returns:
            None
        """

        for menu_uuid in menu_uuids:
            self.connection.request_receive(
                self.remote,
                module_remove_menu_item_atom(),
                menu_uuid)

    def message_handler(self, message_content):

        try:
            atom = message_content[0] if len(message_content) else None

            # message handler for attribute change
            if isinstance(atom, type(change_attribute_event_atom())):
                role = message_content[3]
                attr_uuid = message_content[2] if len(
                    message_content) > 2 else Uuid()
                if attr_uuid in self.attrs_by_uuid_:
                    self.attribute_changed(self.attrs_by_uuid_[attr_uuid], role)
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

            elif isinstance(atom, type(menu_node_activated_atom())):
                menu_item_data = message_content[1].get() if len(
                    message_content) > 1 else None
                user_data = str(message_content[2]) if len(
                    message_content) > 2 else ""
                if "uuid" in menu_item_data:
                    uuid = Uuid(menu_item_data["uuid"])
                    if uuid in self.menu_item_ids:
                        self.menu_item_activated(menu_item_data, user_data)
                    if uuid in self.menu_trigger_callbacks:
                        self.menu_trigger_callbacks[uuid]()

        except Exception as err:
            print (err)
            print (traceback.format_exc())

    @property
    def uuid(self):
        """Get Module uuid

        Returns:
            uuid(Uuid): Uuid of actor container.
        """
        if not self._uuid:
            self._uuid = self.connection.request_receive(
                self.remote,
                uuid_atom())[0]
        return self._uuid
