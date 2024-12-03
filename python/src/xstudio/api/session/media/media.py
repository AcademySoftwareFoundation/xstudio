# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_media_source_atom, current_media_source_atom, get_json_atom, get_metadata_atom, reflag_container_atom, rescan_atom
from xstudio.core import invalidate_cache_atom, get_media_pointer_atom, MediaType, Uuid, media_status_atom
from xstudio.core import add_media_source_atom, FrameRate, FrameList, parse_posix_path, URI, MediaStatus
from xstudio.core import set_json_atom, JsonStore, quickview_media_atom

from xstudio.api.session.container import Container
from xstudio.api.session.media.media_source import MediaSource

import json

class Media(Container):
    """Media object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Media object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def media_sources(self):
        """Get media sources.

        Returns:
            media_sources(list[MediaSource]): List of media sources.
        """

        result = self.connection.request_receive(self.remote, get_media_source_atom())[0]
        _media_sources = []

        for i in result:
            _media_sources.append(MediaSource(self.connection, i.actor, i.uuid))

        return _media_sources


    @property
    def status(self):
        """Get media status

        Returns:
            status(MediaStatus): Status of current media source.
        """

        return self.connection.request_receive(self.remote, media_status_atom())[0]

    @property
    def is_online(self):
        """Get media status

        Returns:
            status(MediaStatus): Status of current media source.
        """

        return self.status == MediaStatus.MS_ONLINE

    @property
    def flag_colour(self):
        """Get media flag colour.

        Returns:
            colour(string): Colour of flag.
        """

        return self.connection.request_receive(self.remote, reflag_container_atom())[0][0]

    @property
    def flag_text(self):
        """Get media flag text.

        Returns:
            colour(string): Colour of flag.
        """

        return self.connection.request_receive(self.remote, reflag_container_atom())[0][1]

    @flag_colour.setter
    def flag_colour(self, colour):
        """Set media flag colour.

        Args:
            colour(string): colour string

        Returns:
            bool: success

        """
        return self.reflag(colour, self.flag_text)

    @flag_text.setter
    def flag_text(self, text):
        """Set media flag text.

        Args:
            text(string): text string

        Returns:
            bool: success

        """
        return self.reflag(self.flag_colour, text)

    def media_source(self, media_type=MediaType.MT_IMAGE):
        """Get current media source.
        Args:
            media_type(MediaType): Media Source type

        Returns:
            media_source(MediaSource): Current media source.
        """
        result = self.connection.request_receive(self.remote, current_media_source_atom(), media_type)[0]
        return MediaSource(self.connection, result.actor, result.uuid)

    def acquire_metadata(self):
        """Acquire media metadata.

        Returns:
            success(bool): Metadata acquired.

        """
        return self.connection.request_receive(self.remote, get_metadata_atom())[0]

    def invalidate_cache(self):
        """Flush media item from cache.

        Returns:
            result(bool): Flushed ?
        """
        return self.connection.request_receive(self.remote, invalidate_cache_atom())[0]

    def rescan(self):
        """Rescan media item.

        Returns:
            result(MediaReference): New ref ?
        """
        return self.connection.request_receive(self.remote, rescan_atom())[0]

    @property
    def metadata(self):
        """Get media metadata.

        Returns:
            metadata(json): Media metadata.
        """
        return json.loads(self.connection.request_receive(self.remote, get_json_atom(), Uuid(), "")[0].dump())

    @metadata.setter
    def metadata(self, new_metadata):
        """Set media reference rate.

        Args:
            new_metadata(json): Json dict to set as media source metadata

        Returns:
            bool: success

        """
        return self.connection.request_receive(self.remote, set_json_atom(), Uuid(), JsonStore(new_metadata))

    def get_metadata(self, path):
        """Get metdata at JSON path

        Args:
            path(str): JSON Pointer

        Returns:
            metadata(json) Json at pointer location
        """

        return json.loads(self.connection.request_receive(self.remote, get_json_atom(), Uuid(), path)[0].dump())

    def set_metadata(self, data, path):
        """Get metdata at JSON path

        Args:
            data(json): JSON Data
            path(str): JSON Pointer

        Returns:
            bool: success
        """

        return self.connection.request_receive(self.remote, set_json_atom(), Uuid(), JsonStore(data), path)[0]

    @property
    def source_metadata(self):
        """Get media metadata.

        Returns:
            metadata(json): Media metadata.
        """
        return json.loads(self.connection.request_receive(self.remote, get_json_atom(), "")[0].dump())

    def get_media_pointer(self, logical_frame=0, media_type=MediaType.MT_IMAGE):
        """Get Media Pointer

        Kwargs:
            logical_frame(int): Frame to get.
            media_type(MediaType): MediaType of pointer.

        Returns:
            media_pointer(MediaPointer): Media pointer.
        """
        return self.connection.request_receive(self.remote, get_media_pointer_atom(), media_type, logical_frame)[0]

    def add_media_source(self, path, frame_list=None, frame_rate=None):
        """Add media source from path

        Args:
            path(URI | str): File path
            frame_list(FrameList): Frame range
            frame_rate(FrameRate): Default frame rate

        Returns:
            media_source(MediaSource): New media source.
        """
        if frame_rate is None:
            frame_rate = FrameRate()

        if not isinstance(path, URI):
            (path, path_frame_list) = parse_posix_path(path)
            if frame_list is None:
                frame_list = path_frame_list

        if frame_list is None:
            frame_list = FrameList()

        result = self.connection.request_receive(self.remote, add_media_source_atom(), path, frame_list, frame_rate)[0]
        return MediaSource(self.connection, result.actor, result.uuid)

    def set_media_source(self, source, media_type=MediaType.MT_IMAGE):
        """Set current media source.

         Args:
            source(MediaSource|Uuid): Media Source
            media_type(MediaType): Media Source type

       Returns:
            bool(Success): Success
        """

        if not isinstance(source, Uuid):
            source = source.uuid

        return self.connection.request_receive(self.remote, current_media_source_atom(), source, media_type)[0]

    def ordered_bookmarks(self):
        """Get all bookmarks on this media source, ordered by start frame

        Returns:
            list(Bookmark)
        """
        all_bookmarks = self.connection.api.session.bookmarks
        result = []
        for bookmark in all_bookmarks.bookmarks:
            if bookmark.detail.owner.uuid == self.uuid:
                result.append(bookmark)

        result = sorted(result, key=lambda x: x.detail.start)
        return result

    def reflag(self, flag_colour, flag_string):
        """Reflag media.

        Args:
            flag(str): New flag colour.
            flag_string(str): New flag text.

        Returns:
            success(bool): Returns result.
        """
        return self.connection.request_receive(self.remote, reflag_container_atom(), flag_colour, flag_string)[0]