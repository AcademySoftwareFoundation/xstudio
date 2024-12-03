from xstudio.plugin.plugin_base import PluginBase
from xstudio.core import hud_settings_atom, HUDElementPosition

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
        toggle_attr = self.get_attribute(name)
        toggle_attr.set_role_data("toolbar_position", position_in_hud_list)

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
        hud_attr = self.get_attribute(self.name)
        
        # if qml code is being added as attribute data and we also have qml_folder
        # set we add an import directive to the qml code with the path to the
        # plugin location. This allows plugins with qml to be relocatable.
        if self.qml_folder:
            qml_code =\
                "import \"file://{0}/{1}\"\n{2}".format(
                    self.plugin_path,
                    self.qml_folder,
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
            qml_code =\
                "import \"file://{0}/{1}\"\n{2}".format(
                    self.plugin_path,
                    self.qml_folder,
                    qml_code
                )
        self.get_attribute(self.name).set_role_data("user_data", qml_code)