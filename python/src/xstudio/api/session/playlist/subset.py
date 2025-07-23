# SPDX-License-Identifier: Apache-2.0
from xstudio.core import Uuid, add_media_atom, actor, selection_actor_atom, get_media_atom
from xstudio.core import get_playlist_atom
from xstudio.api.session.container import Container
from xstudio.api.session.media.media import Media
from xstudio.api.session.playhead import Playhead, PlayheadSelection
from xstudio.api.auxiliary.json_store import JsonStoreHandler

class Subset(Container, JsonStoreHandler):
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
        JsonStoreHandler.__init__(self, self)

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
            media = media.uuid_actor()

        if not isinstance(before_uuid, Uuid):
            before_uuid = before_uuid.uuid

        result = self.connection.request_receive(self.remote, add_media_atom(), media, before_uuid)[0]

        return Media(self.connection, result.actor, result.uuid)

    @property
    def media(self):
        """Get media in subset

        Returns:
            media(list[media]): Media
        """
        result = self.connection.request_receive(self.remote, get_media_atom())[0]
        return [Media(self.connection, i.actor, i.uuid) for i in result]

    @property
    def playhead_selection(self):
        """The actor that filters a selection of media from a playhead
        and passes to a playhead for playback.

        Returns:
            source(PlayheadSelection): Currently playing this.
        """
        result =  self.connection.request_receive(self.remote, selection_actor_atom())[0]
        return PlayheadSelection(self.connection, result)

    @property
    def parent_playlist(self):
        """Get the parent playlist of the ContactSheet/Subset object

        Returns:
            source(PlayheadList): Currently playing this.
        """
        result =  self.connection.request_receive(self.remote, get_playlist_atom())[0]
        return PlayheadList(self.connection, result)

