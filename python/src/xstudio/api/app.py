# SPDX-License-Identifier: Apache-2.0
from xstudio.core import session_atom, join_broadcast_atom
from xstudio.core import colour_pipeline_atom, get_actor_from_registry_atom
from xstudio.core import viewport_playhead_atom
from xstudio.api.session import Session, Container
from xstudio.api.module import ModuleBase
from xstudio.api.auxiliary import ActorConnection

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
        return ModuleBase(self.connection, self.connection.request_receive(self.connection.remote(), get_actor_from_registry_atom(), "MAIN_VIEWPORT")[0])

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