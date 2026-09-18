# SPDX-License-Identifier: Apache-2.0
from xstudio.core import autosave_atom
from xstudio.api.session import Container
from xstudio.api.auxiliary.json_store import JsonStoreHandler

class GlobalStore(Container, JsonStoreHandler):
    """Access global state dictionary"""

    def __init__(self, connection, remote, uuid=None):
        """Create GlobalStore object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)
        JsonStoreHandler.__init__(self, self)

    @property
    def autosave(self):
        """Get autosave.

        Returns:
            autosave(bool): Autosave enabled.
        """
        return self.connection.request_receive(self.remote, autosave_atom())[0]

    @autosave.setter
    def autosave(self, enabled):
        """Set autosave.

        Args:
            enabled(bool): Set autosave mode.
        """
        self.connection.send(self.remote, autosave_atom(), enabled)

    def save(self, context):
        """Save context.

        Args:
            context(str): Context to save.

        Returns:
            success(bool): State saved.
        """
        return self.connection.request_receive(self.remote, save_atom(), context)[0]

    def value(self, path):
        """Get value from store

        Args:
            path(str): path to value.

        Returns:
            result(JSON): result.
        """
        return self.get(path + "/value")

    def default_value(self, path):
        """Get default value from store

        Args:
            path(str): path to default value.

        Returns:
            result(JSON): result.
        """
        return self.get(path + "/default_value")

    def datatype(self, path):
        """Get datatype from store

        Args:
            path(str): path to datatype.

        Returns:
            result(str): result.
        """
        return self.get(path + "/datatype")

    def description(self, path):
        """Get description from store

        Args:
            path(str): path to description.

        Returns:
            result(str): result.
        """
        return self.get(path + "/description")

    def context(self, path):
        """Get context from store

        Args:
            path(str): path to context.

        Returns:
            result(JSON): result.
        """
        return self.get(path + "/context")

    def maximum(self, path):
        """Get maximum from store

        Args:
            path(str): path to maximum.

        Returns:
            result(numeric): result.
        """
        return self.get(path + "/maximum")

    def minimum(self, path):
        """Get minimum from store

        Args:
            path(str): path to minimum.

        Returns:
            result(numeric): result.
        """
        return self.get(path + "/minimum")


    def set_value(self, path, value):
        """Set value in store

        Args:
            path(str): path to value.
            value(JSON): value.

        Returns:
            result(bool): result.
        """
        return self.set(value, path + "/value")

    def set_maximum(self, path, value):
        """Set maximum in store

        Args:
            path(str): path to value.
            value(numeric): value.

        Returns:
            result(bool): result.
        """
        return self.set(value, path + "/maximum")

    def set_minimum(self, path, value):
        """Set minimum in store

        Args:
            path(str): path to value.
            value(numeric): value.

        Returns:
            result(bool): result.
        """
        return self.set(value, path + "/minimum")

    def get_preferences(self, context=None):
        """Get preferences from store

        Kwargs:
            context(str): context.

        Returns:
            result(JSON): result.
        """
        if context is None:
            context = set()

        return self.get().get_preferences(context)

    def get_values(self, context=None):
        """Get values from store

        Kwargs:
            context(str): context.

        Returns:
            result(JSON): result.
        """
        if context is None:
            context = set()

        return self.get().get_values(context)