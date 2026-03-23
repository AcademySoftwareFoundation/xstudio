#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from xstudio.core import AttributeRole, Uuid
from xstudio.plugin import PluginBase

# This plugin is intended to allow for OCIO helper operations that can be
# executed in Python rather than C++. This is useful for operations that
# require access to Python libraries as well as an alternative path to
# implementing feature requests that may be easier to do in Python than C++.

# Code from src/plugin/media_reader/ffmpeg/src/ffmpeg_stream.cpp
# Matrix generated with colour science for Python 0.3.15
# BT.601 (also see Poynton Digital Video and HD 2012 Eq 29.4)
YCBCR_TO_RGB_601 = np.array([
    [1., 0., 1.402],
    [1., -0.34413629, -0.71413629],
    [1., 1.772, -0.]
])
# BT.2020
YCBCR_TO_RGB_2020 = np.array([
    [1., -0., 1.4746],
    [1., -0.16455313, -0.57135313],
    [1., 1.8814, 0.]
])
# BT.709 (also see Poynton Digital Video and HD 2012 Eq 30.4)
YCBCR_TO_RGB_709 = np.array([
    [1., 0., 1.5748],
    [1., -0.18732427, -0.46812427],
    [1., 1.8556, 0.]
])


class OCIOPluginPython(PluginBase):
    """Python plugin for OCIO colour management operations."""

    def __init__(self, connection):
        """Initialize the OCIO plugin.

        Create menu items for YUV range and matrix overrides
        that are only enabled when a YUV media is selected.
        """
        PluginBase.__init__(
            self,
            connection,
            name="OCIOPluginPython"
        )

        # TODO: Changing here mess up the pixel probe
        # TODO: qrc:/menus/XsMenuMultiChoice.qml:107:17:
        # Unable to assign [undefined] to bool error when
        # changing Auto labels

        # Range setting

        self.range_attr = self.add_attribute(
            attribute_name="Range",
            attribute_value="Auto",
            attribute_role_data={
                "combo_box_options": ["Auto", "Full", "Limited"],
                "combo_box_options_enabled": [True, True, True],
            }
        )

        self.range_menu_id = self.insert_menu_item(
            menu_model_name="media_list_menu_",
            menu_text="Set YUV Range",
            menu_path="Media Settings",
            menu_item_position=1.0,
            attr_id=self.range_attr.uuid,
        )

        # Matrix setting

        self.matrix_attr = self.add_attribute(
            attribute_name="Matrix",
            attribute_value="Auto",
            attribute_role_data={
                "combo_box_options": ["Auto", "BT.601", "BT.709", "BT.2020"],
                "combo_box_options_enabled": [True, True, True, True],
            }
        )

        self.matrix_menu_id = self.insert_menu_item(
            menu_model_name="media_list_menu_",
            menu_text="Set YUV Matrix",
            menu_path="Media Settings",
            menu_item_position=2.0,
            attr_id=self.matrix_attr.uuid,
        )

        self.connect_to_ui()

    def menu_item_shown(self, menu_item_data, user_data):
        """Handle menu item shown event to update menu state.

        When the media selected is not in YUV format, the
        menu items for range and matrix are disabled. When
        the media selected is in YUV format, the menu items
        for range and matrix are enabled and the "Auto"
        option is updated to reflect the current metadata
        of the media.
        """
        shown_uuid = Uuid(menu_item_data["uuid"])

        # Always base the Auto label off the main selection
        media = self.connection.api.session.selected_media[0]
        media_metadata = self._yuv_media_metadata(media)
        plugin_metadata = self._plugin_metadata(media)

        all_yuv = all(
            self._media_is_yuv(m)
            for m in self.connection.api.session.selected_media
        )

        # Only enable menu when all selected media are "YUV" format
        if not all_yuv:
            self.range_attr.set_role_data(
                "combo_box_options_enabled",
                [False, False, False],
            )
            self.matrix_attr.set_role_data(
                "combo_box_options_enabled",
                [False, False, False, False],
            )
            return
        else:
            self.range_attr.set_role_data(
                "combo_box_options_enabled",
                [True, True, True],
            )
            self.matrix_attr.set_role_data(
                "combo_box_options_enabled",
                [True, True, True, True],
            )

        # Update auto labels when menu is shown based on the current media metadata
        if shown_uuid == self.range_attr.uuid:
            choices = self.range_attr.role_data("combo_box_options")
            choices[0] = "Auto ({})".format(
                self._ffmpeg_to_range(
                    media_metadata["color_range"]
                )
            )
            self.range_attr.set_role_data(
                "combo_box_options", choices
            )

            # If previous value was "Auto", we need to update to the new name
            if not plugin_metadata.get("range_override"):
                self.range_attr.set_value(choices[0])

        elif shown_uuid == self.matrix_attr.uuid:
            choices = self.matrix_attr.role_data("combo_box_options")
            choices[0] = "Auto ({})".format(
                self._ffmpeg_to_matrix(
                    media_metadata["color_space"]
                )
            )
            self.matrix_attr.set_role_data(
                "combo_box_options", choices
            )

            # If previous value was "Auto", we need to update to the new name
            if not plugin_metadata.get("matrix_override"):
                self.matrix_attr.set_value(choices[0])

    def attribute_changed(self, attribute, role):
        """Handle attribute changes.

        * Ignore changes that are not value changes.
        * Support batch changes when multiple media are selected
        """
        if role != AttributeRole.Value:
            return

        medias = self.connection.api.session.selected_media

        if attribute == self.range_attr:
            range_value = self.range_attr.value()

            for media in medias:
                if self._media_is_yuv(media):
                    # Update the plugin metadata
                    plugin_metadata = self._plugin_metadata(media)
                    if range_value.startswith("Auto"):
                        plugin_metadata.pop("range_override", None)
                    else:
                        plugin_metadata["range_override"] = range_value
                    self._set_plugin_metadata(media, plugin_metadata)

                    # Update the shader overrides based on the new settings
                    self._update_yuv_media_conversion(media)

        elif attribute == self.matrix_attr:
            matrix = self.matrix_attr.value()

            for media in medias:
                if self._media_is_yuv(media):
                    # Update the plugin metadata
                    plugin_metadata = self._plugin_metadata(media)
                    if matrix.startswith("Auto"):
                        plugin_metadata.pop("matrix_override", None)
                    else:
                        plugin_metadata["matrix_override"] = matrix
                    self._set_plugin_metadata(media, plugin_metadata)

                    # Update the shader overrides based on the new settings
                    self._update_yuv_media_conversion(media)

    def _update_yuv_media_conversion(self, media):
        media_metadata = self._yuv_media_metadata(media)
        plugin_metadata = self._plugin_metadata(media)

        # Determine range and matrix settings for the media
        has_range_override = False
        if "range_override" in plugin_metadata:
            color_range = self._range_to_ffmpeg(
                plugin_metadata["range_override"],
                media_metadata,
            )
            has_range_override = True
        else:
            color_range = media_metadata["color_range"]

        has_matrix_override = False
        if "matrix_override" in plugin_metadata:
            matrix = self._matrix_to_ffmpeg(
                plugin_metadata["matrix_override"],
                media_metadata,
            )
            has_matrix_override = True
        else:
            matrix = media_metadata["color_space"]

        # Update shader overrides to adjust YUV to RGB conversion
        if has_range_override or has_matrix_override:
            shader_params = self._yuv_shader_params(
                media_metadata["bitdepth"],
                color_range,
                matrix,
            )
            media.media_source().set_metadata(
                shader_params,
                "/colour_pipeline/shader_overrides",
            )
        else:
            media.media_source().set_metadata(
                {}, "/colour_pipeline/shader_overrides"
            )

    def _plugin_metadata(self, media):
        metadata = media.media_source().get_metadata(
            "/colour_pipeline"
        )
        return metadata.get("ocio_py_plugin", {})

    def _set_plugin_metadata(self, media, plugin_metadata):
        media.media_source().set_metadata(
            plugin_metadata,
            "/colour_pipeline/ocio_py_plugin",
        )

    def _yuv_media_metadata(self, media):
        """Extract YUV related metadata from the media. This 
        metadata is injected by the ffprobe metadata reader and is only
        preset for YUV encoded media. For non-YUV media, our try
        except will fail and we will return default metadata values."""
        
        meta = {
            "pix_fmt": None,
            "bitdepth": None,
            "color_range": "tv",
            "color_space": "bt601"
        }
        try:
            img_stream = media.media_source().image_stream
            meta["pix_fmt"] = img_stream.get(
                "/metadata/stream/@/pix_fmt"
            ).get()
            meta["bitdepth"] = img_stream.get(
                "/metadata/stream/@/bits_per_raw_sample"
            ).get()
            meta["color_range"] = img_stream.get(
                "/metadata/stream/@/color_range"
            ).get()
            meta["color_space"] = img_stream.get(
                "/metadata/stream/@/color_space"
            ).get()

            # Default settings when metadata is missing
            if not meta["color_range"]:
                meta["color_range"] = "tv"
            if not meta["color_space"]:
                meta["color_space"] = "bt601"

        except Exception:
            pass

        return meta

    def _yuv_shader_params(self, bitdepth, color_range, matrix):
        params = {}

        # YCbCr to RGB matrix and offset
        if matrix == "bt2020":
            yuv_to_rgb = YCBCR_TO_RGB_2020.copy()
        elif matrix == "bt709":
            yuv_to_rgb = YCBCR_TO_RGB_709.copy()
        else:
            yuv_to_rgb = YCBCR_TO_RGB_601.copy()

        if color_range == "pc":
            offset = np.array([0.0, 128.0, 128.0])
            offset *= np.power(2.0, float(bitdepth - 8))
            params["yuv_offsets"] = [
                "ivec3", 1, offset[0], offset[1], offset[2]
            ]
        else:
            limits = np.array([16.0, 235.0, 16.0, 240.0])
            limits *= np.power(2.0, float(bitdepth - 8))

            max_cv = np.floor(np.power(2, bitdepth) - 1)

            scale = np.zeros((3, 3))
            scale[0][0] = 1.0 * max_cv / (limits[1] - limits[0])
            scale[1][1] = 1.0 * max_cv / (limits[3] - limits[2])
            scale[2][2] = 1.0 * max_cv / (limits[3] - limits[2])
            yuv_to_rgb = yuv_to_rgb @ scale

            offset = np.array([16.0, 128.0, 128.0])
            offset *= np.power(2.0, float(bitdepth - 8))
            params["yuv_offsets"] = [
                "ivec3", 1, offset[0], offset[1], offset[2]
            ]

        params["yuv_conv"] = [
            "mat3", 1, *yuv_to_rgb.flatten().tolist()
        ]

        return params

    def _range_to_ffmpeg(self, name, metadata):
        if name.startswith("Auto"):
            return metadata["color_range"]
        elif name == "Limited":
            return "tv"
        elif name == "Full":
            return "pc"

    def _ffmpeg_to_range(self, name):
        if name == "tv":
            return "Limited"
        elif name == "pc":
            return "Full"
        else:
            return "Unknown"

    def _matrix_to_ffmpeg(self, name, metadata):
        if name.startswith("Auto"):
            return metadata["color_space"]
        elif name == "BT.601":
            return "bt601"
        elif name == "BT.709":
            return "bt709"
        elif name == "BT.2020":
            return "bt2020"

    def _ffmpeg_to_matrix(self, name):
        if name == "bt601":
            return "BT.601"
        elif name == "bt709":
            return "BT.709"
        elif name == "bt2020":
            return "BT.2020"
        else:
            return "Unknown"

    def _media_is_yuv(self, media):
        metadata = self._yuv_media_metadata(media)
        pix_fmt = metadata.get("pix_fmt", "")
        return pix_fmt and pix_fmt.startswith("yuv")


def create_plugin_instance(connection):
    """Create and return a plugin instance."""
    return OCIOPluginPython(connection)
