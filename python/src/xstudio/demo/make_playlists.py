#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

import argparse
from xstudio.connection import Connection

def make_playlists(session, names):
    """Create multiple playlists.

        Args:
           session (object): Session object.
           names (str list): names.
    """
    for i in names:
        session.create_playlist(i)


if __name__ == "__main__":
    """Create empty playlists from names."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument('names', metavar='NAME', type=str, nargs='+',
                    help='names for creating playlists')

    args = parser.parse_args()

    XSTUDIO = Connection(auto_connect=True)

    make_playlists(XSTUDIO.api.session, args.names)
