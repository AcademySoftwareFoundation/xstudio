def name_from_media_shot(items=XSTUDIO.api.session.selected_containers):
    for item in items:
        try:
            media = item.media[0]
            project = media.metadata["metadata"]["shotgun"]["version"]["relationships"]["project"]["data"]["name"]
            shot = media.metadata["metadata"]["shotgun"]["version"]["relationships"]["entity"]["data"]["name"]
            item.name = project+" - "+shot
        except:
            pass

name_from_media_shot()
