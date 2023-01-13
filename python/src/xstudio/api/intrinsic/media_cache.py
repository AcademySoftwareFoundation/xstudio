# SPDX-License-Identifier: Apache-2.0
from xstudio.api.auxiliary import ActorConnection
from xstudio.core import keys_atom, erase_atom, count_atom, size_atom

class MediaCache(ActorConnection):
    """Media cache object."""
    def __init__(self, connection, remote):
        """Create media cache object.

        Args:
            connection(Connection): Connection object.
            remote(actor): Cache actor object.
        """
        ActorConnection.__init__(self, connection, remote)

    def keys(self):
        """Get cache keys.

        Returns:
            keys(list[str]): List of key strings.
        """

        return self.connection.request_receive(self.remote, keys_atom())[0]

    def erase(self, key):
        """Erase key/cached item.

        Args:
            key(str): Key of item to remove from cache.
        """

        self.connection.send(self.remote, erase_atom(), key)

    @property
    def count(self):
        """Get cache item count.

        Returns:
            count(int): Count of cached items.
        """
        return self.connection.request_receive(self.remote, count_atom())[0]

    @property
    def size(self):
        """Size of cached items.

        Returns:
            size(int): Count of cached items in bytes.
        """
        return self.connection.request_receive(self.remote, size_atom())[0]
