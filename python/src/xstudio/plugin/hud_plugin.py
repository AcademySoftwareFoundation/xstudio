from xstudio.plugin.plugin_base import PluginBase
from xstudio.core import hud_settings_atom, HUDElementPosition
from xstudio.core import event_atom, show_atom, media_atom
from xstudio.core import get_global_playhead_events_atom
from xstudio.api.session.media import Media
import pathlib

class HUDPlugin(PluginBase):

    """Base class for HUD (Viewport Overlay) plugins"""

    def __init__(
            self,
            connection,
            name,
            qml_folder="",
            position_in_hud_list=0.0
            ):
        """Create a plugin

        Args:
            name(str): Name of HUD plugin - this will appear in the HUD toolbar menu
        """
        PluginBase.__init__(self, connection, name, "HUDPlugin", qml_folder=qml_folder)

        # base class has added an attribute with the same name as the plugin
        # name. This is how toggles for hud plugins are added to the HUD button
        # in the viewport toolbar. The 'toolbar_position' role data is used
        # to sort the list of HUD items
        self.toggle_attr = self.get_attribute(name)
        self.toggle_attr.set_role_data("toolbar_position", position_in_hud_list)
        self.__enabled = self.toggle_attr.value()

        # here we subscribe to playhead events in order to receive notification
        # of when new media goes on screen
        self.subscribe_to_global_playhead_events(self.__playhead_event_handler)

        self.per_viewport_hud_data_attrs = {}

        self.per_viewport_hud_data = {}

    @property
    def enabled(self):
        return self.__enabled

    def add_hud_settings_attribute(
        self,
        attr
        ):
        """Add an attribute to the 'settings' for the HUD plugin. This means
        the attribute will appear in the settings interface for the plugin
        via the HUD toolbar button.

        Args:
            attr_uuid(ModuleAttribute): The attribute to be exposed in the
                settings dialog.
        """
        attr.expose_in_ui_attrs_group(self.name + " Settings")

    def hud_element_qml(
        self,
        qml_code,
        hud_position=HUDElementPosition.FullScreen
        ):
        """Set the QML code for the visual element of the HUD that appears in
        the xSTUDIO viewport.

        Args:
            qml_code(str): The qml code required to instance a visual element
                that will be displayed over the viewport
            hud_position(HUDElementPosition): The position in the viewport where the visual
                element will be. Must be one of (HUDElementPosition.FullScreen,
                HUDElementPosition.BottomLeft, HUDElementPosition.BottomCentre,
                HUDElementPosition.TopLeft etc.). The FullScreen option requires that
                the visual element manages its own transformation to draw
                itself over the viewport as required.
        """

        # if qml code is being added as attribute data and we also have qml_folder
        # set we add an import directive to the qml code with the path to the
        # plugin location. This allows plugins with qml to be relocatable.
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

        self.connection.send(
                self.remote,
                hud_settings_atom(),
                qml_code,
                hud_position
            )

    def set_custom_settings_qml(self, qml_code):
        """Add qml for a custom dialog to control the settings for a HUD item

        Args:
            qml_code(str): The qml code required to instance a dialog that will
            allow the user to adjust the settings for your HUD item."""

        # Note - HUD Plugins always have an attribute with the same name as
        # the plugin itself. It's this attribute that toggles the HUD item on
        # and off in the HUD toolbar button. We can also add custom qml to this
        # attribute if the plugin author wants to provide a custom settings
        # dialog.
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
        self.get_attribute(self.name).set_role_data("user_data", qml_code)

    def media_item_hud_data_rw(self, media_actor):
        return self.media_item_hud_data(Media(self.connection, media_actor) if media_actor else None)

    def media_item_hud_data(self, media_item = None):
        """This method should be overridden by HUD plugins that inherit from
        this HUDPlugin class. A Media object is passed in when a new media
        item is due to be drawn on screen. At this point, the HUD plugin should
        perform necessary operations to generate the data required to do the
        HUD display in the UI overlay for the given Media item and return it
        as a dictionary or array that can be expressed as json data. xSTUDIO 
        will store the data and make it available to the QML overlay as a 
        special JavaScript variable 'hud_item_data'.

        Args:
            media_item(Media): Media due to go on-screen.
            
        Returns:
            hud data(dict)/array: An array or a dict with string keys with no 
            whitespaces or numbers as first character. Values can be strings, 
            numbers, boolean or uuid or more dictionaries or arrays. """
        return {}

    def attribute_changed(self, attr, role):
        if attr == self.toggle_attr:
            self.__enabled = self.toggle_attr.value()
            if self.__enabled:
                # kick the global playhead events actor to re-broadcast on-screen
                # media. This will return to us below giving us an opportunity to
                # run media_item_hud_data and refresh the HUD graphics
                gphev = self.connection.request_receive(
                    self.connection.remote(),
                    get_global_playhead_events_atom())[0]
                self.connection.send(gphev, media_atom(), True)

        super(HUDPlugin, self).attribute_changed(attr, role)

    def __playhead_event_handler(self, event_args):

        if not self.enabled:
            return
        if (
            len(event_args) == 6
            and isinstance(event_args[0], event_atom)
            and isinstance(event_args[1], show_atom)):

            # this message tells us that new media is going on-screen. We are
            # provided the Media actor, MediaSource actor, viewport name and
            # the sub-playhead index (if we have multiple images on-screen, 
            # like a Grid compare mode, each tile in the layout has a sub-playhead
            # so if we are viewing 4 images in a 2x2 grid, we expect this callback
            # to happen 4 times)

            media_ref = event_args[2].actor
            viewport_name = event_args[4]
            sub_playhead_index = event_args[5]
            if media_ref:

                media = Media(self.connection, media_ref)
                self.set_hud_media_data(viewport_name, sub_playhead_index, self.media_item_hud_data(media))

            else:

                self.set_hud_media_data(viewport_name, sub_playhead_index, self.media_item_hud_data())

    def set_hud_media_data(self, viewport_name, sub_playhead_index, data):

        viewport_image = "{}_{}".format(viewport_name, sub_playhead_index)

        # create an attr for this plugin and for the viewport to expose
        # the data to the QML layer
        if viewport_image not in self.per_viewport_hud_data_attrs:
            # the trick here is that we add the attribute to a specifically named
            # attribute group. We reconstruct the same group name in the QML
            # code so that we can access the attribute data
            self.per_viewport_hud_data_attrs[viewport_image] = self.add_attribute("{}".format(self.name), {})
            self.per_viewport_hud_data_attrs[viewport_image].expose_in_ui_attrs_group("{}_HUD_ATTRS".format(viewport_image))

        # set the attr value
        self.per_viewport_hud_data_attrs[viewport_image].set_value(
            data
            )