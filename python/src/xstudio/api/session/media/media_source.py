# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_media_stream_atom, current_media_stream_atom, MediaType, media_reference_atom, rescan_atom, invalidate_cache_atom
from xstudio.core import media_status_atom, get_json_atom, set_json_atom, JsonStore

from xstudio.api.session.container import Container
from xstudio.api.session.media.media_stream import MediaStream

import json

class MediaSource(Container):
    """MediaSource object."""

    def __init__(self, connection, remote, uuid=None):
        """Create MediaSource object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)
        self._streams = {
            str(MediaType.MT_IMAGE): None,
            str(MediaType.MT_AUDIO): None
        }

    @property
    def image_streams(self):
        """Get image streams.

        Returns:
            image_streams(list[MediaStream]): List of image streams.
        """
        return self.streams(MediaType.MT_IMAGE)

    @property
    def audio_streams(self):
        """Get audio streams.

        Returns:
            audio_streams(list[MediaStream]): List of audio streams.
        """
        return self.streams(MediaType.MT_AUDIO)

    def streams(self, media_type=MediaType.MT_IMAGE):
        """Get streams.

        Kwargs:
            media_type(MediaType): Type of stream.

        Returns:
            streams(list[MediaStream]): List of streams.
        """
        media_type_str = str(media_type)
        if self._streams[media_type_str] is None:
            self._streams[media_type_str] = []

            result = self.connection.request_receive(self.remote, get_media_stream_atom(), media_type)[0]

            for i in result:
                self._streams[media_type_str].append(MediaStream(self.connection, i.actor, i.uuid))

        return self._streams[media_type_str]

    @property
    def rate(self):
        """Get media reference rate.

        Returns:
            rate(FrameRate): Current media reference rate.
        """
        try:
            return self.media_reference.rate()
        except RuntimeError:
            return None

    @rate.setter
    def rate(self, new_rate):
        """Set media reference rate.

        Args:
            new_rate(FrameRate): Set media reference rate.
        """

        try:
            mr = self.media_reference
            mr.set_rate(new_rate)
            self.media_reference = mr
        except RuntimeError:
            pass

    @property
    def media_reference(self):
        """Get media reference.

        Returns:
            media_reference(MediaReference): Current media reference.
        """
        try:
            return self.connection.request_receive(self.remote, media_reference_atom())[0]
        except RuntimeError:
            return None

    @media_reference.setter
    def media_reference(self, mr):
        """Set media reference.

        Args:
            mr(MediaReference): Set media reference.
        """

        try:
            self.connection.request_receive(self.remote, media_reference_atom(), mr)
        except RuntimeError:
            pass

    def rescan(self):
        """Rescan media item.

        Returns:
            result(MediaReference): New Reference
        """
        return self.connection.request_receive(self.remote, rescan_atom())[0]

    def invalidate_cache(self):
        """Flush media item from cache.

        Returns:
            result(bool): Flushed ?
        """
        return self.connection.request_receive(self.remote, invalidate_cache_atom())[0]

    @property
    def metadata(self):
        """Get media metadata.

        Returns:
            metadata(json): Media metadata.
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

    @property
    def image_stream(self):
        """Get current image stream.

        Returns:
            image_stream(MediaStream): Current image stream.
        """
        try:
            result = self.connection.request_receive(self.remote, current_media_stream_atom(), MediaType.MT_IMAGE)[0]
            return MediaStream(self.connection, result.actor, result.uuid)
        except RuntimeError:
            return None

    @property
    def audio_stream(self):
        """Get current audio stream.

        Returns:
            audio_stream(MediaStream): Current audio stream.
        """
        try:
            result = self.connection.request_receive(self.remote, current_media_stream_atom(), MediaType.MT_AUDIO)[0]
            return MediaStream(self.connection, result.actor, result.uuid)
        except RuntimeError:
            return None


    @property
    def media_status(self):
        """Get status.

        Returns:
            status(MediaStatus): State.
        """
        return self.connection.request_receive(self.remote, media_status_atom())[0]

    @media_status.setter
    def media_status(self, status):
        """Set status.

        Args:
            status(MediaStatus): Set status state.
        """
        self.connection.request_receive(self.remote, media_status_atom(), status)