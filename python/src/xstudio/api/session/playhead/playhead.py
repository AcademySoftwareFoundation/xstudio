# SPDX-License-Identifier: Apache-2.0
from xstudio.core import play_atom, loop_atom, compare_mode_atom, play_forward_atom
from xstudio.core import logical_frame_atom, play_rate_mode_atom, source_atom
from xstudio.core import viewport_playhead_atom
from xstudio.core import JsonStore, change_attribute_value_atom, jump_atom

from xstudio.api.session import Container
from xstudio.api.session.playhead.playhead_selection import PlayheadSelection

class Playhead(Container):
    """MediaStream object, represents playhead."""

    def __init__(self, connection, remote, uuid=None):
        """Create Playhead object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    def show_in_viewport(self):
        """Connect this playhead to the viewport.
        """
        self.connection.send(self.remote, viewport_playhead_atom())

    @property
    def playing(self):
        """Is playing.

        Returns:
            playing(bool): Is playhead playing..
        """
        return self.connection.request_receive(self.remote, play_atom())[0]

    @playing.setter
    def playing(self, x):
        """Set playing.

        Args:
            playing(bool): Set playing.
        """
        self.connection.send(self.remote, play_atom(), x)

    @property
    def looping(self):
        """Is looping.

        Returns:
            looping(LoopMode): Current loop mode.
        """
        return self.connection.request_receive(self.remote, loop_atom())[0]

    @looping.setter
    def looping(self, x):
        """Set loop mode.

        Args:
            loop(LoopMode): Set loop mode.
        """
        self.connection.send(self.remote, loop_atom(), x)

    @property
    def compare_mode(self):
        """Playhead compare mode.

        Returns:
            compare_mode(str): Current compare mode.
        """
        self.connection.send(self.remote, attribute_value_atom(), "Compare")
        result = self.connection.dequeue_message()
        return result[0]

    @compare_mode.setter
    def compare_mode(self, x):
        """Set compare mode.

        Args:
            mode(str): Set compare mode.
        """
        self.connection.send(
            self.remote,
            change_attribute_value_atom(),
            "Compare",
            JsonStore(x)
            )

    @property
    def play_forward(self):
        """Is playing forwards.

        Returns:
            play_forward(bool): Is playhead playing forwards.
        """
        return self.connection.request_receive(self.remote, play_forward_atom())[0]

    @play_forward.setter
    def play_forward(self, x):
        """Set play forward mode.

        Args:
            mode(bool): Set playing forward.
        """
        self.connection.send(self.remote, play_forward_atom(), x)

    @property
    def position(self):
        """Current frame position.

        Returns:
            position(int): Current frame.
        """
        return self.connection.request_receive(self.remote, logical_frame_atom())[0]

    @position.setter
    def position(self, new_position):
        """Set current frame.

        Args:
            new_position(int): Set current frame to this.
        """
        self.connection.request_receive(self.remote, jump_atom(), new_position)

    @property
    def play_mode(self):
        """Current play mode.

        Returns:
            play_mode(TimeSourceMode): Current play mode .
        """
        return self.connection.request_receive(self.remote, play_rate_mode_atom())[0]

    @property
    def source(self):
        """Current media sequence.

        Returns:
            source(PlayheadSelection): Currently playing this.
        """
        result = self.connection.request_receive(self.remote, selection_actor_atom())
        return PlayheadSelection(self.connection, result.actor, result.uuid)

    @source.setter
    def source(self, new_source):
        """Set source.

        Args:
            new_source(actor): Set source to this.
        """
        self.connection.send(self.remote, source_atom(), new_source)
