# SPDX-License-Identifier: Apache-2.0
from xstudio.api.session.playlist.subset import Subset
from xstudio.core import URI, selection_actor_atom
from xstudio.api.session.playhead import Playhead, PlayheadSelection

class ContactSheet(Subset):
    """ContactSheet object."""

    def __init__(self, connection, remote, uuid=None):
        """Create contactSheet object.

        Derived from Subset.

        Args:
            connection(Connection): Connection object
            remote(object): Remote actor object

        Kwargs:
            uuid(Uuid): Uuid of remote actor.
        """
        Subset.__init__(self, connection, remote, uuid)

    @property
    def playhead_selection(self):
        """The actor that filters a selection of media from a playhead
        and passes to a playhead for playback.

        Returns:
            source(PlayheadSelection): Currently playing this.
        """
        result =  self.connection.request_receive(self.remote, selection_actor_atom())[0]
        return PlayheadSelection(self.connection, result)
