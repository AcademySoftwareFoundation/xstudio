# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_playlists_atom, get_playlist_atom, add_playlist_atom, move_container_atom
from xstudio.core import get_container_atom, create_group_atom, remove_container_atom, path_atom, get_media_atom
from xstudio.core import rename_container_atom, create_divider_atom, media_rate_atom, playhead_rate_atom
from xstudio.core import reflag_container_atom, merge_playlist_atom, copy_container_to_atom
from xstudio.core import get_bookmark_atom, save_atom, active_media_container_atom, current_media_atom, name_atom
from xstudio.core import viewport_active_media_container_atom
from xstudio.core import URI, Uuid, UuidVec

from xstudio.api.session.container import Container, PlaylistTree, PlaylistItem
from xstudio.api.session.playlist import Playlist
from xstudio.api.session.media.media import Media
from xstudio.api.session.bookmark import Bookmarks
from xstudio.api.session.playlist.subset import Subset
from xstudio.api.session.playlist.contact_sheet import ContactSheet
from xstudio.api.session.playlist.timeline import Timeline


class Session(Container):
    """Session object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Session object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def selected_media(self):
        """Get currently selected media.

        Returns:
            Media(): Media.
        """

        result = self.connection.request_receive(self.remote, current_media_atom())[0]
        media = []
        for i in result:
            media.append(Media(self.connection, i.actor, i.uuid))

        return media

    @property
    def viewed_playlist(self):
        """Get currently viewed (Playlist,Subset,Timelime,ContactSheet).

        Returns:
            Playlist(): Playlist.
        """
        return self.viewed_container

    @property
    def viewed_container(self):
        """Get currently viewed container.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """

        result = self.connection.request_receive(self.remote, viewport_active_media_container_atom())[0]
        c = Container(self.connection, result.actor)

        if c.type == "Timeline":
            return Timeline(self.connection, result.actor, result.uuid)
        elif c.type == "Subset":
            return Subset(self.connection, result.actor, result.uuid)
        elif c.type == "ContactSheet":
            return ContactSheet(self.connection, result.actor, result.uuid)

        return Playlist(self.connection, result.actor, result.uuid)

    @viewed_container.setter
    def viewed_container(self, container):
        """Set the current, on-screen playlist

        Args:
            container(Playlist,Subset,Timelime,ContactSheet): playlist, subset or timeline(sequence)
        """
        self.connection.send(self.remote, viewport_active_media_container_atom(), container.uuid)

    @property
    def inspected_playlist(self):
        """Get currently inspected playlist/subset/timeline etc.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """
        return self.inspected_container


    @property
    def inspected_container(self):
        """Get currently inspected container.

        Returns:
            container(Playlist,Subset,Timelime,ContactSheet): Container.
        """

        result = self.connection.request_receive(self.remote, active_media_container_atom())[0]
        c = Container(self.connection, result.actor)

        if c.type == "Timeline":
            return Timeline(self.connection, result.actor, result.uuid)
        elif c.type == "Subset":
            return Subset(self.connection, result.actor, result.uuid)
        elif c.type == "ContactSheet":
            return ContactSheet(self.connection, result.actor, result.uuid)

        return Playlist(self.connection, result.actor, result.uuid)

    @inspected_container.setter
    def inspected_container(self, container):
        """Set the current, *inspected* playlist

        Args:
            container(Playlist,Subset,Timelime,ContactSheet): playlist, subset or timeline(sequence)
        """
        self.connection.send(self.remote, active_media_container_atom(), container.uuid)

    @property
    def path(self):
        """Get session file path.

        Returns:
            path(str): Session path.
        """

        return str(self.connection.request_receive(self.remote, path_atom())[0][0])

    @property
    def media_rate(self):
        """Get new media rate.

        Returns:
            media_rate(core.FrameRate): Media rate to use for rateless media.
        """
        return self.connection.request_receive(self.remote, media_rate_atom())[0]

    @property
    def bookmarks(self):
        """Get bookmarks.

        Returns:
            Bookmarks(): Session bookmarks.
        """
        return Bookmarks(
            self.connection,
            self.connection.request_receive(self.remote, get_bookmark_atom())[0],
            Uuid()
        )

    @media_rate.setter
    def media_rate(self, x):
        """Set media_rate.

        Args:
            media_rate(core.FrameRate): Set media_rate.
        """
        self.connection.send(self.remote, media_rate_atom(), x)

    @property
    def playhead_rate(self):
        """Get playhead rate.

        Returns:
            playhead_rate(core.FrameRate): Rate for new playheads.
        """
        return self.connection.request_receive(self.remote, playhead_rate_atom())[0]

    @playhead_rate.setter
    def playhead_rate(self, x):
        """Set playhead_rate.

        Args:
            playhead_rate(core.FrameRate): Set playhead_rate.
        """
        self.connection.send(self.remote, playhead_rate_atom(), x)

    @property
    def playlists(self):
        """Get playlists.

        Returns:
            playlists(list[Playlist]): Playlists.
        """
        result = self.connection.request_receive(self.remote, get_playlists_atom())[0]
        return [Playlist(self.connection, i.actor, i.uuid) for i in result]


    def create_playlist(self, name="New Playlist", before=Uuid(), into=False):
        """Create new playlist.

        Kwargs:
            name(str): Name of new playlist
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            playlist(Uuid,Playlist): Returns container Uuid and Playlist
        """

        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, add_playlist_atom(), name, before, into)[0]
        return (result[0], Playlist(self.connection, result[1].actor, result[1].uuid))

    def merge_playlist(self, playlists, name="Combined Playlist", before=Uuid(), into=False):
        """Create new playlist.

        Kwargs:
            playlists(list): Playlists to merge
            name(str): Name of new playlist
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            playlist(Uuid,Playlist): Returns container Uuid and Playlist
        """
        uuids = UuidVec()
        for i in playlists:
            if not isinstance(i, Uuid):
                i = i.uuid
            uuids.push_back(i)

        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, merge_playlist_atom(), name, before, uuids)[0]
        return (result[0], Playlist(self.connection, result[1].actor, result[1].uuid))

        # return (result[0], Playlist(self.connection, result[1][1], result[1][0]))

    def create_divider(self, name="New Divider", before=Uuid(), into=False):
        """Create new divider.

        Kwargs:
            name(str): Name of new divider
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            divider(PlaylistTree): Returns container.
        """

        if not isinstance(before, Uuid):
            before = before.uuid

        uuid = self.connection.request_receive(self.remote, create_divider_atom(), name, before, into)[0]
        result = self.connection.request_receive(self.remote, get_container_atom(), uuid)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def move_container(self, src, before=Uuid(), into=False):
        """Move container in tree.

        Args:
            src(Container/Uuid): Item to move.

        Kwargs:
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid
        if not isinstance(before, Uuid):
            before = before.uuid

        return self.connection.request_receive(self.remote, move_container_atom(), src, before, into)[0]

    def remove_container(self, src):
        """Remove container from tree.

        Args:
            src(Container/Uuid): Item to remove.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid

        return self.connection.request_receive(self.remote, remove_container_atom(), src)[0]

    def get_container(self, src):
        """Get container from tree.

        Args:
            src(Item/Uuid): Item to find.

        Returns:
            Container(PlaylistTree): Returns result.
        """
        if not isinstance(src, Uuid):
            src = src.uuid

        result = self.connection.request_receive(self.remote, get_container_atom(), src)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def set_on_screen_source(self, src):
        """Set the onscreen source.

        Args:
            src(Container): Playlist, Subset, ContactSheet, Timeline
        """
        self.connection.send(
            self.remote,
            active_media_container_atom(),
            src.uuid_actor().actor
            )

    @property
    def playlist_tree(self):
        """Get container tree.

        Returns:
            Tree(PlaylistTree): Returns result.
        """
        result = self.connection.request_receive(self.remote, get_container_atom())[0]
        return PlaylistTree(self.connection, self.remote, result)

    def create_group(self, name="New Group", before=Uuid()):
        """Create new group.

        Kwargs:
            name(str): Name of new group
            before(Uuid): Insert before this item.

        Returns:
            group(PlaylistTree): Returns container.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, create_group_atom(), name, before)[0]
        return PlaylistTree(self.connection, self.remote, result)

    def rename_container(self, name, container):
        """Rename container in tree.

        Args:
            name(str): New name.
            container(Container/Uuid): Item to rename.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(container, Uuid):
            container = container.uuid

        return self.connection.request_receive(self.remote, rename_container_atom(), name, container)[0]

    def reflag_container(self, flag, container):
        """Reflag container in tree.

        Args:
            flag(str): New flag.
            container(Container/Uuid): Item to reflag.

        Returns:
            success(bool): Returns result.
        """
        if not isinstance(container, Uuid):
            container = container.uuid

        return self.connection.request_receive(self.remote, reflag_container_atom(), flag, container)[0]

    def copy_containers_to(self, playlist, containers, before=Uuid(), into=False):
        """Create copy containers to playlist.

        Kwargs:
            playlist(Uuid/Playlist): Destination playlist.
            containers(list): Containers to copy
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            uuids(list[Uuid]): Returns list of new container uuids.
        """
        uuids = UuidVec()
        for i in containers:
            if not isinstance(i, Uuid):
                i = i.uuid
            uuids.push_back(i)

        if not isinstance(before, Uuid):
            before = before.uuid
        if not isinstance(playlist, Uuid):
            playlist = playlist.uuid

        return self.connection.request_receive(self.remote, copy_container_to_atom(), playlist, uuids, before, into)[0]

    def get_media(self):
        """Return all media over all playlists

        Returns:
            media(list[Media]): List of media objects.
        """
        result = self.connection.request_receive(self.remote, get_media_atom())[0]
        return [Media(self.connection, i.actor, i.uuid) for i in result]

    def save(self):
        """Save session

        Returns:
            int(hash]): Hash of saved session
        """
        return self.connection.request_receive(self.remote, save_atom())[0]

    def save_as(self, path):
        """Save session to path

        Args:
            path(str/uri): Path to save to.

        Returns:
            int(hash]): Hash of saved session
        """
        if not isinstance(path,URI):
            path = URI(path)

        return self.connection.request_receive(self.remote, save_atom(), path)[0]