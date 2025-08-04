# SPDX-License-Identifier: Apache-2.0
from xstudio.core import session_atom, join_broadcast_atom
from xstudio.core import colour_pipeline_atom, get_actor_from_registry_atom
from xstudio.core import viewport_playhead_atom, quickview_media_atom
from xstudio.core import UuidActorVec, UuidActor, viewport_atom
from xstudio.core import get_global_playhead_events_atom, set_clipboard_atom
from xstudio.core import active_viewport_atom, name_atom
from xstudio.api.session import Session, Container
from xstudio.api.session.playhead import Playhead
from xstudio.api.module import ModuleBase
from xstudio.api.auxiliary import ActorConnection
from xstudio.api.intrinsic import Viewport

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

    def viewport(self, name):
        """Access a named Viewport.
        Args:
            name(str): The viewport name

        Returns:
            viewport(ModuleBase): Viewport module."""
        return Viewport(self.connection, name)

    def viewport_names(self):
        """Get a list of the names of viewport instances that exist in the
        application. The viewport name can be used to access the viewport 
        via the 'viewport' method on this App object.
        
        Returns:
            list(str): A list of viewport names"""
        gphev = self.connection.request_receive(
                self.connection.remote(),
                get_global_playhead_events_atom())[0]

        viewports = self.connection.request_receive(
                gphev,
                viewport_atom()
                )[0]

        result = []
        for vp in viewports:
            result.append(
                self.connection.request_receive(
                    vp,
                    name_atom()
                    )[0]
                )
        return result

    @property
    def active_viewport(self):
        """Access the current (active & visible) viewport in the xSTUDIO UI.
        
        Returns:
            viewport(Viewport): Viewport module."""
        return Viewport(self.connection, active_viewport=True)

    def set_viewport_playhead(self, viewport_name, playhead):
        """Set's the named viewport to be driven by the given playhead
        Args:
            name(str): The viewport name
            playhead(Playhead): The playhead to drive the viewport"""

        viewport_remote = self.connection.request_receive(
                self.global_playhead_events.remote,
                viewport_atom(),
                viewport_name
                )[0]

        self.connection.send(viewport_remote, viewport_playhead_atom(), playhead.remote)

    def set_global_playhead(self, playhead):
        """Set's the global playhead to the given playhead. This will result
        in viewports showing frames coming from the given playhead
        Args:
            playhead(Playhead): The playhead to drive all viewports auto
            connecting to the global playhead"""

        self.connection.send(
                self.global_playhead_events.remote,
                viewport_playhead_atom(),
                playhead.remote
                )

    def set_clipboard(self, content):
        """Set UI clipboard
        Args:
            content(str): content
        """
        self.connection.request_receive(
            self.remote,
            set_clipboard_atom(),
            content
        )[0]

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