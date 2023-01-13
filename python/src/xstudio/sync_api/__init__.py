# SPDX-License-Identifier: Apache-2.0
from xstudio.common_api import CommonAPI

class SyncAPI(CommonAPI):
    """We use this as a base for the SYNC API connection."""

    def __init__(self, connection):
        """Hold connection handle.

        Args:
           connection (object): Hold connection object.

        """
        super(SyncAPI, self).__init__(connection)

