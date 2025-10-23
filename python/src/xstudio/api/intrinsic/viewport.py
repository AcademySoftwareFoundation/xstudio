# SPDX-License-Identifier: Apache-2.0
from xstudio.core import colour_pipeline_atom
from xstudio.core import viewport_playhead_atom, quickview_media_atom
from xstudio.core import UuidActorVec, UuidActor, viewport_atom
from xstudio.core import get_global_playhead_events_atom, active_viewport_atom
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

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
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

    def set_playhead(self, playhead):
        """Set the playhead that is delivering frames to the viewport, i.e.
        the active playhead.

        Args:
            playhead(Playhead): The playhead."""

        self.connection.request_receive(self.remote, viewport_playhead_atom(), playhead.remote)
        self.__playhead = Playhead(self.connection, self.connection.request_receive(self.remote, viewport_playhead_atom())[0])
