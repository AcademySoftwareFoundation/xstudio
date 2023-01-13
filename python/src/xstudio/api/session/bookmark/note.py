# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.container import Container

class Note(Container):
    """Note object."""

    def __init__(self, connection, remote, uuid=None):
        """Create Note object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

