# SPDX-License-Identifier: Apache-2.0
from xstudio.core import UuidActor, Uuid, actor, item_atom, MediaType, ItemType, enable_atom, item_flag_atom
from xstudio.core import active_range_atom, available_range_atom, undo_atom, redo_atom, history_atom, add_media_atom, item_name_atom
from xstudio.core import URI, selection_actor_atom, item_selection_atom, item_type_atom, get_media_atom, save_atom, export_atom
from xstudio.core import import_atom, erase_item_atom, get_playhead_atom, FrameRate, FrameRateDuration
from xstudio.api.session.container import Container
from xstudio.api.intrinsic import History
from xstudio.api.session.media.media import Media
from xstudio.api.session.playlist.timeline.item import Item
from xstudio.api.session.playhead import Playhead, PlayheadSelection
from xstudio.api.auxiliary import NotificationHandler

def create_gap(connection, name="Gap"):
    """Create Gap object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Gap name.

    Returns:
        gap(Gap): Gap object.
    """

    return Gap(connection, connection.remote_spawn("Gap", name, FrameRateDuration()))

def create_clip(connection, media, name="Clip"):
    """Create Clip object.

    Args:
        connection(Connection): Connection object
        media(UuidActor): Media object

    Kwargs:
        name(Str): Clip name.

    Returns:
        clip(Clip): Clip object.
    """

    return Clip(connection, connection.remote_spawn("Clip", media, name))

def create_stack(connection, name="Stack"):
    """Create Stack object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Stack name.

    Returns:
        stack(Stack): Stack object.
    """

    return Stack(connection, connection.remote_spawn("Stack", name))

def create_video_track(connection, name="Video Track"):
    """Create Track object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Track name.

    Returns:
        track(Track): Track object.
    """

    return Track(ItemType.IT_VIDEO_TRACK, connection, connection.remote_spawn("Track", name, FrameRate(), MediaType.MT_IMAGE))

def create_audio_track(connection, name="Audio Track"):
    """Create Track object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Track name.

    Returns:
        track(Track): Track object.
    """

    return Track(ItemType.IT_AUDIO_TRACK, connection, connection.remote_spawn("Track", name, FrameRate(), MediaType.MT_AUDIO))

def create_item_container(connection, item):
    return create_item_container_from_type(connection, item.item_type(), item.uuid(), item.actor())

def create_item_container_from_type(connection, item_type, uuid, actor):
    result = None
    if item_type == ItemType.IT_GAP:
        result = Gap(connection, actor, uuid)
    elif item_type == ItemType.IT_CLIP:
        result =  Clip(connection, actor, uuid)
    elif item_type == ItemType.IT_STACK:
        result =  Stack(connection, actor, uuid)
    elif item_type == ItemType.IT_VIDEO_TRACK:
        result =  Track(ItemType.IT_VIDEO_TRACK, connection, actor, uuid)
    elif item_type == ItemType.IT_AUDIO_TRACK:
        result =  Track(ItemType.IT_AUDIO_TRACK, connection, actor, uuid)
    elif item_type == ItemType.IT_TIMELINE:
        result =  Timeline(connection, actor, uuid)
    else:
        raise RuntimeError("Invalid type " + item_type)

    return result


class Timeline(Item, NotificationHandler):
    """Timeline object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Timeline object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Item.__init__(self, connection, remote, uuid)
        NotificationHandler.__init__(self, self)

    def __len__(self):
        """Get size.

        Returns:
            size(int): Size.
        """
        return len(self.item)

    @property
    def stack(self):
        """Get stack.

        Returns:
            stack(Stack): Stack for timeline.
        """
        # it may not exist..
        item = self.connection.request_receive(self.remote, item_atom(), 0)[0]
        return create_item_container(self.connection, item)

    @property
    def selection(self):
        """Get currently selected items

        Returns:
            list([item]): Selected Items
        """

        items = self.connection.request_receive(self.remote, item_selection_atom())[0]
        result = []

        for i in items:
            item_type = self.connection.request_receive(i.actor, item_type_atom())[0]
            result.append(
                create_item_container_from_type(self.connection, item_type, i.uuid, i.actor)
            )

        return result

    @property
    def children(self):
        """Get children.

        Returns:
            children([Stack]): Children.
        """
        return [self.stack]

    def children_of_type(self, types):
        """Get children matching types.

        Args:
            types([ItemType]): List of item types.

        Returns:
            children([item]): Children.
        """
        return [create_item_container(self.connection, i) for i in self.item.children() if i.item_type() in types]

    @property
    def tracks(self):
        """Get children.

        Returns:
            children([Stack]): Children.
        """
        return self.stack.children

    @property
    def audio_tracks(self):
        """Get children.

        Returns:
            children([Stack]): Children.
        """
        return self.stack.children_of_type([ItemType.IT_AUDIO_TRACK])

    @property
    def video_tracks(self):
        """Get children.

        Returns:
            children([Stack]): Children.
        """
        return self.stack.children_of_type([ItemType.IT_VIDEO_TRACK])

    @property
    def history(self):
        """Get History.

        Returns:
            history(History): History actor.
        """
        result = self.connection.request_receive(self.remote, history_atom())[0]
        return History(self.connection, result.actor, result.uuid)

    def undo(self):
        return self.connection.request_receive(self.remote, undo_atom())[0]

    def redo(self):
        return self.connection.request_receive(self.remote, redo_atom())[0]

    def clear(self):
        """Clear all video and audio tracks in the stack"""
        while len(self.tracks):
            print (self.tracks)
            self.stack.erase_child(0)

    def create_gap(self, name="Gap"):
        """Create Gap object.

        Kwargs:
            name(Str): Gap name.

        Returns:
            gap(Gap): Gap object.
        """
        return create_gap(self.connection, name)

    def create_clip(self, media, name="Clip"):
        """Create Clip object.

        Args:
            media(Media/UuidActor): Media contained in clip.

        Kwargs:
            name(Str): Clip name.

        Returns:
            clip(Clip): Clip object.
        """
        if not isinstance(media, UuidActor):
            media = media.uuid_actor()

        return create_clip(self.connection, media, name)

    def create_stack(self, name="Stack"):
        """Create Stack object.

        Kwargs:
            name(Str): Stack name.

        Returns:
            stack(Stack): Stack object.
        """
        return create_stack(self.connection, name)

    def create_audio_track(self, name="Audio Track"):
        """Create Track object.

        Kwargs:
            name(Str): Track name.

        Returns:
            track(Track): Track object.
        """
        return create_audio_track(self.connection, name)

    def create_video_track(self, name="Video Track"):
        """Create Track object.

        Kwargs:
            name(Str): Track name.

        Returns:
            track(Track): Track object.
        """
        return create_video_track(self.connection, name)

    def insert_video_track(self, name="Video Track", index=0):
        """Insert video track

        Args:
            name(str): Name of new track.
            index(int): Position to insert

        Returns:
            success(Item): New item
        """

        result = Track(ItemType.IT_VIDEO_TRACK, self.connection, self.connection.remote_spawn("Track", name, self.rate, MediaType.MT_IMAGE))
        self.stack.insert_child(result, index)
        return result

    def insert_audio_track(self, name="Audio Track", index=-1):
        """Insert audio track

        Args:
            name(str): Name of new track.
            index(int): Position to insert

        Returns:
            success(Item): New item
        """

        result = Track(ItemType.IT_AUDIO_TRACK, self.connection, self.connection.remote_spawn("Track", name, self.rate, MediaType.MT_AUDIO))
        self.stack.insert_child(result, index)
        return result


    def export_timeline_to_file(self, path, adapter_name=None):
        """Create otio from timeline.

        Args:
            path(str): Path to write to

        Kwargs:
            adapter_name(str): Override adaptor.
        """
        return export_timeline_to_file(self, path, adapter_name)

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

        return result

    def load_otio(self, otio_body, path="", clear=False):
        """Load otio json data into the timeline. Replaces entire timeline
        with the incoming otio

        Args:
            otio_body(json dict): OTIO data

        Kwargs:
            path(str): Path that otio_body was loaded from
            clear(bool): Clear timeline completely before load

        Returns:
            seccess(bool): True on successful load from OTIO.
        """
        return self.connection.request_receive(
            self.remote,
            import_atom(),
            URI(path) if path else URI(),
            otio_body,
            clear)[0]

    def export_otio(self, path):
        """Export timeline via OpenTimelineIO. File path extension infers the
        format of the exported file.

        Args:
            path(str/uri): Path to expor to.

        Returns:
            bool: True on success, False on failure
        """
        otio_string = self.connection.request_receive(self.remote, export_atom())[0]

        from opentimelineio.adapters import read_from_string, write_to_file
        otio_object = read_from_string(otio_string)
        return write_to_file(otio_object, path)

    @property
    def audio_tracks(self):
        return self.stack.audio_tracks

    @property
    def video_tracks(self):
        return self.stack.video_tracks

    @property
    def media(self):
        """Get media in timeline

        Returns:
            media(list[media]): Media
        """
        result = self.connection.request_receive(self.remote, get_media_atom())[0]
        return [Media(self.connection, i.actor, i.uuid) for i in result]


    @property
    def playhead(self):
        """Get playhead.

        Returns:
            playhead(Playhead): Playhead attached to timeline.
        """
        result = self.connection.request_receive(self.remote, get_playhead_atom())[0]

        return Playhead(self.connection, result.actor, result.uuid)


    @property
    def playhead_selection(self):
        """The actor that filters a selection of media from a playhead
        and passes to a playhead for playback.

        Returns:
            source(PlayheadSelection): Currently playing this.
        """
        result =  self.connection.request_receive(self.remote, selection_actor_atom())[0]
        return PlayheadSelection(self.connection, result)

    # @property
    # def audio_tracks(self):
    #     """Get children.

    #     Returns:
    #         children([Stack]): Children.
    #     """
    #     return [if for x in self.stack.children]

    # @property
    # def video_tracks(self):
    #     """Get children.

    #     Returns:
    #         children([Stack]): Children.
    #     """
    #     return self.stack.children

from xstudio.api.session.playlist.timeline.gap import Gap
from xstudio.api.session.playlist.timeline.clip import Clip
from xstudio.api.session.playlist.timeline.stack import Stack
from xstudio.api.session.playlist.timeline.track import Track
from xstudio.api.auxiliary.otio import export_timeline_to_file
