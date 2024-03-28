# SPDX-License-Identifier: Apache-2.0
from xstudio.core import Uuid, actor, UuidActor, ItemType, item_flag_atom
from xstudio.core import item_atom, insert_item_atom, enable_atom, remove_item_atom, erase_item_atom, item_name_atom, move_item_atom
from xstudio.core import active_range_atom, available_range_atom
from xstudio.api.session.container import Container
from xstudio.api.session.playlist.timeline import create_item_container

class Stack(Container):
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
        Container.__init__(self, connection, remote, uuid)

    @property
    def item(self):
        """Get item.

        Returns:
            item(Item): Item for timeline.
        """
        return self.connection.request_receive(self.remote, item_atom())[0]

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

        return self.connection.request_receive(self.remote, insert_item_atom(), index, obj)[0]

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
        return self.connection.request_receive(self.remote, erase_item_atom(), index)[0]

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


    @property
    def children(self):
        """Get children.

        Returns:
            children([Gap/Track/Clip/Stack]): Children.
        """
        children = []

        for i in self.item.children():
            children.append(create_item_container(self.connection, i))

        return children

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
