#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection

def dump_shots(session):
    """Dump shot flag pairs.

        Args:
           session (object): Session object.
    """

    media = session.get_media()
    shot_flags = {}
    for i in media:
        if i.flag_colour != "#00000000":
            # has flag..
            # get metadata and see if shot is set.
            meta = i.source_metadata
            try:
                shot = meta["metadata"]["external"]["DNeg"]["shot"]
                if shot not in shot_flags:
                    shot_flags[shot] = []
                shot_flags[shot].append(i.flag_text)
            except:
                pass

    for i in sorted(shot_flags.keys()):
        print ("'"+i+"','"+ ",".join(shot_flags[i])+"'" )

def dump_shots_gruff(session):
    """Dump shot flag pairs.

        Args:
           session (object): Session object.
    """

    media = session.get_media()
    flag_shots = {}

    for i in media:
        if i.flag_colour != "#00000000":
            # has flag..
            # get metadata and see if shot is set.
            meta = i.source_metadata
            try:
                shot = meta["metadata"]["external"]["DNeg"]["shot"]
                if i.flag_text not in flag_shots:
                    flag_shots[i.flag_text] = set()

                flag_shots[i.flag_text].add(shot)
            except:
                pass

    for i in sorted(flag_shots.keys()):
        print(i+":")
        for ii in sorted(flag_shots[i]):
            print (ii)


def render_all_annotations(session):
    """Render all annotations in the session as a sequence of (unordered) images
    according to the filepath /$TMPDIR/xs_annotation.%04d.exr.

        Args:
           session (object): Session object.
    """

    import os
    from xstudio.core import render_annotations_atom

    tmpdir = os.environ["TMPDIR"]
    path = ("{0}/xs_annotation.%04d.exr").format(tmpdir)
    result = session.connection.request_receive(
        session.remote,
        render_annotations_atom(),
        path)[0]


if __name__ == "__main__":
    XSTUDIO = Connection(auto_connect=True)
    dump_shots(XSTUDIO.api.session)