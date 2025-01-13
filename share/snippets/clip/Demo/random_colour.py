from xstudio.api.session.playlist.timeline import Timeline, Clip
import random
import colorsys

def random_clip_colour(item=XSTUDIO.api.session.viewed_container):
    if isinstance(item, Timeline):
        for i in item.selection:
            if isinstance(i, Clip):
                c = random.randrange(0, 255)

                rgb = colorsys.hsv_to_rgb(c * (1.0/255.0), 1, 1)
                i.item_flag = "#FF" + f"{int(rgb[0]*255.0):0{2}x}"+ f"{int(rgb[1]*255.0):0{2}x}"+ f"{int(rgb[2]*255.0):0{2}x}"

random_clip_colour()

