# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_json_atom, set_json_atom, JsonStore
from xstudio.api.session import Container

class JsonStorePy(Container):
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

    def get(self, path=None):
        """Get JSON.

        Kwargs:
            path(str): Path to JSON subsection.

        Returns:
            json(JsonStore): JsonStore.
        """

        if path is None:
            return self.connection.request_receive(self.remote, get_json_atom())[0]

        return self.connection.request_receive(self.remote, get_json_atom(), path)[0]

    def set(self, obj, path=None):
        """Set JSON.

        Args:
            obj(JSON): JSON to set.

        Kwargs:
            path(str): Path to JSON subsection.

        Returns:
            success(bool): Did it work.
        """
        if path is None:
            return self.connection.request_receive(self.remote, set_json_atom(), JsonStore(obj))[0]

        return self.connection.request_receive(self.remote, set_json_atom(), JsonStore(obj), path)[0]
