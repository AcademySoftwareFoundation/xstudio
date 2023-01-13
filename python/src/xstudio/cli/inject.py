# SPDX-License-Identifier: Apache-2.0
"""Main for the xStudio control command."""

import sys
import os
import argparse
import xstudio
from xstudio.connection import Connection
from xstudio.core import *

__version__ = xstudio.__version__

def inject_main():
    """Do main function."""
    retval = 0
    parser = argparse.ArgumentParser(
        description=__doc__ + " Injects media into xStudio.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        "-s", "--session", action="store",
        type=str,
        help="Host name.",
        default=None
    )

    parser.add_argument(
        "-H", "--host", action="store",
        type=str,
        help="Host name.",
        default=None
    )

    parser.add_argument(
        "-p", "--port", action="store",
        type=int,
        help="Port.",
        default=45500
    )

    parser.add_argument(
        "media", type=str, nargs="*", default=[],
        help="Media paths to add."
    )

    args = parser.parse_args()

    if args.session:
        m = RemoteSessionManager(remote_session_path())
        s = m.find(args.session)
        conn = Connection()
        conn.connect_remote(s.host(), s.port.port(), False)

    elif args.host:
        conn = Connection()
        conn.connect_remote(args.host, args.port, False)
    else:
        conn = Connection()
        conn.connect_remote("localhost", args.port, False)

    pl = conn.api.app.session.create_playlist("Injected Media")[1]

    if len(args.media):
        for path in args.media:
            try:
                path = os.path.abspath(path)
                print("Added "+str(len(pl.add_media_list(path, True)))+" Media items.")
            except ValueError as err:
                print(str(err))
    else:
        # try stdin
        while True:
            try:
                line = sys.stdin.readline()
            except KeyboardInterrupt:
                break

            if not line:
                break

            path = line.strip()
            if path:
                try:
                    path = os.path.abspath(path)
                    print("Added "+str(len(pl.add_media_list(path, True)))+" Media items.")
                except ValueError as err:
                    print(str(err))

    conn.disconnect()

    sys.exit(retval)
