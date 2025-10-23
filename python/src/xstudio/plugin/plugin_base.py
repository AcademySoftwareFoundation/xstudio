# SPDX-License-Identifier: Apache-2.0
from xstudio.api.module import ModuleBase
from xstudio.api.session.playhead import Playhead
from xstudio.core import JsonStore, Uuid, AttributeRole
from xstudio.api.auxiliary.helpers import get_event_group
from xstudio.core import spawn_plugin_base_atom, viewport_playhead_atom
from xstudio.core import get_global_playhead_events_atom, show_message_box_atom
from xstudio.core import plugin_events_group_atom, get_event_group_atom
import sys
import os
import traceback
import pathlib

def make_simple_string(string_in):
    import re
    return re.sub('[^0-9a-zA-Z]+', '_', string_in).lower()

class PluginBase(ModuleBase):

    """Base class for python plugins"""

    def __init__(
            self,
            connection,
            name="GenericPlugin",
            base_class_name="StandardPlugin",
            qml_folder="",
            actor=None
            ):
        """Create python plugin base class.

        Args:
            name(str): Name of pytho plugin
            base_class_name(str): Name of underlying C++ plugin class. This
        """
        if not actor:
            actor = connection.request_receive(
                connection.api.plugin_manager.remote,
                spawn_plugin_base_atom(),
                name,
                JsonStore(),
                base_class_name)[0]

        ModuleBase.__init__(self, connection, actor)

        self.name = name
        self.qml_folder = qml_folder
        self.plugin_path = os.path.realpath(
            os.path.dirname(
                sys.modules[self.__module__].__file__
            )
        )
        self.qml_item_attrs = {}
        self.playhead_subscriptions = []

        self.user_attr_handler_ = None

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

        # if qml code is being added as attribute data and we also have qml_folder
        # set we add an import directive to the qml code with the path to the
        # plugin location. This allows plugins with qml to be relocatable.
        if "qml_code" in attribute_role_data and self.qml_folder:
            uri = pathlib.Path(
                "{0}/{1}".format(
                    self.plugin_path,
                    self.qml_folder)).as_uri()
            attribute_role_data["qml_code"] =\
                "import \"{0}\"\n{1}".format(
                    uri,
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
        app = self.connection.api.app
        cp = self.connection.send(
            app.remote,
            show_message_box_atom(),
            message_title,
            message_body,
            close_button,
            int(autohide_timeout_secs)
            )

    def create_qml_item(
        self,
        qml_item,
        callback_fn=None
        ):
        """Create and show a qml item (typically a pop-up window, dialog etc.).
        Args:
            qml_item(str): The class name for the item to be created
            callback_fn(method): Callback function that receives data from the
            qml item (e.g. text entered by the user). On the qml side the 
            item can trigger the callback by calling xstudio_callback(JSON)
            function somewhere in the qml signal handlers etc.
        Returns:
            item_id(uuid): The id of the attribute that injects the item into 
            the UI. Use this with 'delete_qml_item' to destroy the item.
        """

        attr_name = "QML item {}".format(hash(qml_item))

        try:
            attr = self.get_attribute(attr_name)
        except:
            attr = None

        if not attr:
            attr = self.add_attribute(
                attr_name,
                attribute_value = False,
                attribute_role_data={
                    "qml_code": qml_item,
                    "groups": ["dynamic qml items"],
                    "enabled": False
                })

        if callback_fn:
            self.qml_item_attrs[attr.uuid] = (attr, callback_fn)
        # 'attr_enabled' controls the visibility of the widget
        attr.set_role_data("attr_enabled", True) 
        return attr.uuid

    def delete_qml_item(self, item_uuid):
        """Destroy / Clean up a QML item created with 'create_qml_item'.
        Args:
            item_uuid(uuid): The id of the item created by 'create_qml_item'
        """
        self.remove_attribute(item_uuid)        

    def attribute_changed(
        self,
        attribute,
        role
        ):
        # check if 'CallbackData' has been set - if the attribute is one of our 
        # qml_item_attrs then we execute the associated callback
        if role == AttributeRole.CallbackData and \
            attribute.uuid in self.qml_item_attrs and \
            attribute.role_data("callback_data") != {}:
            self.qml_item_attrs[attribute.uuid][1](attribute.role_data("callback_data"))
            # now reset the callback data so if the callback is called again with
            # the same data as before we still get an 'attribute_changed' signal
            attribute.set_role_data("callback_data", {})

    def get_plugin(
        self,
        plugin_name
        ):
        """Get an/the instance of the named xSTUDIO plugin. If the plugin is 
        'resident' then the singleton instance of the plugin will be returned,
        otherwise a new spawning of the plugin will be returned.

        This can be useful to get to another plugin (e.g. "AnnotationsPlugin")
        to query its attributes, set its attributes and do other interactions
        with it. See api.plugin_manager for more ways to query plugins.

        Args:
            plugin_name(str): The name of the plugin.

        Returns:
            plugin(PluginBase/actor): Returns a PluginBase actor wrapper if the
            plugin itself is derived from the StandardPlugin base class.
            Otherwise an actor handle is returned
        """

        for plugin_detail in self.connection.api.plugin_manager.plugin_detail:
            if plugin_detail.name() == plugin_name:
                plugin_actor = self.connection.api.plugin_manager.get_plugin_instance(plugin_detail.uuid())
                return PluginBase(connection=self.connection, actor=plugin_actor)
        
        raise RuntimeError("No plugin found with name \"{}\"".format(plugin_name))

    def subscribe_to_playhead_events(self, playhead, callback_method, auto_cancel=True):
        """Set-up a subscription to the events of an xstudio playhead. Can
        automatically cancel previous subscription(s)

        Args:
            plugin(actor): The playhead that we want to watch
            callback_method(Callable): The function which will be called
            with playhead events. Must take a single argument (a tuple of the event
            data)
        
        Returns:
            uuid (callback id): A uuid for the subscription. Pass to
            unsubscribe_from_event_group to cancel an event subscription
        """
        event_group = self.connection.request_receive(
            playhead,
            get_event_group_atom())[0]

        if not event_group:
            raise Exception("Actor has no event group.")

        if auto_cancel and self.playhead_subscriptions:
            for sub in self.playhead_subscriptions:
                self.unsubscribe_from_event_group(sub)
            self.playhead_subscriptions = []

        subscription_id = self.connection.link.add_message_callback(
            event_group, callback_method
            )

        self.playhead_subscriptions.append(subscription_id)

        return subscription_id

    def subscribe_to_plugin_events(self, plugin, callback_method):
        """Set-up a subscription to the events of an xstudio plugin.

        Args:
            plugin(PluginBase): The plugin whose event group (plugin events)
            we want to join.
            callback_method(Callable): The function which will be called
            with event. Must take a single argument (a tuple of the event
            data)
        
        Returns:
            uuid (callback id): A uuid for the subscription. Pass to
            unsubscribe_from_event_group to cancel an event subscription
        """
        event_group = self.connection.request_receive(
            plugin.remote,
            plugin_events_group_atom())[0]

        if not event_group:
            raise Exception("Actor has no event group.")

        return self.connection.link.add_message_callback(
            event_group, callback_method
            )

    def register_ui_panel_qml(self,
        panel_name,
        qml_code,
        position_in_menu,
        viewport_popout_button_icon="",
        viewport_popout_button_position=-1.0,
        toggle_hotkey_id=Uuid()):
        """Register a GUI interface as a panel and/or a floating window with a
        button in the viewport button shelf.

        Args:
            panel_name(str): The name of the panel (as displayed in the panels menu list)
            qml_code(str): The qml code for the contents of the panel
            position_in_menu(float): Where the panel entry appears in the panel menu
            viewport_popout_button_icon(str): QML resource string for the icon, if a viewport shelf button is desired
            viewport_popout_button_position(float): Position of button in shelf
            toggle_hotkey_id(Uuid): Optional uuid for a hotkey that shows/hides the floating panel
        Returns:
            None
        """

        from xstudio.core import get_actor_from_registry_atom, insert_rows_atom
        import json

        if self.qml_folder:
            uri = pathlib.Path(
                "{0}/{1}".format(
                    self.plugin_path,
                    self.qml_folder)).as_uri()
            qml_code =\
                "import \"{0}\"\n{1}".format(
                    uri,
                    qml_code
                )

        data = {}
        data["view_name"] = panel_name
        data["view_qml_source"] = qml_code
        data["position"]  = position_in_menu

        central_models_data_actor = self.connection.request_receive(
            self.connection.remote(),
            get_actor_from_registry_atom(),
            "GLOBALUIMODELDATA")[0]

        j = JsonStore()
        j.parse_string(json.dumps(data))

        self.connection.send(
            central_models_data_actor,
            insert_rows_atom(),
            "view widgets",
            "",
            0,
            1,
            j)

        if viewport_popout_button_icon and viewport_popout_button_position != -1.0:

            data = {}
            data["view_name"]         = panel_name
            data["icon_path"]         = viewport_popout_button_icon
            data["view_qml_source"]   = qml_code
            data["button_position"]   = viewport_popout_button_position
            data["window_is_visible"] = False
            data["hotkey_uuid"]       = str(toggle_hotkey_id)
            data["module_uuid"]       = str(self.uuid)

            j.parse_string(json.dumps(data))

            self.connection.send(
                central_models_data_actor,
                insert_rows_atom(),
                "popout windows",
                "",
                0,
                1,
                j)
