# SPDX-License-Identifier: Apache-2.0
import opentimelineio
from opentimelineio.schema import Timeline as oTimeline, Track as oTrack, Stack as oStack, Clip as oClip, Gap as oGap
from opentimelineio.schema import ExternalReference
from opentimelineio.opentime import TimeRange, RationalTime

from xstudio.api.session.playlist.timeline import Timeline
from xstudio.api.session.playlist.timeline import Gap
from xstudio.api.session.playlist.timeline import Clip
from xstudio.api.session.playlist.timeline import Stack
from xstudio.api.session.playlist.timeline import Track


def write_otio(otio, path, adapter_name=None):
    """Save otio file.

    Args:
        otio(OTIOTimeline): OTIO Timeline object
        path(str): Path to write to

    Kwargs:
        adapter_name(str): Override adaptor.
    """
    opentimelineio.adapters.write_to_file(
        otio, path, adapter_name=adapter_name
    )

def export_timeline_to_file(timeline, path, adapter_name=None):
    """Create otio from timeline.

    Args:
        timeline(Timeline): Timeline object
        path(str): Path to write to

    Kwargs:
        adapter_name(str): Override adaptor.
    """

    if not isinstance(timeline, Timeline):
        raise RuntimeError("Not a timeline")

    # build otio
    otio = oTimeline()

    # our timeline has one stack, so does otio.
    # we just need to start porcessing it.
    for i in reversed(timeline.video_tracks):
        __process_obj(i, otio.tracks, oTrack.Kind.Video)
    for i in timeline.audio_tracks:
        __process_obj(i, otio.tracks, oTrack.Kind.Audio)

    write_otio(otio, path, adapter_name)

def timeline_to_otio_string(timeline, adapter_name=None):
    """Create otio from timeline and reutrn as a string

    Args:
        timeline(Timeline): Timeline object

    Kwargs:
        adapter_name(str): Override adaptor.

    Returns:
        otio(str): Otio serialised timeline as string
    """

    if not isinstance(timeline, Timeline):
        raise RuntimeError("Not a timeline")

    # build otio
    otio = oTimeline()

    # our timeline has one stack, so does otio.
    # we just need to start porcessing it.
    for i in reversed(timeline.video_tracks):
        __process_obj(i, otio.tracks, oTrack.Kind.Video)
    for i in timeline.audio_tracks:
        __process_obj(i, otio.tracks, oTrack.Kind.Audio)

    return opentimelineio.adapters.write_to_string(
        otio, adapter_name=adapter_name
    )

def __process_obj(obj, otio, context=oTrack.Kind.Video):
    ar = obj.available_range
    if ar is None:
        ar = obj.trimmed_range
    sr = obj.active_range
    if sr is None:
        sr = obj.trimmed_range


    if isinstance(obj, Stack):
        print("Stack")
        s = oStack()
        s.name = obj.name

        s.availiable_range = TimeRange(RationalTime(ar.frame_start().frames(), ar.rate().fps()), RationalTime(ar.frame_duration().frames(), ar.rate().fps()))
        s.source_range = TimeRange(RationalTime(sr.frame_start().frames(), sr.rate().fps()), RationalTime(sr.frame_duration().frames(), sr.rate().fps()))

        if context == oTrack.Kind.Video:
            for i in reversed(obj.children):
                __process_obj(i, s, context)
        else:
            for i in obj.children:
                __process_obj(i, s, context)
        otio.append(s)

    elif isinstance(obj, Track):
        print("Track")
        t = oTrack()
        t.name = obj.name
        t.kind = oTrack.Kind.Video if obj.is_video else oTrack.Kind.Audio

        t.availiable_range = TimeRange(RationalTime(ar.frame_start().frames(), ar.rate().fps()), RationalTime(ar.frame_duration().frames(), ar.rate().fps()))
        t.source_range = TimeRange(RationalTime(sr.frame_start().frames(), sr.rate().fps()), RationalTime(sr.frame_duration().frames(), sr.rate().fps()))
        for i in obj.children:
            __process_obj(i, t, context)
        otio.append(t)

    elif isinstance(obj, Clip):
        print("Clip")
        c = oClip()
        c.name = obj.name
        # if has media.. add external reference.
        media = obj.media
        if media:
            ext = ExternalReference(str(media.media_source().media_reference.uri()), TimeRange(RationalTime(ar.frame_start().frames(), ar.rate().fps()), RationalTime(ar.frame_duration().frames(), ar.rate().fps())))
            ext.name = media.name
            c.media_reference = ext
        # c.availiable_range = TimeRange(RationalTime(ar.frame_start().frames(), ar.rate().fps()), RationalTime(ar.frame_duration().frames(), ar.rate().fps()))
        c.source_range = TimeRange(RationalTime(sr.frame_start().frames(), sr.rate().fps()), RationalTime(sr.frame_duration().frames(), sr.rate().fps()))
        otio.append(c)

    elif isinstance(obj, Gap):
        print("Gap")
        g = oGap()
        g.name = obj.name

        g.source_range = TimeRange(RationalTime(sr.frame_start().frames(), sr.rate().fps()), RationalTime(sr.frame_duration().frames(), sr.rate().fps()))
        otio.append(g)
