# SPDX-License-Identifier: Apache-2.0
from xstudio.api.app import App
from xstudio.core import join_broadcast_atom

class Studio(App):
    """Studio object."""
    def __init__(self, connection, remote, uuid=None):
        """Create studio object.

        Derived from App.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        super(Studio, self).__init__(connection, remote, uuid)

        # get studio app group
        # self.add_handler(self.handle_event)

    # def handle_event(self, sender, req_id, event):
    #     print(sender, req_id, event)

