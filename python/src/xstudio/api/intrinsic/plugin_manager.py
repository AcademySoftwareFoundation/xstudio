# SPDX-License-Identifier: Apache-2.0
from xstudio.core import detail_atom, path_atom, add_path_atom, update_atom, enable_atom, get_resident_atom
from xstudio.api.auxiliary import ActorConnection

class PluginManager(ActorConnection):
    """Access global plugin manager"""

    def __init__(self, connection, remote):
        """Create plugin manager object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
        """
        ActorConnection.__init__(self, connection, remote)

    @property
    def plugin_detail(self):
        """ Get current plugins
        """
        return self.connection.request_receive(self.remote, detail_atom())[0]

    @property
    def paths(self):
        """ Get current plugins
        """

        return self.connection.request_receive(self.remote, path_atom())[0]

    def get_detail(self, uuid):
        """Get plugin detail.

        Args:
            uuid(Uuid): Plugin id
        Returns: PluginDetail
        """
        return self.connection.request_receive(self.remote, detail_atom(), uuid)[0]

    def get_type_detail(self, ptype):
        """Get plugin detail.

        Args:
            type(PluginType): Plugin type

        Returns: list(PluginDetail)
        """
        return self.connection.request_receive(self.remote, detail_atom(), ptype)[0]

    def add_path(self, path):
        """Add plugin path.

        Args:
            path(str): Path to add.
        """
        self.connection.send(self.remote, add_path_atom(), path)

    def update(self):
        """Update plugins.

        Returns:
            count(int): Count of plugins.
        """
        return self.connection.request_receive(self.remote, update_atom())[0]

    def enable(self, uuid, enabled):
        """Enable plugin.

        Args:
            uuid(Uuid): Plugin id
            enabled(bool): New state

        Returns:
            enabled(bool): Success.
        """
        return self.connection.request_receive(self.remote, enable_atom(), uuid, enabled)[0]

    @property
    def resident(self):
        """Get resident plugins.

        Returns:
            plugins(list[[uuid,actor]]): Resident plugins.
        """
        return self.connection.request_receive(self.remote, get_resident_atom())[0]