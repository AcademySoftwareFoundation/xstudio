# SPDX-License-Identifier: Apache-2.0
from xstudio.core import monitored_atom, Uuid, MT_IMAGE, MT_AUDIO
from xstudio.core import move_atom, remove_atom, actor, select_all_media_atom
from xstudio.core import select_media_atom, get_selected_sources_atom

from xstudio.api.session import Container
from xstudio.api.session.media.media import Media

class PlayheadSelection(Container):
    """Playhead selection object, what the playhead plays."""

    def __init__(self, connection, remote, uuid=None):
        """Create PlayheadSelection object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """

        Container.__init__(self, connection, remote, uuid)

    @property
    def monitored(self):
        """Current monitored Playhead selection.

        Returns:
            monitored(PlayheadSelection): PlayheadSelection.
        """

        result = self.connection.request_receive(self.remote, monitored_atom())[0]
        return PlayheadSelection(self.connection, result.actor, result.uuid)

    @monitored.setter
    def monitored(self, i):
        """Set current monitored Playhead selection.

        Args:
            monitored(PlayheadSelection): PlayheadSelection.
        """
        self.connection.send(self.remote, monitored_atom(), i)

    @property
    def selected_sources(self):
        """Get selected media objects.

        Returns:
            clips(list[Media]): Current media items in order.
         """
        clips = []

        result = self.connection.request_receive(self.remote, get_selected_sources_atom())[0]

        for i in result:
            clips.append(Media(self.connection, i.actor, i.uuid))

        return clips

    def set_selection(self, media_uuids):
        """Select the media items

        Args:
            list(uuid): Items to select.

        Returns:
            success(bool): Selection set.
        """
        self.connection.request_receive(self.remote, select_media_atom())
        for media_uuid in media_uuids:
            self.connection.request_receive(self.remote, select_media_atom(), media_uuid)
        return True

    def insert(self, media, before=Uuid(), media_type=MT_IMAGE):

        # NEEDS FIXING!
        #
        # if not isinstance(media, actor):
        #   media = media.remote

        # if isinstance(before_uuid, SelectionClip):
        #     before_uuid = before_uuid.source

        # if not isinstance(before_uuid, Uuid):
        #     before_uuid = before_uuid.uuid

        # self.connection.send(self.remote, insert_atom(), media_type, media, before_uuid)
        # result = self.connection.dequeue_message()
        # return SelectionClip(self.connection, result[0][0], result[0][1])
        return None

    def move(self, to_move, before=Uuid()):
        """Move clip.

        Args:
            to_move(uuid,Container): Item to move.

        Kwargs:
            before(Uuid): move before this.

        Returns:
            success(bool): Reordered.
        """

        if not isinstance(to_move, Uuid):
            to_move = to_move.uuid

        if not isinstance(before, Uuid):
            before = before.uuid

        return self.connection.request_receive(self.remote, move_atom(), to_move, before)[0]

    def remove(self, to_remove):
        """Remove clip.

        Args:
            to_remove(Uuid,Container): Item to remove.

        Returns:
            success(bool): Reordered.
        """

        if not isinstance(to_remove, Uuid):
            to_remove = to_remove.uuid

        # got uuids..
        self.connection.send(self.remote, remove_atom(), to_remove)
        return self.connection.dequeue_message()[0]

    def select_all(self):
        """Select all media in playlist.

        Args:
            to_remove(Uuid,Container): Item to remove.

        Returns:
            None.
        """
        self.connection.send(self.remote, select_all_media_atom())
                
