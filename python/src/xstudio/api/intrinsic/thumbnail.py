# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_thumbnail_atom, clear_atom, count_atom, size_atom
from xstudio.api.auxiliary import ActorConnection

class Thumbnail(ActorConnection):
    """Access global state dictionary"""

    def __init__(self, connection, remote):
        """Create Thumbnail object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
        """
        ActorConnection.__init__(self, connection, remote)

    def get_thumbnail(self, media_pointer):
        """Get thumbnail.

        Args:
            media_pointer(MediaPointer): Frame to render.
        """
        return self.connection.request_receive(self.remote, get_thumbnail_atom(), media_pointer)[0]

    def clear(self, memory=True, disk=False):
        """Clear cache.

        Kwargs:
            include_disk(bool): Flush disk cache as well.
        """
        return self.connection.request_receive(self.remote, clear_atom(), memory, disk)[0]

    @property
    def count_memory(self):
        """ Get current memory cache count
        """
        return self.connection.request_receive(self.remote, count_atom(), True)[0]

    @property
    def count_disk(self):
        """ Get current disk cache count
        """
        return self.connection.request_receive(self.remote, count_atom(), False)[0]

    @property
    def size_memory(self):
        """ Get current memory cache count
        """

        return self.connection.request_receive(self.remote, size_atom(), True)[0]

    @property
    def size_disk(self):
        """ Get current disk cache count
        """
        return self.connection.request_receive(self.remote, size_atom(), False)[0]
