from xstudio.api.session.playlist.timeline import Timeline, Stack, Track, Clip

def flag_clip_step(step, colour, item=XSTUDIO.api.session.viewed_container):
    if isinstance(item, Timeline) or isinstance(item, Stack) or isinstance(item, Track):
        for child in item.children:
            flag_clip_step(step, colour, child)
    elif isinstance(item, Clip):
        try:
            mstep = item.media.get_metadata("/metadata/shotgun/version/attributes/sg_pipeline_step")
            if mstep == step:
                item.item_flag = colour
        except:
            pass

flag_clip_step("Layout", "#ff0000ff")
flag_clip_step("Prep", "#ff00ff00")