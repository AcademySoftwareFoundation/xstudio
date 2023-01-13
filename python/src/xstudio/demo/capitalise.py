#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection

def capitalise(session):
    """Capitalise playlist items.

        Args:
           session (object): Session object.
    """

    children = session.playlist_tree.children

    for i in children:
        session.rename_container(i.name.capitalize(), i)

if __name__ == "__main__":
    XSTUDIO = Connection(auto_connect=True)
    capitalise(XSTUDIO.api.session)