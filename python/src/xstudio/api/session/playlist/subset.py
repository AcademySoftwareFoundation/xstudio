# SPDX-License-Identifier: Apache-2.0
from xstudio.core import Uuid, add_media_atom, actor
from xstudio.api.session.container import Container
from xstudio.api.session.media.media import Media

class Subset(Container):
    """Subset object."""

    def __init__(self, connection, remote, uuid=None):
        """Create contactSheet object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    def add_media(self, media, before_uuid=Uuid()):
        """Add media to subset.

        Args:
            media(Media/Uuid/Actor): Media reference.

        Kwargs:
            before_uuid(Uuid/Container): Insert media before this uuid.

        Returns:
            media(Media): Media object.

        """
        if not isinstance(media, actor) and not isinstance(media, Uuid):
            media = media.remote

        if not isinstance(before_uuid, Uuid):
            before_uuid = before_uuid.uuid

        result = self.connection.request_receive(self.remote, add_media_atom(), media, before_uuid)[0]

        return Media(self.connection, result.actor, result.uuid)
