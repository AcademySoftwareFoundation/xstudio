# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.playlist.subset import Subset

class ContactSheet(Subset):
    """ContactSheet object."""

    def __init__(self, connection, remote, uuid=None):
        """Create contactSheet object.

        Derived from Subset.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Subset.__init__(self, connection, remote, uuid)
