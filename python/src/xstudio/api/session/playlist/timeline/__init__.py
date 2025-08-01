# SPDX-License-Identifier: Apache-2.0
from xstudio.core import UuidActor, Uuid, actor, item_atom, MediaType, ItemType, enable_atom, item_flag_atom
from xstudio.core import active_range_atom, available_range_atom, undo_atom, redo_atom, history_atom, add_media_atom, item_name_atom
from xstudio.api.session.container import Container
from xstudio.api.intrinsic import History
from xstudio.api.session.media.media import Media

def create_gap(connection, name="Gap"):
    """Create Gap object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Gap name.

    Returns:
        gap(Gap): Gap object.
    """

    return Gap(connection, connection.remote_spawn("Gap",name))

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

    return Track(ItemType.IT_VIDEO_TRACK, connection, connection.remote_spawn("Track", name, MediaType.MT_IMAGE))

def create_audio_track(connection, name="Audio Track"):
    """Create Track object.

    Args:
        connection(Connection): Connection object

    Kwargs:
        name(Str): Track name.

    Returns:
        track(Track): Track object.
    """

    return Track(ItemType.IT_AUDIO_TRACK, connection, connection.remote_spawn("Track", name, MediaType.MT_AUDIO))

def create_item_container(connection, item):
    result = None
    if item.item_type() == ItemType.IT_GAP:
        result = Gap(connection, item.actor(), item.uuid())
    elif item.item_type() == ItemType.IT_CLIP:
        result =  Clip(connection, item.actor(), item.uuid())
    elif item.item_type() == ItemType.IT_STACK:
        result =  Stack(connection, item.actor(), item.uuid())
    elif item.item_type() == ItemType.IT_VIDEO_TRACK:
        result =  Track(ItemType.IT_VIDEO_TRACK, connection, item.actor(), item.uuid())
    elif item.item_type() == ItemType.IT_AUDIO_TRACK:
        result =  Track(ItemType.IT_AUDIO_TRACK, connection, item.actor(), item.uuid())
    elif item.item_type() == ItemType.IT_TIMELINE:
        result =  Timeline(connection, item.actor(), item.uuid())
    else:
        raise RuntimeError("Invalid type " + i.item_type())

    return result


class Timeline(Container):
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
        Container.__init__(self, connection, remote, uuid)

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
    def item_name(self):
        """Get name.

        Returns:
            name(str): Name.
        """
        return self.item.name()

    @item_name.setter
    def item_name(self, x):
        """Set name.

        Args:
            name(str): Set name.
        """
        self.connection.request_receive(self.remote, item_name_atom(), x)

    @property
    def item_flag(self):
        """Get flag.

        Returns:
            name(str): flag.
        """
        return self.item.flag()

    @item_flag.setter
    def item_flag(self, x):
        """Set flag.

        Args:
            name(str): Set flag.
        """
        self.connection.request_receive(self.remote, item_flag_atom(), x)

    @property
    def enabled(self):
        """Get enabled state.

        Returns:
            enabled(bool): State.
        """
        return self.item.enabled()

    @enabled.setter
    def enabled(self, x):
        """Set enabled state.

        Args:
            state(bool): Set enabled state.
        """
        self.connection.request_receive(self.remote, enable_atom(), x)

    @property
    def item(self):
        """Get item.

        Returns:
            item(Item): Item for timeline.
        """
        return self.connection.request_receive(self.remote, item_atom())[0]

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
    def trimmed_range(self):
        return self.item.trimmed_range()

    @property
    def available_range(self):
        return self.item.available_range()

    @available_range.setter
    def available_range(self, x):
        """Set available_range.

        Args:
            x(FrameRange): Set available_range.
        """
        self.connection.request_receive(self.remote, available_range_atom(), x)

    @property
    def active_range(self):
        return self.item.active_range()

    @active_range.setter
    def active_range(self, x):
        """Set active_range.

        Args:
            x(FrameRange): Set active_range.
        """
        self.connection.request_receive(self.remote, active_range_atom(), x)

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
            media = media.remote

        if not isinstance(before_uuid, Uuid):
            before_uuid = before_uuid.uuid

        result = self.connection.request_receive(self.remote, add_media_atom(), media, before_uuid)[0]

        return result

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
