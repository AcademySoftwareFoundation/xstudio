# SPDX-License-Identifier: Apache-2.0
from xstudio.core import Uuid, actor, item_atom, enable_atom, active_range_atom, available_range_atom, item_name_atom
from xstudio.api.session.container import Container

class Gap(Container):
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
    def children(self):
        return []

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
