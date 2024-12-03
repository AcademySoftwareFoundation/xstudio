# SPDX-License-Identifier: Apache-2.0
from xstudio.core import session_atom, join_broadcast_atom
from xstudio.core import colour_pipeline_atom, get_actor_from_registry_atom
from xstudio.core import viewport_playhead_atom, quickview_media_atom
from xstudio.core import UuidActorVec, UuidActor, viewport_atom
from xstudio.core import get_global_playhead_events_atom
from xstudio.api.session import Session, Container
from xstudio.api.session.playhead import Playhead
from xstudio.api.module import ModuleBase
from xstudio.api.auxiliary import ActorConnection

class Viewport(ModuleBase):
    """Viewport object, represents a viewport in the UI or offscreen."""

    def __init__(self, connection, viewport_name):
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

        ModuleBase.__init__(
            self,
            connection,
            connection.request_receive(
                gphev,
                viewport_atom(),
                viewport_name
                )[0]
            )

    @property
    def playhead(self):
        """Access the Playhead object supplying images to the viewport
        """        
        return Playhead(self.connection, self.connection.request_receive(self.remote, viewport_playhead_atom())[0])            

    def quickview(self, media_items, compare_mode="Off", position=(100,100), size=(1280,720)):
        """Connect this playhead to the viewport.

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

class App(Container):
    """App access. """
    def __init__(self, connection, remote, uuid=None):
        """Create app object.
        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def session(self):
        """Session object.

        Returns:
            session(Session): Session object."""
        return Session(self.connection, self.connection.request_receive(self.remote, session_atom())[0])

    @property
    def colour_pipeline(self):
        """Colour Pipeline (colour management) object.

        Returns:
            colour_pipeline(ModuleBase): Colour Pipeline object."""
        return ModuleBase(self.connection, self.connection.request_receive(self.connection.remote(), colour_pipeline_atom())[0])

    @property
    def viewport(self):
        """The main Viewport module.

        Returns:
            viewport(ModuleBase): Viewport module."""
        return Viewport(self.connection)

    @property
    def global_playhead_events(self):
        """The global playhead events actor. This can be used to query which
        playhead is active in the viewer, or to set the active playhead. We
        can also join to its events group to get other fine grained messages
        about playead state (such as current frame, current media etc.)

        Returns:
            global_playhead_events(ActorConnection): Global Playhead Events Actor."""
        return ActorConnection(
            self.connection,
            self.connection.request_receive(self.connection.remote(), get_actor_from_registry_atom(), "GLOBALPLAYHEADEVENTS")[0]
            )

    def set_viewport_playhead(self, playhead):
        """Set the playhead that is delivering frames to the viewport, i.e.
        the active playhead.

        Args:
            playhead(Playhead): The playhead."""

        self.connection.request_receive(self.viewport.remote, viewport_playhead_atom(), playhead.remote)