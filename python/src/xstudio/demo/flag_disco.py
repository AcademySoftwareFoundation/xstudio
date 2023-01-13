#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

import random
import time
from xstudio.connection import Connection

def animate_flags(session, cycles=100):
    """Animate session item flags.

        Args:
           session (object): Session object.

        Kwargs:
           cycles (int): Run for this many cycles.
    """

    children = session.playlist_tree.children

    for i in range(0, cycles):
        session.reflag_container(
            "#{:02X}{:02X}{:02X}".format(
                random.randint(0, 255),
                random.randint(0, 255),
                random.randint(0, 255)
            ),
            children[random.randint(0, len(children) - 1)]
        )

        time.sleep(0.05)

if __name__ == "__main__":
    XSTUDIO = Connection(auto_connect=True)
    animate_flags(XSTUDIO.api.session)
