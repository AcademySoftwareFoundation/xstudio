# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.playlist.timeline.item import Item

class Gap(Item):
    """Timeline object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Gap object.

        Derived from Item.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Item.__init__(self, connection, remote, uuid)

