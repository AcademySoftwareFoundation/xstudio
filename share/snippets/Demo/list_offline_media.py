for playlist in XSTUDIO.api.session.playlists:
    for m in playlist.media:
        if not m.is_online:
            print("'" + "','".join([playlist.name, m.name, str(m.media_source().media_reference.uri())]) + "'")