# SPDX-License-Identifier: Apache-2.0
import xstudio
import os
from xstudio.api.session.media import Media
from xstudio.api.session.media import MediaSource
from xstudio.api.session.media import MediaStream
from xstudio.core import MediaType


def test_media_stream(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    assert isinstance(m, Media) == True

    msrc = m.media_source()
    assert isinstance(msrc, MediaSource) == True

    mstr = msrc.image_stream
    assert isinstance(mstr, MediaStream) == True

    assert mstr.media_type == MediaType.MT_IMAGE

    msd = mstr.media_stream_detail

    assert msd.name() == "stream 0"
    assert msd.key_format() == "{0}@{1}/{2}"
    assert msd.duration().seconds() == 4.166666666666667
    assert msd.media_type() == MediaType.MT_IMAGE

    assert pl.remove_media(m) == True
    assert s.remove_container(pl_uuid) == True

    assert s.playlist_tree.empty == True

