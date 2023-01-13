# SPDX-License-Identifier: Apache-2.0
class CommonAPI(object):
    """We use this as a base for the current API connection types.

    .. note::

       This is a private base class, and is not used without subclassing.

    """
    def __init__(self, connection):
        """Hold connection handle.

        Args:
           connection (object): Hold connection object.

        """

        self.connection = connection
