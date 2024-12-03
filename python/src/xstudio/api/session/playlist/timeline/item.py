# SPDX-License-Identifier: Apache-2.0
# SPDX-License-Identifier: Apache-2.0
from xstudio.core import Uuid, actor, item_atom, enable_atom, item_prop_atom, item_lock_atom
from xstudio.core import active_range_atom, available_range_atom, item_name_atom, item_flag_atom
from xstudio.api.session.container import Container

import json

class Item(Container):
    """Timeline object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Gap object.

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
    def locked(self):
        """Get enabled state.

        Returns:
            enabled(bool): State.
        """
        return self.item.locked()

    @locked.setter
    def locked(self, x):
        """Set enabled state.

        Args:
            state(bool): Set enabled state.
        """
        self.connection.request_receive(self.remote, item_lock_atom(), x)

    @property
    def item_prop(self):
        """Get item.

        Returns:
            item(Item): Item for timeline.
        """
        return json.loads(self.connection.request_receive(self.remote, item_prop_atom())[0].dump())

    @item_prop.setter
    def item_prop(self, x):
        """Set enabled state.

        Args:
            state(bool): Set enabled state.
        """
        self.connection.request_receive(self.remote, item_prop_atom(), x)

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

    @property
    def item_flag_colour(self):
        """Get flag.

        Returns:
            name(str): flag.
        """
        colour = self.item.flag()

        colours = {
            "#FFFF0000": "Red",
            "#FF00FF00": "Green",
            "#FF0000FF": "Blue",
            "#FFFFFF00": "Yellow",
            "#FFFFA500": "Orange",
            "#FF800080": "Purple",
            "#FF000000": "Black",
            "#FFFFFFFF": "White",
        }

        return colours[colour] if colour in colours else colour

    @item_flag.setter
    def item_flag(self, x):
        """Set flag.

        Args:
            name(str): Set flag.
        """
        self.connection.request_receive(self.remote, item_flag_atom(), x)

    @property
    def children(self):
        """Get children.

        Returns:
            children([Gap/Track/Clip/Stack]): Children.
        """
        from xstudio.api.session.playlist.timeline import create_item_container
        children = []

        for i in self.item.children():
            children.append(create_item_container(self.connection, i))

        return children

    @property
    def trimmed_range(self):
        return self.item.trimmed_range()

    @property
    def available_range(self):
        return self.item.available_range()

    def range_at_index(self, index):
        return self.item.range_at_index(index)

    def item_at_frame(self, frame):
        return self.item.item_at_frame(frame)

    def frame_at_index(self, index):
        return self.item.frame_at_index(index)

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

    def index_of_child(self, child):
        """Get index of child.""

        Args child([Gap/Track/Clip/Stack]): Child.

        Returns:
            index(int): Index.
        """

        index = -1
        children = self.children
        for i in range(len(children)):
            if children[i].uuid == child.uuid:
                index = i
                break
        return index
