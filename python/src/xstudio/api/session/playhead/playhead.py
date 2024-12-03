# SPDX-License-Identifier: Apache-2.0
from xstudio.core import play_atom, loop_atom, compare_mode_atom, play_forward_atom
from xstudio.core import logical_frame_atom, play_rate_mode_atom, source_atom, media_atom
from xstudio.core import simple_loop_start_atom, simple_loop_end_atom, use_loop_range_atom
from xstudio.core import viewport_playhead_atom, media_logical_frame_atom, playhead_rate_atom
from xstudio.core import JsonStore, change_attribute_value_atom, jump_atom

from xstudio.api.module import ModuleBase
from xstudio.api.session import Container
from xstudio.api.session.playhead.playhead_selection import PlayheadSelection
from xstudio.api.session.media.media import Media

class Playhead(ModuleBase):
    """MediaStream object, represents playhead."""

    def __init__(self, connection, remote, uuid=None):
        """Create Playhead object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        ModuleBase.__init__(self, connection, remote)

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
    def use_loop_range(self):
        """Using loop range

        Returns:
            use_loop_range(bool): Playhead will loop over specified range (or over whole range of source)
        """
        return self.connection.request_receive(self.remote, use_loop_range_atom())[0]

    @use_loop_range.setter
    def use_loop_range(self, x):
        """Set playing.

        Args:
            use_loop_range(bool): Sets if playhead loops over specified range
        """
        self.connection.send(self.remote, use_loop_range_atom(), x)

    @property
    def loop_in_point(self):
        """Loop in point.

        Returns:
            loop_in_point(int): Start frame of loop range
        """
        return self.connection.request_receive(self.remote, simple_loop_start_atom())[0]

    @loop_in_point.setter
    def loop_in_point(self, x):
        """Set playing.

        Args:
            loop_in_point(int): Set start frame of loop range.
        """
        self.connection.send(self.remote, simple_loop_start_atom(), x)

    @property
    def loop_out_point(self):
        """Loop out point.

        Returns:
            loop_out_point(int): End frame of loop range
        """
        return self.connection.request_receive(self.remote, simple_loop_end_atom())[0]

    @loop_out_point.setter
    def loop_out_point(self, x):
        """Set loop out point.

        Args:
            loop_out_point(int): Set end frame of loop range.
        """
        self.connection.send(self.remote, simple_loop_end_atom(), x)

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

    @property
    def media_frame(self):
        """Current (media) frame of on-screen media.

        Returns:
            position(int): Current media frame showing on-screen
        """
        return self.connection.request_receive(self.remote, media_logical_frame_atom())[0]

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
    def on_screen_media(self):
        """Current on-screen media sequence.

        Returns:
            source(Media): Currently playing this.
        """
        result = self.connection.request_receive(self.remote, media_atom())[0]
        return Media(self.connection, result)

    def source(self, new_source):
        """Set source to play (e.g. Media or Timeline).

        Args:
            new_source(actor): Set playable source to this.
        """
        self.connection.send(self.remote, source_atom(), new_source)

    @property
    def frame_rate(self):
        """Get playhead rate.

        Returns:
            playhead_rate(core.FrameRate): Rate for new playheads.
        """
        return self.connection.request_receive(self.remote, playhead_rate_atom())[0]
