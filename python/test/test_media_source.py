# SPDX-License-Identifier: Apache-2.0
import xstudio
import os
from xstudio.api.session.media import Media
from xstudio.api.session.media import MediaSource


def test_media_source(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    assert isinstance(m, Media) == True

    ms = m.media_source()
    assert isinstance(ms, MediaSource) == True

    mr = ms.media_reference

    assert mr.container() == True
    assert mr.frame_count() == 309
    assert mr.seconds() == 10.3
    assert mr.duration().seconds() == 10.3
    assert mr.rate().fps() == 30.0
    assert str(mr.uri_from_frame(1)) == "file://localhost/"+os.environ["TEST_RESOURCE"]+"/media/test.mov"
    assert str(mr.timecode()) == "00:00:00:00"

    assert pl.remove_media(m) == True
    assert s.remove_container(pl_uuid) == True

    assert s.playlist_tree.empty == True

