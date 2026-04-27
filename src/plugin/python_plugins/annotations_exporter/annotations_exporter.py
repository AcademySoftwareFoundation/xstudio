#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.plugin import PluginBase
from enum import Enum
import os
import shutil
from urllib.parse import urlsplit
from pathlib import Path
import re

# path to QML resources relative to this .py file
qml_folder_name = "qml/AnnotationsExporter.1"

# QML code necessary to create the overlay item that is drawn over the xSTUDIO
# viewport.
dialog_qml = """
AnnotationsExportDialog {
}
"""

scope_tooltip = """Choose whether to export an image for the current on-screen
annotation only, or all of the annotations on the media that is on-screen, or 
all of the annotations in the currently selected Playlist, Subset or Timeline, 
or all annotations in the entire session."""

mode_tooltip = """Choose 'Annotated Images - Use Media Name and Frame' to export
an image named acoording to the annotated media item and numbered
according to the corresponding media frame number. 

Select 'Annotated Images - Use Your Own Name' to export images with your own
custom naming. The exported images will be numbered sequentially starting from 1.

Select 'Greasepencil Package' to export a bundle of PNG images that only contain
the annotation brush strokes (with opacity) and an xml manifest that can be
imported into some DCCs such as Maya or Houdini as a viewport overlay."""

class ExportType(Enum):
    MEDIA_NAMED_IMAGES = 1
    USER_NAMED_IMAEGS = 2
    GREASEPENCIL = 3

# Declare our plugin class - we're using the PluginBase base class here.
class AnnotationsExporter(PluginBase):

    def __init__(self, connection):

        PluginBase.__init__(
            self,
            connection,
            name="AnnotationsExporter",
            qml_folder=qml_folder_name)

        self.menu_id = self.insert_menu_item(
            "main menu bar",
            "Annotations ...",
            "File|Export",
            0.5,
            callback=self.menu_callback)

        # Create attrs to drive the combo-boxes. This isn't required, we could
        # define the options in QML, but doing it here means we can store the
        # user's selections as preferences so that the dialog 'remembers' their
        # previous settings.

        self.scope = self.add_attribute(
            "Scope",
            "Current Media",
            {
                "combo_box_options": [
                    "Current Frame",
                    "Current Media",
                    "Current Playlist / Timeline",
                    "Entire Session"
                    ]
            },
            register_as_preference=True 
            )
        self.scope.set_tool_tip(scope_tooltip)
        self.scope.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.user_name = self.add_attribute(
            "User Name",
            "annotation_export")
        self.user_name.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.export_type = self.add_attribute(
            "Export Mode",
            "Annotated Images - Use Media Name and Frame",
            {
                "combo_box_options": [
                    "Annotated Images - Use Media Name and Frame",
                    "Annotated Images - Use Your Own Name",
                    "Greasepencil Package"
                    ]
            },
            register_as_preference=True 
            )
        self.export_type.set_tool_tip(mode_tooltip)
        self.export_type.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.file_type = self.add_attribute(
            "File Type",
            "PNG",
            {
                "combo_box_options": ["PNG", "JPEG", "TIFF", "EXR"]
            },
            register_as_preference=True 
            )
        self.file_type.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.resolution = self.add_attribute(
            "Resolution",
            "Match Media Resolution",
            register_as_preference=True 
            )
        self.resolution.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.resolution_options = self.add_attribute(
            "Resolution Options",
            ["Match Media Resolution", "1920x1080", "2048x1080", "3840x2160"],
            register_as_preference=True 
            )
        self.resolution_options.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )
        
        self.output_folder = self.add_attribute(
            "Output Folder",
            "",
            register_as_preference=True 
            )
        self.output_folder.expose_in_ui_attrs_group(
            "annotation_export_attrs"
            )

        self.__output_folder = None
        self.__export_type = None
        self.subfolder = None
        self.exported_images = []
        self.export_width = -1
        self.export_height = -1

        # expose our attributes in the UI layer
        self.connect_to_ui()

    def attribute_changed(self, attr, role):

        if attr == self.scope:
            if self.scope.value() in ["Current Media", "Current Frame"]:
                self.user_name.set_value(
                    self.current_playhead().on_screen_media.name
                )
            elif self.scope.value() == "Current Playlist / Timeline":
                self.user_name.set_value(
                    self.connection.api.session.inspected_container.name
                    )
            elif  self.scope.value() == "Entire Session":
                self.user_name.set_value(
                    Path(self.connection.api.session.path).stem
                )

    def menu_callback(self):

        # auto set the output name
        self.attribute_changed(self.scope, 0)

        # This pops up our export dialog
        self.create_qml_item(
            dialog_qml
        )

    def do_export(self, scope, export_type, user_name, output_folder, file_type, resolution):

        self.__output_folder = urlsplit(output_folder).path
        self.exported_images = []

        # This is not necessary, the attributes should already by set in the UI
        # code but we'll do it here to make sure the backend attributes are 
        # correctly linked to the UI widgets state so that next time the tool
        # is used the values are the same as the last time it was used
        self.scope.set_value(scope)
        self.export_type.set_value(export_type)
        self.user_name.set_value(user_name)
        self.output_folder.set_value(output_folder)
        self.file_type.set_value(file_type)
        self.resolution.set_value(resolution)
        self.__image_file_ext = file_type.lower()
        __tmp_folder = None

        m = re.match("([0-9]+)*x*([0-9]+)", resolution)
        if m:
            self.export_width = int(m.group(1))
            self.export_height = int(m.group(2))
        else:
            self.export_width = -1
            self.export_height = -1

        if self.export_type.value() == "Annotated Images - Use Media Name and Frame":
            self.__export_type = ExportType.MEDIA_NAMED_IMAGES
        elif self.export_type.value() == "Annotated Images - Use Your Own Name":
            self.__export_type = ExportType.USER_NAMED_IMAEGS
        elif self.export_type.value() == "Greasepencil Package":
            self.__export_type = ExportType.GREASEPENCIL
            # try to make package folder
            import uuid
            __tmp_folder = self.__output_folder + "/.tmpfolder." + str(uuid.uuid4())
            if not os.path.isdir(__tmp_folder):
                os.mkdir(__tmp_folder)
            self.__image_file_ext = "png"
        else:
            raise Exception("Bad export type: {}.".format(self.export_type.value()))

        if not os.path.isdir(__tmp_folder if __tmp_folder else self.__output_folder):

            raise Exception("Output path {} is not a valid filesystem directory.".format(__tmp_folder if __tmp_folder else self.__output_folder))
        
        if scope == "Current Frame":

            self.export_bookmark_on_current_frame()

        elif scope == "Current Media":

            self.export_media_annotations(
                self.current_playhead().on_screen_media
                )
            
        elif scope == "Current Playlist / Timeline":
            
            self.export_bookmark_for_current_playlist(
                self.connection.api.session.inspected_container
                )

        else: 
            # "Entire Session"
            for playlist in self.connection.api.session.playlists:
                self.export_bookmark_for_current_playlist(
                    playlist
                    )
        
        # generate the greasePencil.xml
        if self.__export_type == ExportType.GREASEPENCIL:

            gp_file_path = self.__output_folder + "/greasePencil.xml"
            self.make_greaspencil_xml_file(
                gp_file_path,
                self.current_playhead().on_screen_media.media_source().rate.fps()
                )
            # now we zip the folder
            final_name = shutil.make_archive(self.__output_folder + "/" + self.user_name.value(), 'zip', __tmp_folder)
            # clean-up the temp folder
            shutil.rmtree(__tmp_folder)
            result_message = "GreasePencil package exported to {} with {} images.".format(
                final_name,
                len(self.exported_images)
                )
        else:
            result_message = "{} annotated image{} exported to {}".format(
                len(self.exported_images),
                "s" if len(self.exported_images) != 1 else "",
                self.__output_folder
                )

        return [True, result_message]

    def export_frame(self, idx, frame, duration, bookmark, media):

        include_image = True
        if self.__export_type == ExportType.MEDIA_NAMED_IMAGES:
            path = "{}/{}.{:04d}.{}".format(
                self.__output_folder,
                media.name,
                frame,
                self.__image_file_ext
                )
        elif self.__export_type == ExportType.USER_NAMED_IMAEGS:
            path = "{}/{}_{}.{}".format(
                self.__output_folder,
                self.user_name.value(),
                idx,
                self.__image_file_ext
                )
        elif self.__export_type == ExportType.GREASEPENCIL:
            path = "{}/{}.{:04d}.{}".format(
                self.__output_folder,
                self.user_name.value(),
                frame,
                self.__image_file_ext
                )
            include_image = False

        self.exported_images.append((frame, duration, Path(path).name))

        self.connection.api.app.snapshot_viewport.render_bookmark_with_transparency(
            path,
            bookmark.uuid,
            include_image=include_image,
            include_overlays=False,
            include_drawings=True,
            width=self.export_width,
            height=self.export_height
            )
        return idx+1

    def export_bookmark_on_current_frame(self):

        m = self.current_playhead().on_screen_media
        current_frame = self.current_playhead().attributes['Media Logical Frame'].value()
        bookmarks = m.ordered_bookmarks()
        bookmark = None
        for bm in bookmarks:
            bookmark_start_frame = int(round(bm.start.total_seconds()/m.media_source().rate.seconds()))
            bookmark_end_frame = bookmark_start_frame + int(round(bm.duration.total_seconds()/m.media_source().rate.seconds()))
            if bookmark_start_frame <= current_frame and bookmark_end_frame >= current_frame:
                bookmark = bm
                break
        if not bookmark:
            self.popup_message_box(
                "Error",
                "It doesn't look like there is a bookmark on-screen to export.")
        self.export_frame(
            0,
            bookmark_start_frame,
            1,
            bookmark,
            m)

    def export_media_annotations(self, media, index = 0):

        bookmarks = media.ordered_bookmarks()
        export_frames = set()
        for bookmark in bookmarks:
            bookmark_frame = int(round(bookmark.start.total_seconds()/media.media_source().rate.seconds()))
            bookmark_frame = bookmark_frame + media.media_source().media_reference.timecode().total_frames()
            duration = max(1, int(round(bookmark.duration.total_seconds()/media.media_source().rate.seconds())))
            export_frames.add((bookmark, bookmark_frame, duration))
        if not export_frames:
            self.popup_message_box(
                "Error",
                "It doesn't look like there are any bookmarks on the current media to export.")
            return

        for (bookmark, frame, duration) in export_frames:

            index = self.export_frame(
                index,
                frame,
                duration,
                bookmark,
                media)

        return index

    def export_bookmark_for_current_playlist(self, playlist, index = 0):

        media_items = playlist.media
        for m in media_items:
            index = self.export_media_annotations(
                m,
                index
                )
        return index

    def make_greaspencil_xml_file(self, output_file_path, fps):

        import xml.etree.cElementTree as ET

        root = ET.Element("greasepencil")
        settings = ET.SubElement(root, "settings")
        ET.SubElement(settings, "setting", fps=str(fps))
        frames = ET.SubElement(root, "frames")
        for (frame_num, duration, filename) in self.exported_images:
            ET.SubElement(frames, "frame", duration=str(duration), file=filename, layer="1", time=str(frame_num))
        tree = ET.ElementTree(root)
        ET.indent(tree)
        tree.write(output_file_path)



"""
Model XML from original support ticket
<greasepencil>
    <settings>
        <setting fps="24.0"/>
    </settings>
    <frames>
        <frame duration="1" file="40197511.0068.png" layer="1" time="68"/>
        <frame duration="1" file="40197511.0069.png" layer="1" time="69"/>
        <frame duration="1" file="40197511.0053.png" layer="1" time="53"/>
        <frame duration="1" file="40197511.0040.png" layer="1" time="40"/>
        <frame duration="1" file="40197511.0023.png" layer="1" time="23"/>
        <frame duration="1" file="40197511.0046.png" layer="1" time="46"/>
        <frame duration="1" file="40197511.0001.png" layer="1" time="1"/>
        <frame duration="1" file="40197511.0052.png" layer="1" time="52"/>
        <frame duration="1" file="40197511.0037.png" layer="1" time="37"/>
        <frame duration="1" file="40197511.0036.png" layer="1" time="36"/>
    </frames>
</greasepencil>
"""

# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return AnnotationsExporter(connection)
