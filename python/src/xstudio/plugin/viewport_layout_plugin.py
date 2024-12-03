from xstudio.plugin.plugin_base import PluginBase
from xstudio.core import viewport_layout_atom
from xstudio.core import AssemblyMode, AutoAlignMode
from xstudio.core import JsonStore
import json

class ViewportLayoutPlugin(PluginBase):

    """Base class for Viewport Layout plugins"""

    def __init__(
            self,
            connection,
            name,
            qml_folder="",
            ):
        """Create a plugin 

        Args:
            name(str): Name of Viewport Layout plugin
        """
        PluginBase.__init__(
            self,
            connection,
            name,
            "ViewportLayoutPlugin",
            qml_folder=qml_folder
            )

        # subscribe to the event group of the remote actor (which is a 
        # ViewportLayoutPlugin). 
        self.subscribe_to_event_group(
            self,
            self.layout_callback)

    def add_layout_mode(self, name, playhead_assembly_mode, auto_align_mode=AutoAlignMode.AAM_ALIGN_OFF):
        """Add a viewport layout mode 

        Args:
            name(str): Name of the layout. This must be unique and not one that
            has already been added by another plugin
            playhead_assembly_mode (xstudio.core.AssemblyMode): The playhead 
            assembly mode. Can be AM_STRING for stringing together the selected
            media to play in sequence. AM_ONE means only one image is needed for
            the layout. AM_ALL means all selected media is decoded at the 
            same time and delivered for rendering to the viewport.
        """
        self.connection.send(
                self.remote,
                viewport_layout_atom(),
                name,
                playhead_assembly_mode,
                auto_align_mode)            

    def add_layout_settings_attribute(
        self,
        attr,
        layout_name
        ):
        """Add an attribute to the 'settings' for the lahyout plugin. This means
        the attribute will appear in the settings interface for the plugin
        via the Compare toolbar button.

        Args:
            attr_uuid(ModuleAttribute): The attribute to be exposed in the
                settings dialog.
        """
        attr.expose_in_ui_attrs_group(layout_name + " Settings")

    def do_layout(self, layout_name, image_set_data):
        """Re-implement this function to do your custom layout. You must return
        a tuple of 3 elements:
            0: list of tuples of 3 floats (xtranslate, ytranslate, scale), one for
                each image in image_set_data, whether or not it will be drawn
            1: list if ints (indexes of images that will be drawn)
            2: float (the aspect ratio of the layout)

        Args:
            layout_name(str): Name of the layout.
            image_set_data(json): Information about the image set that needs
            to be laid out. The json is an array, one entry for each image
            to be displayed. Each entry provides the pixels size of the image
            and the pixel aspect ratio.

        Return:
            tuple(list(tuple(float, float, float)), list(int), float): 
        """
        return ([(0.0, 0.5, 0.5) for a in image_set_data["image_info"]], [0], 16.0/9.0)

    def layout_callback(self, args):

        if len(args) == 4 and type(args[0]) == viewport_layout_atom and\
            type(args[1]) == str and type(args[2]) == JsonStore:
            layout = self.do_layout(args[1], json.loads(str(args[2])))
            j = {
                "transforms": layout[0],
                "image_draw_order": layout[1],
                "layout_aspect_ratio": layout[2],
                "hash": args[3]
                }
            # horrible. The has is an int64 ... JsonStore won't convert.
            j = json.loads(json.dumps(j))
            self.connection.send(
                self.remote,
                viewport_layout_atom(),
                args[1],
                JsonStore(j))