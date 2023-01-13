# SPDX-License-Identifier: Apache-2.0
from xstudio.core import overflow_mode_atom, duration_atom
from xstudio.core import source_offset_frames_atom, source_atom

from xstudio.api.auxiliary.helpers import get_uuid, get_type
from xstudio.api.session import Container
from xstudio.api.session.media import Media
from xstudio.api.session.media import MediaSource
from xstudio.api.session.media import MediaStream

def _construct_from_remote(connection, remote, uuid=None, remote_type=None):
    result = None

    if uuid is None:
        uuid = get_uuid(connection, remote)

    if remote_type is None:
        remote_type = get_type(connection, remote)

    if uuid and remote_type:
        if remote_type == "Media":
            result = Media(connection, remote, uuid)
        elif remote_type == "MediaSource":
            result = MediaSource(connection, remote, uuid)
        elif remote_type == "MediaStream":
            result = MediaStream(connection, remote, uuid)

    return result

class RetimedClip(Container):
    """Wrapper for playhead retimed clips clips"""
    def __init__(self, connection, remote, uuid=None):
        """Create RetimedClip object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def overflow_mode(self):
        """Item overflow mode.

        Returns:
            overflow_mode(core.OverflowMode): What happens when we leave clip.
        """
        return self.connection.request_receive(self.remote, overflow_mode_atom())[0]

    @overflow_mode.setter
    def overflow_mode(self, i):
        """Set item overflow mode.

        Args:
            overflow_mode(core.OverflowMode): Set to this.
        """
        self.connection.request_receive(self.remote, overflow_mode_atom(), i)

    @property
    def duration(self):
        """Item duration.

        Returns:
            duration(core.FrameRateDuration): Duration of item.
        """
        return self.connection.request_receive(self.remote, duration_atom())[0]

    @duration.setter
    def duration(self, i):
        """Set item duration.

        Args:
            duration(core.FrameRateDuration): Set to this.
        """
        self.connection.request_receive(self.remote, duration_atom(), i)

    @property
    def offset_frames(self):
        """Item source start time.

        Returns:
            offset_frames(int): The retime offset (frames).
        """
        return self.connection.request_receive(self.remote, source_offset_frames_atom())[0]

    @offset_frames.setter
    def offset_frames(self, i):
        """Set item source offset frames.

        Args:
            source_offset_frames(int): Set offset to this.
        """
        self.connection.request_receive(self.remote, source_offset_frames_atom(), i)

    @property
    def source_duration(self):
        """Source Item duration.

        Returns:
            source_duration(core.FrameRateDuration): Duration of item source.
        """
        return self.connection.request_receive(self.remote, duration_atom())[0]

    @source_duration.setter
    def source_duration(self, i):
        """Set item source duration.

        Args:
            source_duration(core.FrameRateDuration): Set to this.
        """
        self.connection.request_receive(self.remote, duration_atom(), i)


    @property
    def source(self):
        """Item source.

        Returns:
            source(Media*): Item source.
        """
        result = self.connection.request_receive(self.remote, source_atom())[0]
        return _construct_from_remote(self.connection, result.actor, result.uuid)

    # @source.setter
    # def source(self, i):
    #     self.connection.send(self.remote, overflow_mode_atom(), i)
    #     self.connection.dequeue_message()
