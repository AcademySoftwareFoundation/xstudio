# SPDX-License-Identifier: Apache-2.0

__doc__ = """
Use OpenTimelineIO module to build a timeline to be subsequently used
to load a timeline in xSTUDIO. 
"""

import argparse

import opentimelineio as otio

def make_otio_timeline(shots_data):
    """
    Build an OTIO with two tracks (one audio, one video). The sources
    in 'source_files' are strung out in the same order with 8 frame 
    handles.
    """

    # build the structure
    tl = otio.schema.Timeline(name="Demo Timeline")

    video_track = otio.schema.Track(name="Video Track")
    tl.tracks.append(video_track)

    duration = 0
    for shot_data in shots_data:
        frame_range = [
            int(shot_data['comp_range'].split('-')[0]),
            int(shot_data['comp_range'].split('-')[1])
        ]
        duration = duration + frame_range[1]-frame_range[0]+1-16

    # we need a url for the video source. We can use our 'fake' file format
    # to take advantage of our procedural image generator so xSTUDIO doesn't
    # error on missing media
    base_track_fname = 'http://fake_media/{0}/{1}/{2}/{2}_{1}_v{3:03d}/{2}_{1}_v{3:03d}.{4}-{5}.fake'.format(shot_data['job'], shot_data['sequence'], 'seq', 1, 1, duration)

    ref = otio.schema.ExternalReference(
        target_url=base_track_fname,
        # available range is the content available for editing
        available_range = otio.opentime.TimeRange(
            start_time=otio.opentime.RationalTime(1, 24),
            duration=otio.opentime.RationalTime(duration, 24)
        )
    )

    clip_frame = 1
    # build the clips
    for shot_data in shots_data:

        frame_range = [
            int(shot_data['comp_range'].split('-')[0]),
            int(shot_data['comp_range'].split('-')[1])
        ]

        # attach the reference to the clip
        cl = otio.schema.Clip(
            name=shot_data['shot'],
            media_reference=ref,

            # the source range represents the range of the media that is being
            # 'cut into' the clip. This is an artificial example, so its just
            # a bit shorter than the available range.
            source_range=otio.opentime.TimeRange(
                start_time=otio.opentime.RationalTime(
                    clip_frame,
                    24
                ),
                duration=otio.opentime.RationalTime(
                    frame_range[1] - frame_range[0] - 16,
                    24
                )
            )
        )
        clip_frame += frame_range[1] - frame_range[0] - 16

        # put the clip into the track
        video_track.append(cl)

    # write the OTIO to string (standard otio json format)
    return otio.adapters.write_to_string(tl)
