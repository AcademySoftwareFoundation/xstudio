# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_playhead_atom, get_media_atom, get_container_atom
from xstudio.core import Uuid, create_group_atom, create_contact_sheet_atom, add_media_atom
from xstudio.core import rename_container_atom, create_subset_atom, create_timeline_atom
from xstudio.core import move_container_atom, remove_container_atom, type_atom, parse_posix_path
from xstudio.core import create_divider_atom, media_rate_atom, playhead_rate_atom, URI, FrameRate
from xstudio.core import remove_media_atom, UuidVec, move_media_atom, create_playhead_atom, selection_actor_atom
from xstudio.core import convert_to_timeline_atom, convert_to_subset_atom, convert_to_contact_sheet_atom
from xstudio.core import reflag_container_atom
from xstudio.core import FrameList, FrameRate, MediaType
from xstudio.core import get_json_atom, set_json_atom, JsonStore

from xstudio.api.session.container import Container, PlaylistTree
from xstudio.api.session.playhead.playhead import Playhead
from xstudio.api.session.playhead.playhead_selection import PlayheadSelection
from xstudio.api.session.media.media import Media
from xstudio.api.session.playlist.subset import Subset
from xstudio.api.session.playlist.contact_sheet import ContactSheet
from xstudio.api.session.playlist.timeline import Timeline

import json

class Playlist(Container):
    """Playlist object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Playlist object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def playhead(self):
        """Get playhead.

        Returns:
            playhead(Playhead): Playhead attached to playlist.
        """
        result = self.connection.request_receive(self.remote, get_playhead_atom())[0]

        return Playhead(self.connection, result.actor, result.uuid)


    def add_media_list(self, path, recurse=False, media_rate=None):
        """Add media from path.

        Args:
            path(str/URI): path to collect media from.

        Kwargs:
            recurse(bool): Recursively scan path
            media_rate(FrameRate): Frame rate for rateless media.

        Returns:
            media(list[Media]): Media found.
        """

        if not isinstance(path, URI):
            path = URI("file:///" + path)

        if not isinstance(media_rate, FrameRate):
            result = self.connection.request_receive(self.remote, add_media_atom(), path, recurse, Uuid())[0]
        else:
            result = self.connection.request_receive(self.remote, add_media_atom(), path, recurse, media_rate, Uuid())[0]
            
        return [Media(self.connection, i.actor, i.uuid) for i in result]

    def add_media_with_audio(self, image_path, audio_path, audio_offset=0):
        """Add media pair from path.

        Args:
            image_path(str/URI): path to collect image from.
            audio_path(str/URI): path to collect audio from.
        Kwargs:
            audio_offset(int): Offset audio by audio_offset frames.

        Returns:
            media(Media): Media.
        """
        if isinstance(image_path, URI):
            result = self.connection.request_receive(self.remote, add_media_atom(), image_path, False, Uuid())[0][0]
        else:
            ppp = parse_posix_path(image_path)

            if not str(ppp[1]):
                result = self.connection.request_receive(self.remote, add_media_atom(), image_path, ppp[0], Uuid())[0]
            else:
                result = self.connection.request_receive(self.remote, add_media_atom(), image_path, ppp[0], ppp[1], Uuid())[0]

        media = Media(self.connection, result.actor, result.uuid)
        audiosource = media.add_media_source(audio_path)
        if audiosource:
            if audio_offset != 0:
                mr = audiosource.media_reference
                mr.set_offset(audio_offset)
                audiosource.media_reference = mr

            media.set_media_source(audiosource, MediaType.MT_AUDIO)

        return media

    def add_media(self, path, frame_list=None):
        """Add media from path.

        Args:
            path(str/URI): path to collect media from.

        Returns:
            media(Media): Media.
        """
        if isinstance(path, URI):
            result = self.connection.request_receive(self.remote, add_media_atom(), path, False, Uuid())[0][0]
        else:
            ppp = parse_posix_path(path)

            if not str(ppp[1]) and frame_list is None:
                result = self.connection.request_receive(self.remote, add_media_atom(), path, ppp[0], Uuid())[0]
            else:
                result = self.connection.request_receive(
                    self.remote, add_media_atom(), path, ppp[0], ppp[1] if frame_list is None else frame_list, Uuid()
                )[0]

        return Media(self.connection, result.actor, result.uuid)

    @property
    def media(self):
        """Get media.

        Returns:
            media(list[Media]): media in playlist.
        """
        result = self.connection.request_receive(self.remote, get_media_atom())[0]
        return [Media(self.connection, i.actor, i.uuid) for i in result]

    @property
    def containers(self):
        """Get containers.

        Returns:
            containers(list[Subset/Timeline/ContactSheet]): containers in playlist.
        """

        result = self.connection.request_receive(self.remote, get_container_atom(), True)[0]
        # vector of uuidactors
        _containers = []
        # need to determine type..
        for i in result:
            # send request to actor to get type..
            actor_type = self.connection.request_receive(i.actor, type_atom())[0]
            if actor_type == "Subset":
                _containers.append(Subset(self.connection, i.actor, i.uuid))
            elif actor_type == "ContactSheet":
                _containers.append(ContactSheet(self.connection, i.actor, i.uuid))
            elif actor_type == "Timeline":
                _containers.append(Timeline(self.connection, i.actor, i.uuid))

        return _containers

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

    def create_playhead(self):
        """Create new playhead.

        Returns:
            playhead(Playhead): Returns playhead
        """
        result = self.connection.request_receive(self.remote, create_playhead_atom())[0]
        return Playhead(self.connection, result.actor, result.uuid)

    @property
    def playlist_tree(self):
        """Get playlist tree.

        Returns:
            playlist_tree(PlaylistTree): PlaylistTree in playlist, how items are organised.
        """
        result = self.connection.request_receive(self.remote, get_container_atom())[0]
        return PlaylistTree(self.connection, self.remote, result)


    def create_group(self, name, before=Uuid()):
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

        uuid =  self.connection.request_receive(self.remote, create_divider_atom(), name, before, into)[0]
        result =  self.connection.request_receive(self.remote, get_container_atom(), uuid)[0]

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

    def create_subset(self, name="Subset", before=Uuid(), into=False):
        """Create new subset.

        Kwargs:
            name(str): Name of new subset
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            subset(Uuid,Subset): Returns container Uuid and Subset.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, create_subset_atom(), name, before, into)[0]

        return (result[0], Subset(self.connection, result[1].actor, result[1].uuid))

    def import_timeline(self, path, name=None, before=Uuid()):
        """Create new timeline from otio.

        Args:
            path(str): Path to otio.

        Kwargs:
            name(str): Name of new timeline
            before(Uuid): Insert before this item.

        Returns:
            timeline(Uuid,Timeline): Returns container Uuid and Timeline.
        """
        return import_timeline_from_file(self, path, name, before)


    def create_timeline(self, name="Timeline", before=Uuid(), into=False):
        """Create new timeline.

        Kwargs:
            name(str): Name of new timeline
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            timeline(Uuid,Timeline): Returns container Uuid and Timeline.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, create_timeline_atom(), name, before, into)[0]

        return (result[0], Timeline(self.connection, result[1].actor, result[1].uuid))

    def create_contact_sheet(self, name="ContactSheet", before=Uuid(), into=False):
        """Create new contact sheet.

        Kwargs:
            name(str): Name of new contact sheet
            before(Uuid): Insert before this item.
            into(bool): Insert into this container (not used).

        Returns:
            subset(Uuid,ContactSheet): Returns container Uuid and contact sheet.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        result = self.connection.request_receive(self.remote, create_contact_sheet_atom(), name, before, into)[0]
        return (result[0], ContactSheet(self.connection, result[1].actor, result[1].uuid))

    @property
    def media_rate(self):
        """Get new media rate.

        Returns:
            media_rate(core.FrameRate): Media rate to use for rateless media.
        """
        return self.connection.request_receive(self.remote, media_rate_atom())[0]

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
    def playhead_selection(self):
        """The actor that filters a selection of media from a playhead
        and passes to a playhead for playback.

        Returns:
            source(PlayheadSelection): Currently playing this.
        """
        result =  self.connection.request_receive(self.remote, selection_actor_atom())[0]
        return PlayheadSelection(self.connection, result)

    def remove_media(self, media):
        """Remove media from playlist.

        Args:
            media(Media/Uuid or list of): Item to remove.

        Returns:
            success(bool): Returns result.
        """
        if isinstance(media, Media):
            media = media.uuid

        if isinstance(media, list):
            media_list = UuidVec()
            for m in media:
                if isinstance(m, Media):
                    media_list.push_back(m.uuid)
                else:
                    media_list.push_back(m)

            media = media_list

        return self.connection.request_receive(self.remote, remove_media_atom(), media)[0]

    def move_media(self, media, before=Uuid()):
        """Move media in tree.

        Args:
            media(Media/Uuid): Media to move.

        Kwargs:
            before(Uuid): Insert before this item.

        Returns:
            success(bool): Returns result.
        """
        if isinstance(media, Media):
            media = media.uuid

        if isinstance(before, Media):
            before = before.uuid

        return self.connection.request_receive(self.remote, move_media_atom(), media, before)[0]

    def convert_to_subset(self, src, name="Converted", before=Uuid()):
        """Convert src to Subset.

        Args:
            src(Container/Uuid): Container to convert.

        Kwargs:
            name(str): Name of new item.
            before(Uuid): Insert before this item.

        Returns:
            subset(Uuid,Subset): Returns container Uuid and subset.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        if isinstance(src, Container):
            src = src.uuid

        result = self.connection.request_receive(self.remote, convert_to_subset_atom(), src, name, before)[0]

        return (result[0], Subset(self.connection, result[1].actor, result[1].uuid))

    def convert_to_contact_sheet(self, src, name="Converted", before=Uuid()):
        """Convert src to ContactSheet.

        Args:
            src(Container/Uuid): Container to convert.

        Kwargs:
            name(str): Name of new item.
            before(Uuid): Insert before this item.

        Returns:
            subset(Uuid,ContactSheet): Returns container Uuid and subset.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        if isinstance(src, Container):
            src = src.uuid

        result = self.connection.request_receive(self.remote, convert_to_contact_sheet_atom(), src, name, before)[0]
        return (result[0], ContactSheet(self.connection, result[1].actor, result[1].uuid))

    def convert_to_timeline(self, src, name="Converted", before=Uuid()):
        """Convert src to Timeline.

        Args:
            src(Container/Uuid): Container to convert.

        Kwargs:
            name(str): Name of new item.
            before(Uuid): Insert before this item.

        Returns:
            subset(Uuid,Timeline): Returns container Uuid and subset.
        """
        if not isinstance(before, Uuid):
            before = before.uuid

        if isinstance(src, Container):
            src = src.uuid

        result = self.connection.request_receive(self.remote, convert_to_timeline_atom(), src, name, before)[0]
        return (result[0], Timeline(self.connection, result[1].actor, result[1].uuid))

    @property
    def metadata(self):
        """Get metadata.

        Returns:
            metadata(json): Metadata attached to playlist.
        """

        return json.loads(self.connection.request_receive(self.remote, get_json_atom(), "")[0].dump())

    @metadata.setter
    def metadata(self, new_metadata):
        """Set media reference rate.

        Args:
            new_metadata(json): Json dict to set as media source metadata

        Returns:
            bool: success

        """
        return self.connection.request_receive(self.remote, set_json_atom(), JsonStore(new_metadata))

    def get_metadata(self, path):
        """Get metdata at JSON path

        Args:
            path(str): JSON Pointer

        Returns:
            metadata(json) Json at pointer location
        """

        return json.loads(self.connection.request_receive(self.remote, get_json_atom(), path)[0].dump())

    def set_metadata(self, data, path):
        """Get metdata at JSON path

        Args:
            data(json): JSON Data
            path(str): JSON Pointer

        Returns:
            bool: success
        """

        return self.connection.request_receive(self.remote, set_json_atom(), JsonStore(data), path)[0]

from xstudio.api.auxiliary.otio import import_timeline_from_file
