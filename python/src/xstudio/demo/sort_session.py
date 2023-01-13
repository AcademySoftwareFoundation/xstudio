#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection

def sort_session(session):
    """Sort playlist items.

        Args:
           session (object): Session object.
    """

    children = session.playlist_tree.children

    names = sorted(children, key=lambda x: x.name)

    for i in names:
        session.move_container(i)


if __name__ == "__main__":
    XSTUDIO = Connection(auto_connect=True)

    sort_session(XSTUDIO.api.session)
