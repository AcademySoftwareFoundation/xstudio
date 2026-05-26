from .hud_plugin import HUDPlugin
from xstudio.core import viewport_mask_atom, change_atom, Mask, VectorMask
from xstudio.api.session.media import MediaSource

class MaskPlugin(HUDPlugin):
    """Base class for HUD (Viewport Overlay) plugins."""

    def __init__(
        self,
        connection,
        name,
        qml_folder="",
        position_in_hud_list=0.0,
        per_media_masks=False
    ):
        """Create a plugin.

        Args:
            name(str): Name of Mask plugin - this will appear in the
                HUD toolbar menu.
        """
        HUDPlugin.__init__(
            self, connection, name, qml_folder, position_in_hud_list
        )

        self._per_media_masks = per_media_masks
        self.mask_renderer = connection.api.get_actor_from_registry("MASK_RENDERER")
        connection.send(self.mask_renderer.remote, viewport_mask_atom(), self.uuid, self.name, self.remote, per_media_masks)

    def static_masks(self):
        """Re-implement to return a Mask or list of Masks that should be applied
        to the viewport regardless of what media is on screen."""
        return None

    def media_source_masks(self, media_source):
        """Re-implement to return a Mask or list of Masks that should be applied
        to the viewport for a specific media source. Requires 'per_media_masks' 
        to be set to True in the constructor."""
        return None

    def on_screen_media_changed(self, media_source):
        """Re-implement to receive notification of when the on-screen media changes.
        This will be useful if your mask plugin has attributes that will change
        depending on the media source"""
        pass

    def _media_source_masks(self, media_source, on_screen_media_changed):
        if self.enabled:
            if on_screen_media_changed:
                self.on_screen_media_changed(MediaSource(self.connection, media_source))
                return
            mask_data = self.static_masks() if media_source == None else self.media_source_masks(MediaSource(self.connection, media_source))
            if isinstance(mask_data, VectorMask):
                pass
            elif isinstance(mask_data, Mask):
                mask_data = VectorMask([mask_data])
            elif isinstance(mask_data, list) and all(isinstance(i, Mask) for i in mask_data):
                mask_data = VectorMask(mask_data)
            elif mask_data == None or mask_data == []:
                mask_data = VectorMask([])
            return mask_data
        return VectorMask([])

    def attribute_changed(self, attr, role):

        HUDPlugin.attribute_changed(self, attr, role)
        if self._per_media_masks:
            # tell the mask renderer to request new masks from this plugin
            self.connection.send(
                self.mask_renderer.remote,
                viewport_mask_atom(),
                change_atom(),
                self.uuid)
        else:
            # update static masks
            self.connection.send(
                self.mask_renderer.remote,
                viewport_mask_atom(),
                self.uuid,
                self._media_source_masks(None, False))
