# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.playlist.timeline.item import Item
from xstudio.api.session.media.media import Media
from xstudio.core import get_media_atom

class Clip(Item):
    """Timeline object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Clip object.

        Derived from Item.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Item.__init__(self, connection, remote, uuid)

    @property
    def media(self):
        """Get media item.

        Returns:
            item(Item): Media for Clip.
        """
        m = self.connection.request_receive(self.remote, get_media_atom())[0]
        if m.uuid.is_null():
            return None

        return Media(self.connection, m.actor, m.uuid)
