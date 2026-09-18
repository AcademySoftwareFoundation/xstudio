# SPDX-License-Identifier: Apache-2.0
import math
from xstudio.core import name_atom, uuid_atom, type_atom, version_atom, get_event_group_atom

class ActorConnection(object):
    def __init__(self, connection, remote):
        """Create ActorConnection object.

        Args:
            connection(Connection): Connection object
            remote(actor): Remote actor object
        """
        self.connection = connection
        self.remote = remote

    def add_handler(self, handler):
        """Add event handler
        Args:
            handler(fun(sender, req_id, event)): Handler function.

        Returns:
            handler_id(str): Key for removing handler.
        """
        return self.connection.add_handler(self.remote, handler)

    def remove_handler(self, handler_id):
        """Add event handler
        Args:
            handler_id(str): Handler id.

        Returns:
            success(bool): Success.
        """
        return self.connection.remove_handler(handler_id)

    def add_event_callback(self, callback):
        """Add event message callback
        Args:
            callback(fun(sender, req_id, event)): Callback function.

        Returns:
            handler_id(str): Key for removing handler.
        """
        return self.connection.add_event_callback(self.remote, callback)

    def remove_event_callback(self, handler_id):
        """Add event handler
        Args:
            handler_id(str): Handler id.

        Returns:
            success(bool): Success.
        """
        return self.connection.remove_event_callback(handler_id)

def get_name(connection, remote):
    """Get actor name

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.

    Returns:
        name(str): Name of actor container.
    """
    return connection.request_receive(remote, name_atom())[0]

def set_name(connection, remote, name):
    """Set actor name.

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.
        name(str): New name.
    """
    connection.send(remote, name_atom(), name)

def get_event_group(connection, remote):
    """Get event actor.

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.

    Returns:
        group(actor): Event group of actor.
    """
    return connection.request_receive(remote, get_event_group_atom())[0]

def get_uuid(connection, remote):
    """Get actor uuid.

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.

    Returns:
        uuid(Uuid): Uuid of actor container.
    """
    return connection.request_receive(remote, uuid_atom())[0]

def get_type(connection, remote):
    """Get actor type name.

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.

    Returns:
        type(str): Type name of actor container.
    """
    return connection.request_receive(remote, type_atom())[0]

def get_version(connection, remote):
    """Get actor version.

    Args:
        connection(Connection): Connection object.
        remote(actor): Remote actor object.

    Returns:
        version(str): Version of actor container.
    """
    return connection.request_receive(remote, version_atom())[0]

class Filesize(object):
    """
    Container for a size in bytes with a human readable representation
    Use it like this::

        >>> size = Filesize(123123123)
        >>> print size
        "117.4 MB"
    """

    chunk = 1024
    units = ["", "K", "M", "G", "T", "P"]
    precisions = [0, 0, 1, 1, 1, 1]

    def __init__(self, size):
        self.size = size

    def __int__(self):
        return self.size

    def __str__(self):
        if self.size == 0:
            return "0"
        from math import log
        unit = self.units[
            min(int(log(self.size, self.chunk)), len(self.units) - 1)
        ]
        return self.format(unit)

    def format(self, unit):
        """
            Bumpf
        """
        if unit not in self.units:
            raise Exception("Not a valid file size unit: %s" % unit)
        if self.size == 1 and unit == "B":
            return "1B"
        exponent = self.units.index(unit)
        quotient = float(self.size) / self.chunk**exponent
        precision = self.precisions[exponent]
        format_string = "{0:.%sf}{1}" % (precision)
        return format_string.format(quotient, unit)

def millify(number):
    """
        Bumpf
    """
    millnames = ["", " Thousand", " Million", " Billion", " Trillion"]
    millidx = 0
    if number:
        millidx = max(
            0,
            min(
                len(millnames) - 1,
                int(math.floor(math.log10(abs(number)) / 3.0))
            )
        )
    return "%.0f%s" % (number/10**(3*millidx), millnames[millidx])