from xstudio.api.session.playlist import Playlist, Subset

def remove_duplicate_sg_version(item=XSTUDIO.api.session.inspected_container):
    if isinstance(item, Playlist) or isinstance(item, Subset):
        dup = set()
        for media in item.media:
            try:
                sid = media.metadata["metadata"]["shotgun"]["version"]["id"]
                if sid not in dup:
                    dup.add(sid)
                else:
                    print("Removing Duplicate", media.name)
                    item.remove_media(media)
            except Exception as err:
                pass

remove_duplicate_sg_version()