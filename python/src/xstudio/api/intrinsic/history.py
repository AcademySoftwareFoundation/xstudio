# SPDX-License-Identifier: Apache-2.0
from xstudio.core import clear_atom, enable_atom, count_atom, undo_atom, redo_atom
from xstudio.api.session import Container

class History(Container):
    """Access JSON stores"""
    def __init__(self, connection, remote, uuid=None):
        """Create JsonStorePy object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    def clear(self):
        """Clear history

        Returns:
            success(bool): History cleared.
        """
        return self.connection.request_receive(self.remote, clear_atom())[0]

    @property
    def enabled(self):
        """Get enabled state.

        Returns:
            enabled(bool): State.
        """
        return self.connection.request_receive(self.remote, enable_atom())[0]

    @enabled.setter
    def enabled(self, x):
        """Set enabled state.

        Args:
            state(bool): Set enabled state.
        """
        self.connection.request_receive(self.remote, enable_atom(), x)

    @property
    def count(self):
        """Get history size

        Returns:
            count(int): Count.
        """
        return self.connection.request_receive(self.remote, count_atom())[0]

    def set_max_count(self, value):
        """Sets max size in events
        Args:
            value(int): Set max count.

        Returns:
            success(bool): Count set.
        """
        return self.connection.request_receive(self.remote, count_atom(), value)[0]

    def undo(self):
        """Undo

        Returns:
            success(obj): Undo obj.
        """
        return self.connection.request_receive(self.remote, undo_atom())[0]

    def redo(self):
        """Undo

        Returns:
            success(obj): Undo obj.
        """
        return self.connection.request_receive(self.remote, redo_atom())[0]

