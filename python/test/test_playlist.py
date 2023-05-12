# SPDX-License-Identifier: Apache-2.0
import xstudio
import os
from xstudio.api.session.media import Media

def test_rename_playlist(spawn):
    s = spawn.api.session
    (uuid, pl) = s.create_playlist("TEST")

    s.playlists[0].name == "TEST2"

    s.remove_container(uuid)

def test_playlist_create_remove_divider(spawn):
    s = spawn.api.session
    (uuid, pl) = s.create_playlist("TEST")

    con = pl.create_divider("TEST")
    assert con.name == "TEST"
    assert pl.playlist_tree.empty == False

    pl.rename_container("TEST2", con)

    con = pl.get_container(con)
    assert con.name == "TEST2"

    pl.remove_container(con)

    assert pl.playlist_tree.empty == True

    s.remove_container(uuid)

def test_playlist_create_remove_subset(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    # returns subset, not container..
    (sub_uuid,sub) = pl.create_subset("TEST")
    assert sub.name == "TEST"
    assert pl.playlist_tree.empty == False

    con = pl.get_container(sub_uuid)
    assert con.name == "TEST"

    pl.remove_container(sub_uuid)

    assert pl.playlist_tree.empty == True

    s.remove_container(pl_uuid)

def test_add_remove_media(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    assert isinstance(m, Media) == True

    ml = pl.add_media_list(os.environ["TEST_RESOURCE"]+"/media", True)

    # str(ml) == "[]"
    assert len(ml) == 0

    assert pl.remove_media(m) == True
    assert pl.remove_media(ml) == False

    assert s.remove_container(pl_uuid) == True

    assert s.playlist_tree.empty == True

def test_move_media(spawn):
    s = spawn.api.session
    (pl_uuid, pl) = s.create_playlist("TEST")

    m1 = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    m2 = pl.add_media(os.environ["TEST_RESOURCE"]+"/media/test.mov")
    assert isinstance(m1, Media) == True
    assert isinstance(m2, Media) == True

    # check current order.
    plm = pl.media
    assert plm[0].uuid == m1.uuid
    assert plm[1].uuid == m2.uuid

    assert pl.move_media(m1) == True
    plm = pl.media
    assert plm[1].uuid == m1.uuid

    assert pl.move_media(m1, m2) == True
    plm = pl.media
    assert plm[0].uuid == m1.uuid

    assert pl.remove_media(m1) == True
    assert pl.remove_media(m2) == True
    assert s.remove_container(pl_uuid) == True
    assert s.playlist_tree.empty == True
