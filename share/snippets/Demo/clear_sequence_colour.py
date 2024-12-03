from xstudio.api.session.playlist.timeline import Timeline, Stack, Track, Clip

def clear_sequence_colour(item):
    if isinstance(item, Timeline) or isinstance(item, Stack) or isinstance(item, Track):
        for child in item.children:
            clear_sequence_colour(child)
        if isinstance(item, Track):
            item.item_flag = ""
    elif isinstance(item, Clip):
        item.item_flag = ""

clear_sequence_colour(XSTUDIO.api.session.viewed_container)