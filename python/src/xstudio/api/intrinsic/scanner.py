# SPDX-License-Identifier: Apache-2.0
# from xstudio.core import get_thumbnail_atom, clear_atom, count_atom, size_atom
from xstudio.api.auxiliary import ActorConnection

class Scanner(ActorConnection):
    """Access global state dictionary"""

    def __init__(self, connection, remote):
        """Create Thumbnail object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
        """
        ActorConnection.__init__(self, connection, remote)

