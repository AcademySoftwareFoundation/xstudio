# SPDX-License-Identifier: Apache-2.0

from xstudio.core import insert_item_atom, remove_item_atom, erase_item_atom, move_item_atom
from xstudio.core import Uuid, actor, UuidActor, ItemType, UuidActorVec
from xstudio.api.session.playlist.timeline.item import Item
from xstudio.api.session.playlist.timeline import create_item_container

class Stack(Item):
    """Timeline object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Stack object.

        Derived from Container.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Item.__init__(self, connection, remote, uuid)

    def __len__(self):
        """Get size.

        Returns:
            size(int): Size.
        """
        return len(self.item)

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

    def remove_child(self, index=-1):
        """Remove child obj

        Args:
            index(int): Index to remove

        Returns:
            item(Item): Item removed
        """
        return self.connection.request_receive(self.remote, remove_item_atom(), index)[0]

    def erase_child(self, index=-1):
        """Remove and destroy child obj

        Args:
            index(int): Index to erase

        Returns:
            success(bool): Item erased
        """
        return self.connection.request_receive(self.remote, erase_item_atom(), index, True)[0]

    def move_children(self, start, count, dest):
        """Move child items

        Args:
            start(int): Index of first item
            count(int): Count items
            dest(int): Destination index (-1 for append)

        Returns:
            success(bool): Items moved
        """
        return self.connection.request_receive(self.remote, move_item_atom(), start, count, dest)[0]

    def children_of_type(self, types):
        """Get children matching types.

        Args:
            types([ItemType]): List of item types.

        Returns:
            children([item]): Children.
        """
        return [create_item_container(self.connection, i) for i in self.item.children() if i.item_type() in types]
