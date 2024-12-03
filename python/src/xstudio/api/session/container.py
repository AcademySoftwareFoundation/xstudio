# SPDX-License-Identifier: Apache-2.0
from xstudio.api.auxiliary.helpers import get_name, set_name, get_version
from xstudio.api.auxiliary.helpers import get_uuid, get_type, get_event_group
from xstudio.api.auxiliary import ActorConnection
from xstudio.core import UuidActor

try:
    import XStudioExtensions
except:
    XStudioExtensions = None


class PlaylistItem(object):
    def __init__(self, connection, remote, item):
        """Create PlaylistItem object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
            item(core.PlaylistItem): Item object
        """
        self.connection = connection
        self.remote = remote
        self.item = item
        self._children = None

    @property
    def name(self):
        """Get item name.

        Returns:
            name(str): Name of item.
        """
        return self.item.name()

    @property
    def uuid(self):
        """Get item uuid

        Returns:
            uuid(Uuid): Uuid of item.
        """
        return self.item.uuid()

class PlaylistTree(object):
    def __init__(self, connection, remote, tree):
        """Create Playhead object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
            tree(core.PlaylistTree): Tree object
        """
        self.connection = connection
        self.remote = remote
        self.tree = tree
        self._children = None

    @property
    def type(self):
        """Get item type

        Returns:
            type(str): Type of item.
        """
        return self.tree.type()

    @property
    def name(self):
        """Get item name

        Returns:
            name(str): Name of item.
        """
        return self.tree.name()

    @property
    def uuid(self):
        """Get item uuid

        Returns:
            uuid(Uuid): Uuid of item.
        """
        return self.tree.uuid()

    @property
    def value_uuid(self):
        """Get item value uuid

        Returns:
            uuid(Uuid): Uuid of item value.
        """
        return self.tree.value_uuid()

    @property
    def empty(self):
        """Is empty

        Returns:
            empty(bool): No children.
        """
        return self.tree.empty()

    @property
    def size(self):
        """Number of children

        Returns:
            size(int): Count of children.
        """
        return self.tree.size()

    @property
    def children(self):
        """(Children)

        Returns:
            children(list[PlaylistTree]): Children.
        """
        if self._children is None:
            self._children = [PlaylistTree(self.connection, self.remote, i) for i in self.tree.children()]

        return self._children


class Container(ActorConnection):
    def __init__(self, connection, remote, uuid=None):
        """Create Container object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        ActorConnection.__init__(self, connection, remote)
        self._uuid = uuid
        self._type = None
        self._version = None

    def uuid_actor(self):
        """Return as a UuidActor."""
        return UuidActor(self.uuid, self.remote)

    def add_event_callback_function(self, callback_function):
        """Set a callback that will be called whenever the Container sends
        a message to its event group
        Args:
            callback_function(function): Callback function

        Returns:
            name(str): Name of actor container.
        """
        if XStudioExtensions:
            XStudioExtensions.add_message_callback(
                (self.group, callback_function, self.remote)
                )
        else:
            self.connection.link.run_callback_with_delay(
                (self.group, callback_function, self.remote)
                )


    def remove_event_callback_function(self, callback_function):
        """Remove a callback that was set-up to get event messages from the
        container
        Args:
            callback_function(function): Callback function

        Returns:
            name(str): Name of actor container.
        """
        if XStudioExtensions:
            XStudioExtensions.remove_message_callback(
                (self.group, callback_function)
                )
        else:
            self.connection.remove_message_callback(
                (self.group, callback_function)
                )

    @property
    def name(self):
        """Get container name

        Returns:
            name(str): Name of actor container.
        """
        return get_name(self.connection, self.remote)

    @property
    def group(self):
        """Get container group

        Returns:
            group(actor): Event group of actor.
        """
        return get_event_group(self.connection, self.remote)

    @name.setter
    def name(self, x):
        """Set container name.

        Args:
            name(str): New name.
        """
        set_name(self.connection, self.remote, x)

    @property
    def version(self):
        """Get container version

        Returns:
            version(str): Version of actor container.
        """
        if not self._version:
            self._version = get_version(self.connection, self.remote)
        return self._version

    @property
    def type(self):
        """Get container type

        Returns:
            type(str): Type of actor container.
        """
        if not self._type:
            self._type = get_type(self.connection, self.remote)
        return self._type

    @property
    def uuid(self):
        """Get container uuid

        Returns:
            uuid(Uuid): Uuid of actor container.
        """
        if not self._uuid:
            self._uuid = get_uuid(self.connection, self.remote)
        return self._uuid
