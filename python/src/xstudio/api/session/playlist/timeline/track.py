# SPDX-License-Identifier: Apache-2.0
from xstudio.core import insert_item_atom, remove_item_atom, erase_item_atom, move_item_atom
from xstudio.core import split_item_at_frame_atom, split_item_atom, erase_item_at_frame_atom
from xstudio.core import move_item_at_frame_atom
from xstudio.core import Uuid, actor, UuidActor, ItemType, UuidActorVec, FrameRateDuration
from xstudio.api.session.playlist.timeline.item import Item
from xstudio.api.session.playlist.timeline import create_item_container
from xstudio.api.session.playlist.timeline.gap import Gap
from xstudio.api.session.playlist.timeline.clip import Clip
from xstudio.api.auxiliary import NotificationHandler

class Track(Item, NotificationHandler):
    """Timeline object."""

    def __init__(self, item_type, connection, remote, uuid=None):
        """Create Track object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Item.__init__(self, connection, remote, uuid)
        NotificationHandler.__init__(self, self)
        self.item_type = item_type

    @property
    def is_audio(self):
        """Is audio track

        Returns:
            is_audio(bool): Is audio track.
        """
        return self.item_type == ItemType.IT_AUDIO_TRACK

    @property
    def is_video(self):
        """Is video track

        Returns:
            is_video(bool): Is video track.
        """
        return self.item_type == ItemType.IT_VIDEO_TRACK

    @property
    def clips(self):
        return self.children_of_type([ItemType.IT_CLIP])

    @property
    def gaps(self):
        return self.children_of_type([ItemType.IT_GAP])

    def __len__(self):
        """Get size.

        Returns:
            size(int): Size.
        """
        return len(self.item)

    def insert_gap(self, frames, index=-1):
        """Insert gap
        Args:
            frames(int): Duration in frames.
            index(int): Position to insert

        returns success(Item): New item
        """
        result = Gap(self.connection,
            self.connection.remote_spawn("Gap", "Gap", FrameRateDuration(frames, self.rate))
        )

        self.insert_child(result, index)

        return result

    def insert_clip(self, media, index=-1):
        """Insert gap
        Args:
            media(Media): Media to insert, MUST already exist in timeline.
            index(int): Position to insert

        returns success(Item): New item
        """
        result = Clip(self.connection,
            self.connection.remote_spawn("Clip",  media.uuid_actor(), "")
        )

        self.insert_child(result, index)

        return result

    def insert_child(self, obj, index=-1):
        """Insert child obj

        Args:
            obj(ActorUuid/Gap/Track/Clip/Stack): Object to insert.
            index(int): Position to insert

        Returns:
            success(bool): Success
        """

        if not isinstance(obj, UuidActor):
            obj = obj.uuid_actor()

        uav = UuidActorVec()
        uav.push_back(obj)

        return self.connection.request_receive(self.remote, insert_item_atom(), index, uav)[0]

    def remove_child_at_index(self, index=-1, count=1, add_gap=True):
        """Remove child obj, removed item must be released or reparented.

        Args:
            index(int): Index to remove

        Returns:
            item(Item): Item removed
        """
        return self.connection.request_receive(self.remote, remove_item_atom(), index, count, add_gap)[0]


    def erase_frames(self, frame=0, duration=1, add_gap=True):
        """Remove and destroy frames
        Args:
            frame(int): start frame
            duration(int): Duration frames
            add_gap(bool): Replace with gap

        Returns:
            Transaction(JsonStore): Transaction.
        """
        return self.connection.request_receive(self.remote, erase_item_at_frame_atom(), frame, duration, add_gap)[0]

    def erase_child_at_index(self, index=-1, count=1, add_gap=True):
        """Remove and destroy child obj

        Args:
            index(int): Index to erase

        Returns:
            success(bool): Item erased
        """
        return self.connection.request_receive(self.remote, erase_item_atom(), index, count, add_gap)[0]

    def remove_child(self, child, add_gap=True):
        """Remove child obj, removed item must be released or reparented.

        Args:
            child(Item): Child to remove

        Returns:
            item(Item): Child removed
        """
        return self.connection.request_receive(self.remote, remove_item_atom(), child.uuid, add_gap)[0]

    def erase_child(self, child, add_gap=True):
        """Remove and destroy child obj

        Args:
            child(Item): Child to erase

        Returns:
            success(bool): Item erased
        """
        return self.connection.request_receive(self.remote, erase_item_atom(), child.uuid, add_gap)[0]

    def move_children(self, start, count, dest, add_gap=False):
        """Move child items

        Args:
            start(int): Index of first item
            count(int): Count items
            dest(int): Destination index (-1 for append)

        Returns:
            success(bool): Items moved
        """
        return self.connection.request_receive(self.remote, move_item_atom(), start, count, dest, add_gap)[0]

    def move_frames(self, frame, duration, destination, insert=True, add_gap=False):
        """Move frames

        Args:
            frame(int): First frame
            duration(int): Duration in frames
            destination(int): Destination frame
            insert(bool): Insert at destination frame, overwrite if False
            add_gap(bool): Add gap at source

        Returns:
            success(bool): Items moved
        """
        return self.connection.request_receive(self.remote, move_item_at_frame_atom(), frame, duration, destination, insert, add_gap)[0]


    def split_child_at_index(self, index, frame):
        """Split child obj

        Args:
            index(int): Index to split
            frame(int): Frame of child to split

        Returns:
            success(bool): Item split
        """
        return self.connection.request_receive(self.remote, split_item_atom(), index, frame)[0]

    def split_child(self, child, frame):
        """Split child obj

        Args:
            child(Item): Child to split
            frame(int): Frame of child to split

        Returns:
            success(bool): Item split
        """
        return self.connection.request_receive(self.remote, split_item_atom(), child.uuid, frame)[0]

    def split(self, frame):
        """Split track item at frame

        Args:
            frame(int): Frame of track to split

        Returns:
            success(bool): Item split
        """
        return self.connection.request_receive(self.remote, split_item_at_frame_atom(), frame)[0]

    def children_of_type(self, types):
        """Get children matching types.

        Args:
            types([ItemType]): List of item types.

        Returns:
            children([item]): Children.
        """
        return [create_item_container(self.connection,i) for i in self.item.children() if i.item_type() in types]
