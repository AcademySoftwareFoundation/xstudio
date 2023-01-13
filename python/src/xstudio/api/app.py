# SPDX-License-Identifier: Apache-2.0
from xstudio.core import session_atom, join_broadcast_atom
from xstudio.api.session import Session, Container

class App(Container):
    """App access. """
    def __init__(self, connection, remote, uuid=None):
        """Create app object.
        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Container.__init__(self, connection, remote, uuid)

    @property
    def session(self):
        """Session object.

        Returns:
            session(Session): Session object."""
        return Session(self.connection, self.connection.request_receive(self.remote, session_atom())[0])
