# SPDX-License-Identifier: Apache-2.0
from xstudio.core import UuidActor, Uuid, actor, FrameRange, FrameRateDuration, FrameRate, URI

import opentimelineio
from opentimelineio.schema import Track, Stack, Clip, Gap

def __process_obj(playlist, timeline, parent, obj, media_lookup):
    for i in obj:
        if isinstance(i, Track):
            if i.kind == Track.Kind.Video:
                # print("Video Track")
                trk = timeline.create_video_track(i.name)
                if i.source_range is not None:
                    trk.active_range = FrameRange(FrameRateDuration(int(i.source_range.duration.value), FrameRate(i.source_range.duration.rate)))
                parent.insert_child(trk)
                __process_obj(playlist, timeline, trk, i.each_child(), media_lookup)

            elif i.kind == Track.Kind.Audio:
                # print("Audio Track")
                trk = timeline.create_audio_track(i.name)
                if i.source_range is not None:
                    trk.active_range = FrameRange(FrameRateDuration(int(i.source_range.duration.value), FrameRate(i.source_range.duration.rate)))
                parent.insert_child(trk)
                __process_obj(playlist, timeline, trk, i.each_child(), media_lookup)

        elif isinstance(i, Gap):
            # print("Gap")
            gap = timeline.create_gap(i.name)
            if i.source_range is not None:
                gap.active_range = FrameRange(FrameRateDuration(int(i.source_range.duration.value), FrameRate(i.source_range.duration.rate)))

            parent.insert_child(gap)

        elif isinstance(i, Clip):
            # print("Clip")
            # does clip have a media Reference..
            if not i.media_reference.is_missing_reference:
                # try and create media item in playlist to link clip to.
                if i.media_reference.target_url in media_lookup:
                    media = media_lookup[i.media_reference.target_url]
                else:
                    media = playlist.add_media(URI(i.media_reference.target_url))
                    media_lookup[i.media_reference.target_url] = media

                clp = timeline.create_clip(media.uuid_actor(), i.name)
                clp.available_range = FrameRange(
                    FrameRateDuration(int(i.available_range().start_time.value), FrameRate(i.available_range().start_time.rate)),
                    FrameRateDuration(int(i.available_range().duration.value), FrameRate(i.available_range().duration.rate))
                )

            else:
                clp = timeline.create_clip(UuidActor(Uuid(),actor()), i.name)

            if i.source_range is not None:
                clp.active_range = FrameRange(
                    FrameRateDuration(int(i.source_range.start_time.value), FrameRate(i.source_range.start_time.rate)),
                    FrameRateDuration(int(i.source_range.duration.value), FrameRate(i.source_range.duration.rate))
                )

            parent.insert_child(clp)

        elif isinstance(i, Stack):
            # print("Stack")
            stk = timeline.create_stack(i.name)
            if i.source_range is not None:
                stk.active_range = FrameRange(FrameRateDuration(int(i.source_range.duration.value), FrameRate(i.source_range.duration.rate)))
            parent.insert_child(stk)
            __process_obj(playlist, timeline, trk, i.each_child(), media_lookup)

def import_timeline(playlist, otio, name=None, before=Uuid()):
    """Create Timeline from otio.

    Args:
        playlist(Playlist): Playlist object
        otio(OTIO Timeline): Otio timeline

    Kwargs:
        name(str): name of timeline.

    Returns:
        timeline(Uuid,Timeline): Returns container Uuid and Timeline.
    """
    if name is None:
        name = otio.name

    print("Loading timeline")

    result = playlist.create_timeline(name, before)
    timeline = result[1]
    timeline.history.enabled = False

    media_lookup = {}

    # preprocess clips
    for i in otio.clip_if():
        if i.media_reference.target_url not in media_lookup:
            media = playlist.add_media(URI(i.media_reference.target_url))
            timeline.add_media(media)
            media_lookup[i.media_reference.target_url] = media

    __process_obj(playlist, timeline, timeline.stack, reversed(otio.video_tracks()), media_lookup)
    __process_obj(playlist, timeline, timeline.stack, otio.audio_tracks(), media_lookup)

    timeline.history.enabled = True
    print("Timeline loaded")

    return result

def import_timeline_from_file(playlist, path, name=None, before=Uuid(), adapter_name=None):
    """Create Timeline from otio file.

    Args:
        playlist(Playlist): Playlist object
        path(str): Path to otio timeline

    Kwargs:
        name(str): name of timeline.
        before(Uuid): playlist position to add timeline.
        adapter_name(str): Override otio adapter

    Returns:
        timeline(Uuid,Timeline): Returns container Uuid and Timeline.
    """
    otio = opentimelineio.adapters.read_from_file(
        path, adapter_name=adapter_name
    )

    return import_timeline(playlist, otio, name, before)

