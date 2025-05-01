# SPDX-License-Identifier: Apache-2.0
from xstudio.core import get_json_atom, set_json_atom, JsonStore
from xstudio.api.session import Container
import json

class JsonStoreHandler():
    def __init__(self, actorconnection):
        """Create JsonStoreHandler object.

        Args:
            actorconnection(ActorConnection): Connection object

        """

        self._actor_connection = actorconnection

    def get(self, path=None):
        """Get JSON.

        Kwargs:
            path(str): Path to JSON subsection.

        Returns:
            json(JsonStore): JsonStore.
        """

        if path is None:
            return self._actor_connection.connection.request_receive(self._actor_connection.remote, get_json_atom())[0]

        return self._actor_connection.connection.request_receive(self._actor_connection.remote, get_json_atom(), path)[0]

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
            return self._actor_connection.connection.request_receive(self._actor_connection.remote, set_json_atom(), JsonStore(obj))[0]

        return self._actor_connection.connection.request_receive(self._actor_connection.remote, set_json_atom(), JsonStore(obj), path)[0]


    @property
    def metadata(self):
        """Get media metadata.

        Returns:
            metadata(json): Media metadata.
        """
        return json.loads(self._actor_connection.connection.request_receive(self._actor_connection.remote, get_json_atom())[0].dump())


    @metadata.setter
    def metadata(self, new_metadata):
        """Set media reference rate.

        Args:
            new_metadata(json): Json dict to set as media source metadata

        Returns:
            bool: success

        """
        return self._actor_connection.connection.request_receive(self._actor_connection.remote, set_json_atom(), JsonStore(new_metadata))

    def get_metadata(self, path):
        """Get metdata at JSON path

        Args:
            path(str): JSON Pointer

        Returns:
            metadata(json) Json at pointer location
        """

        return json.loads(self._actor_connection.connection.request_receive(self._actor_connection.remote, get_json_atom(), path)[0].dump())

    def set_metadata(self, data, path):
        """Get metdata at JSON path

        Args:
            data(json): JSON Data
            path(str): JSON Pointer

        Returns:
            bool: success
        """

        return self._actor_connection.connection.request_receive(self._actor_connection.remote, set_json_atom(), JsonStore(data), path)[0]
