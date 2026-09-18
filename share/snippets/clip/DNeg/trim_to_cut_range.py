from xstudio.api.session.playlist.timeline import Timeline, Clip
from xstudio.core import FrameRange, FrameRateDuration, FrameRate

def use_clip_range(item=XSTUDIO.api.session.viewed_container):
    if isinstance(item, Timeline):
        for i in item.selection:
            if isinstance(i, Clip):
                # get media metadata..
                crange = None
                try:
                    meta = i.media.metadata["metadata"]["shotgun"]
                    try:
                        crange = meta["version"]["attributes"]["sg_cut_range"]
                    except:
                        pass

                    if crange is None:
                        try:
                            crange = meta["version"]["attributes"]["sg_cut_in"] + "-" + meta["version"]["attributes"]["sg_cut_out"]
                        except:
                            pass

                    if crange is None:
                        try:
                            crange = meta["shot"]["attributes"]["sg_cut_range"]
                        except:
                            pass

                    if crange is not None:
                        start, end = crange.split("-")
                        start = int(start)
                        end = int(end)
                        cfr = i.trimmed_range.rate()

                        # modify clip range..
                        i.active_range = FrameRange(
                            FrameRateDuration(
                                start,
                                cfr
                            ),
                            FrameRateDuration(
                                end-start,
                                cfr
                            )
                        )

                except Exception as e:
                    print(e)

use_clip_range()
