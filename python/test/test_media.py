# SPDX-License-Identifier: Apache-2.0
import xstudio
import os
from xstudio.api.session.media import Media
from xstudio.api.session.media import MediaSource


def test_add_remove_media(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    assert isinstance(m, Media) == True

    assert m.acquire_metadata() == True

    assert m.source_metadata["metadata"]["media"]["@"]["format"]["bit_rate"] == 183486

    assert isinstance(m.media_source(), MediaSource) == True

    assert pl.remove_media(m) == True
    assert s.remove_container(pl_uuid) == True

    assert s.playlist_tree.empty == True

