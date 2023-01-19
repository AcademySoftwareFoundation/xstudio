# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_media_type_atom, get_stream_detail_atom

from xstudio.api.session.container import Container

class MediaStream(Container):
    """MediaStream object, contains media track."""

    def __init__(self, connection, remote, uuid=None):
        """Create MediaStream object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def media_type(self):
        """Get mediatype.

        Returns:
            media_type(MediaType): Type of media stream.
        """
        return self.connection.request_receive(self.remote, get_media_type_atom())[0]

    @property
    def media_stream_detail(self):
        """Get media stream details.

        Returns:
            stream_detail(StreamDetail): Detail of media stream.
        """
        return self.connection.request_receive(self.remote, get_stream_detail_atom())[0]