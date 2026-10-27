# SPDX-License-Identifier: Apache-2.0
from xstudio.core import colour_pipeline_atom
from xstudio.core import viewport_playhead_atom, quickview_media_atom
from xstudio.core import UuidActorVec, UuidActor, viewport_atom
from xstudio.core import get_global_playhead_events_atom, active_viewport_atom
from xstudio.core import URI, render_viewport_to_image_atom
from xstudio.core import viewport_scale_atom, viewport_pan_atom, V2f
from xstudio.api.session.playhead import Playhead
from xstudio.api.module import ModuleBase
from xstudio.api.intrinsic.colour_pipeline import ColourPipeline

class Viewport(ModuleBase):
    """Viewport object, represents a viewport in the UI or offscreen."""

    def __init__(self, connection, viewport_name = "", active_viewport = False):
        """Create Viewport object.

        Args:
            connection(Connection): Connection object
            viewport_name(str): Name of viewport
            active_viewport(bool): Set to true to get to the first viewport that
               is currently visible in the xSTUDIO Main Window
        """
        gphev = connection.request_receive(
                connection.remote(),
                get_global_playhead_events_atom())[0]

        if active_viewport:
            remote = connection.request_receive(
                gphev,
                active_viewport_atom()
                )[0]
            if not remote:
                raise Exception("No visible viewports")
        else:
            remote = connection.request_receive(
                gphev,
                viewport_atom(),
                viewport_name
                )[0]

        ModuleBase.__init__(
            self,
            connection,
            remote
            )

        self.__playhead = None

    @property
    def playhead(self):
        """Access the Playhead object supplying images to the viewport
        """
        if not self.__playhead:
            self.__playhead = Playhead(self.connection, self.connection.request_receive(self.remote, viewport_playhead_atom())[0])
        return self.__playhead

    @property
    def colour_pipeline(self):
        """Access the ColourPipeline object that provides colour management 
        data to the viewport        
        """
        return ColourPipeline(self.connection, self.connection.request_receive(self.remote, colour_pipeline_atom())[0])

    def quickview(self, media_items, compare_mode="Off", position=(100,100), size=(1280,720)):
        """Launch a quickview window with one or more media items for viewing (and comparing).

        Args:
            media_items(list(Media)): A list of Media objects to be shown in quickview
                                windows
            compare_mode(str): Remote actor object
            position(tuple(int,int)): X/Y Position of new window (default=(100,100))
            size(tuple(int,int)): X/Y Size of new window (default=(1280,720))

        """
        media_actors = UuidActorVec()
        for m in media_items:
            media_actors.push_back(UuidActor(m.uuid, m.remote))

        self.connection.request_receive(
            self.remote,
            quickview_media_atom(),
            media_actors,
            compare_mode,
            position[0],
            position[1],
            size[0],
            size[1])

    @property
    def scale(self):
        """Get the raw viewport scale (image_pixels / viewport_pixels).
        A larger value means more zoomed in. At fit-to-window this is typically 5-15,
        not 1.0 — capture a baseline at fit-to-window to normalise if needed.

        Returns:
            float: Raw scale value.
        """
        return self.connection.request_receive(self.remote, viewport_scale_atom())[0]

    @scale.setter
    def scale(self, value):
        """Set the raw viewport scale.

        Args:
            value(float): Raw scale value (image_pixels / viewport_pixels).
        """
        self.connection.send(self.remote, viewport_scale_atom(), float(value))

    @property
    def pan(self):
        """Get the viewport pan offset as an (x, y) tuple.

        Returns:
            tuple(float, float): Pan offset in viewport-space units.
        """
        v = self.connection.request_receive(self.remote, viewport_pan_atom())[0]
        return (v.x, v.y)

    @pan.setter
    def pan(self, value):
        """Set the viewport pan offset.

        Args:
            value(tuple(float, float) or V2f): Pan offset as (x, y).
        """
        if isinstance(value, (tuple, list)):
            value = V2f(float(value[0]), float(value[1]))
        self.connection.send(self.remote, viewport_pan_atom(), value)

    def set_playhead(self, playhead):
        """Set the playhead that is delivering frames to the viewport, i.e.
        the active playhead.

        Args:
            playhead(Playhead): The playhead."""

        self.connection.request_receive(self.remote, viewport_playhead_atom(), playhead.remote)
        self.__playhead = Playhead(self.connection, self.connection.request_receive(self.remote, viewport_playhead_atom())[0])

class OffscreenViewport(Viewport):

    def __init__(self, connection, viewport_name = "snapshot_viewport"):
        """Create OffscreenViewport object.

        Args:
            connection(Connection): Connection object
            viewport_name(str): Name of offscreen viewport (e.g. 'snapshot_viewport')
        """

        Viewport.__init__(
            self,
            connection,
            viewport_name
            )

        self.__playhead = None

    def render_bookmark_with_transparency(
        self, 
        output_path,
        bookark_id,
        include_image=True,
        include_overlays=False,
        include_drawings=True,
        width=-1,
        height=-1):
        """Render a bookmarked frame to an image file. You can use this method
        to write annotation draw-overs ONLY to disk with transparency by setting
        include_image to False and include_drawings to True

        Args:
            output_path(URI/string): The filesystem path to render to. The extension will
               determine the format of the image file. (e.g. jpg, tiff, png, exr)
            bookark_id(xstudio.core.Uuid): The uuid of the bookmark to render.
            include_image(bool): Include the actual on-screen image in the render
            include_overlays(bool): Include overlays, like mask and metadata HUD
            include_drawings(bool): Include the 
            width(int): The output image with / pixels - do not set to use on-screen image format
            height(int): The output image height / pixels - do not set to use on-screen image format
        """

        bookmark = self.connection.api.session.bookmarks.get_bookmark(bookark_id)
        media = bookmark.media
        media_logical_frame = round(bookmark.start.total_seconds()/media.media_source().rate.seconds())
        if not isinstance(output_path, URI):
            output_path = URI("file:///" + output_path)

        self.connection.request_receive(
            self.remote,
            render_viewport_to_image_atom(), 
            media.remote,
            media_logical_frame, # media logical frame to render
            include_image,
            include_overlays,
            include_drawings,
            width, # image width
            height, # image height
            output_path)

    def render_media_frame(
        self, 
        output_path,
        media,
        media_logical_frame,
        width=1920,
        height=1080):
        """Render a media frame to an image file.

        Args:
            output_path(URI/string): The filesystem path to render to. The extension will
               determine the format of the image file. (e.g. jpg, tiff, png, exr)
            media(Media): The media item to provide the source frame
            media_logical_frame(int): The logical media frame to render out.
            width(int): The output image with / pixels
            height(int): The output image height / pixels
        """

        if not isinstance(path, URI):
            path = URI("file:///" + path)

        self.connection.request_receive(
            self.remote,
            render_viewport_to_image_atom(), 
            media.remote,
            media_logical_frame, # media logical frame to render
            width, # image width
            height, # image height
            path)
