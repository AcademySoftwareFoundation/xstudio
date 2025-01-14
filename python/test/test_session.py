# SPDX-License-Identifier: Apache-2.0
import xstudio

def test_session_playlists(spawn):
    pls = spawn.api.session.playlists
    assert len(pls) == 0

def test_session_create_remove_playlist(spawn):
    s = spawn.api.session
    (uuid, pl) = s.create_playlist("TEST")
    pls = s.playlists
    assert len(pls) == 1
    s.remove_container(uuid)
    pls = s.playlists
    assert len(pls) == 0

def test_session_create_remove_divider(spawn):
    s = spawn.api.session
    div = s.create_divider("TEST")
    assert div.name == "TEST"
    assert s.playlist_tree.empty == False

    s.rename_container("TEST2", div)

    div = s.get_container(div)
    assert div.name == "TEST2"

    s.remove_container(div)

    assert s.playlist_tree.empty == True

def test_session_move_playlist(spawn):
    s = spawn.api.session

    (uuid1, pl1) = s.create_playlist("TEST1")
    (uuid2, pl2) = s.create_playlist("TEST2", uuid1)

    # order should be TEST2,TEST1

    t = s.playlist_tree

    # root is empty..
    assert t.name == "New Session"
    assert t.type == "Session"
    assert t.empty == False

    # assert str(t) == ""

    assert t.size == 2

    # first entry should be TEST2, but we don't mirror the playlist name here..
    # we probably should though.
    children = t.children
    assert len(children) == 2
    assert children[0].name == "TEST2"
    assert str(children[0].uuid) == str(uuid2)
    assert children[0].uuid == uuid2
    assert children[0].value_uuid == pl2.uuid
    assert children[1].name == "TEST1"

    # lets try reordering them.
    s.move_container(children[1],children[0])

    # tree is now invalid..
    t = s.playlist_tree
    children = t.children
    assert children[0].name == "TEST1"

    # cleanup
    s.remove_container(uuid1)
    s.remove_container(uuid2)

    t = s.playlist_tree
    assert len(s.playlists) == 0
    assert t.empty == True



