# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.container import Container
from xstudio.core import bookmark_detail_atom, BookmarkDetail
from xstudio.api.session.media import MediaSource


class Bookmark(Container):
    """Bookmark object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Bookmark object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def detail(self):
        """Bookmark detail.

        Returns:
            result(BookmarkDetail): The bookmark's detail 
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0]

    @property
    def media_source(self):
        """Get the Media object to which the bookmark is 
        attached.

        Returns:
            result(MediaSource): The bookmark's detail 
        """
        result = None

        if self.detail.owner and self.detail.owner.actor:            
            result = MediaSource(self.connection, self.detail.owner.actor, self.detail.owner.uuid)

        return result

    @property
    def enabled(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].enabled

    @enabled.setter
    def enabled(self, x):
        detail = BookmarkDetail()
        detail.enabled = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def has_focus(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].has_focus

    @has_focus.setter
    def has_focus(self, x):
        detail = BookmarkDetail()
        detail.has_focus = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def start(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].start

    @start.setter
    def start(self, x):
        detail = BookmarkDetail()
        detail.start = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def duration(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].duration

    @duration.setter
    def duration(self, x):
        detail = BookmarkDetail()
        detail.duration = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)


    @property
    def user_data(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].user_data

    @user_data.setter
    def user_data(self, x):
        detail = BookmarkDetail()
        detail.user_data = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def author(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].author

    @author.setter
    def author(self, x):
        detail = BookmarkDetail()
        detail.author = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def subject(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].subject

    @subject.setter
    def subject(self, x):
        detail = BookmarkDetail()
        detail.subject = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def category(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].category

    @category.setter
    def category(self, x):
        detail = BookmarkDetail()
        detail.category = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def colour(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].colour

    @colour.setter
    def colour(self, x):
        detail = BookmarkDetail()
        detail.colour = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def note(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """
        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].note

    @note.setter
    def note(self, x):
        detail = BookmarkDetail()
        detail.note = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)

    @property
    def created(self):
        """Is enabled.

        Returns:
            result(bool): Enabled ?
        """

        return self.connection.request_receive(self.remote, bookmark_detail_atom())[0].created

    @created.setter
    def created(self, x):
        detail = BookmarkDetail()
        detail.created = x
        self.connection.request_receive(self.remote, bookmark_detail_atom(), detail)
