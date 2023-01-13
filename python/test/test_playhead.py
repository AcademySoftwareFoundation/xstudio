# SPDX-License-Identifier: Apache-2.0
import xstudio
import os

from xstudio.api.session.playhead import Playhead
from xstudio.api.session.media import Media
from xstudio.api.session.media import MediaSource


def test_playhead(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")

    ph = pl.create_playhead()
    assert isinstance(ph, Playhead) == True

    assert ph.playing == False
    assert ph.position == 0

    assert pl.remove_media(m) == True
    assert s.remove_container(pl_uuid) == True

    assert s.playlist_tree.empty == True

