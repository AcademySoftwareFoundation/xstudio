# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.container import Container
from xstudio.api.session.bookmark.bookmark import Bookmark
from xstudio.core import add_bookmark_atom, remove_bookmark_atom, get_bookmark_atom, get_bookmarks_atom
from xstudio.core import Uuid
from xstudio.core import export_atom, ExportFormat

class Bookmarks(Container):
    """Bookmarks object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Bookmarks object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    def remove_bookmark(self, bookmark):
        """Remove bookmark

        Args:
            bookmark(Bookmark,Uuid): Bookmark to remove.

        Returns:
            removed(bool): Success.
        """

        if not isinstance(bookmark, Uuid):
            bookmark = bookmark.uuid

        return self.connection.request_receive(self.remote, remove_bookmark_atom(), bookmark)[0]

    def get_bookmark(self, uuid):
        """Get bookmark

        Args:
            uuid(Uuid): Bookmark uuid.

        Returns:
            bookmark(Bookmark) or None
        """
        i = self.connection.request_receive(self.remote, get_bookmark_atom(), uuid)[0]
        return Bookmark(self.connection, i.actor, i.uuid)

    @property
    def bookmarks(self):
        """Get bookmarks.

        Returns:
            bookmarks(list[Bookmark]): List of bookmarks.
        """
        result = self.connection.request_receive(self.remote, get_bookmarks_atom())[0]
        bookmarks = []
        for i in result:
            bookmarks.append(Bookmark(self.connection, i.actor, i.uuid))

        return bookmarks

    def add_bookmark(self, target=None):
        """Add bookmark

        Kwargs:
            target(Obj): Object to link bookmark to.

        Returns:
            bookmark(Bookmark) or None
        """
        if target is None:
            i = self.connection.request_receive(self.remote, add_bookmark_atom())[0]
        else:
            i = self.connection.request_receive(self.remote, add_bookmark_atom(), target.uuid_actor())[0]

        return Bookmark(self.connection, i.actor, i.uuid)

    def remove_bookmark(self, bookmark):
        """Remove bookmark

        Kwargs:
            bookmark(Bookmark|Uuid): Bookmark to remove.

        Returns:
            Success(bool): Succeeded.
        """
        if isinstance(bookmark, Bookmark):
            uuid = bookmark.uuid
        elif isinstance(bookmark, Uuid):
            uuid = bookmark
        else:
            uuid = None

        return self.connection.request_receive(self.remote, remove_bookmark_atom(), uuid)[0]

    def export(self, format=ExportFormat.EF_CSV):
        """Remove bookmark

        Kwargs:
            format(ExportFormat): Export format.

        Returns:
            Data([str,byte]): Succeeded.
        """

        return self.connection.request_receive(self.remote, export_atom(), format)[0]
