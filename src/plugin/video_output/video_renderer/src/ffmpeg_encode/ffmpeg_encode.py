#!/bin/env python
# SPDX-License-Identifier: Apache-2.0

from xstudio.connection import Connection
from xstudio.plugin import PluginBase
from xstudio.core import JsonStore, Uuid
from xstudio.core import event_atom, show_atom, actor_from_string
from xstudio.core import HUDElementPosition, ColourTriplet
from xstudio.api.session.media import Media, MediaSource

import io
import os
import sys
import subprocess
import json
import threading
import time
from pathlib import Path

def monitor_output(proc, plugin_actor, job_id, connection):

    line = ""
    for c in iter(lambda: proc.stdout.read(1), b""):
        if c[0] == 10:
            line = line+"\n"
            connection.send(plugin_actor, event_atom(), job_id, line, False)
            line = ""
        elif c[0] == 13:
            line = line+"\n"
            connection.send(plugin_actor, event_atom(), job_id, line, True)
            line = ""
        else:
            line = line+c.decode("utf-8")

    connection.send(plugin_actor, event_atom(), job_id, line, True)

def simple_run(proc, plugin_actor, job_id, connection):
    thread = threading.Thread(target=monitor_output, args=(proc, plugin_actor, Uuid(job_id), connection))
    thread.start()
    proc.wait()
    thread.join()
    connection.send(plugin_actor, event_atom(), Uuid(job_id), proc.returncode)

class FFMpegEncoderPlugin(PluginBase):

    """Plugin instance to manage ffmpeg encode tasks"""

    def __init__(
            self,
            connection,
            ):
        """Create a plugin 

        Args:
            name(str): Name of Viewport Layout plugin
        """
        PluginBase.__init__(
            self,
            connection,
            name="FFMpegEncoderPlugin"
            )

        self.thread = None
        self.jobs = {}

    def stop_ffmpeg_encode(self,
        job_id):

        if job_id in self.jobs:
            if not self.jobs[job_id].returncode:
                self.jobs[job_id].kill()

    def start_ffmpeg_encode(self,
        output_file,
        image_fifo_name,
        width,
        height,
        frame_rate,
        audio_sample_rate,
        video_encoder_parameters,
        is_16_bit,
        video_output_plugin,
        job_id,
        audio_fifo_name = None,
        audio_encoder_parameters = None
        ):

        audio_in_args = []
        if audio_fifo_name:
            # xstudio hardcodes samples format to 16bit per channel, 2x channels
            # setereo.
            audio_in_args = [
                '-f', 's16le',
                '-ar', str(audio_sample_rate),
                '-ac', '2',
                '-i', audio_fifo_name
                ]

        appname = 'ffmpeg'
        import platform
        if platform.system() == 'Darwin':
            appname = str(Path(sys.executable).parent / 'ffmpeg')
        elif platform.system() == 'Windows':
            appname = str(Path(sys.executable).parent / 'ffmpeg.exe')

        cmd = [appname, '-s:v', '{}x{}'.format(width, height),
            '-pixel_format', 'rgba64le' if is_16_bit else 'rgba', # supplied raw frames are rgba 8 bit per chan
            '-r', str(frame_rate), # need to specify the input frame rate (same as output)
            '-i', image_fifo_name] + \
            audio_in_args + \
            ['-r', str(frame_rate),
            '-y', # force overwrite (xSTUDIO confirms if user wants to overwrite)
            '-vf', 'colorspace=bt709:iall=bt601-6-625:fast=1' # ffmpeg assumes intput is bt601. We want bt709 output.
            ] + video_encoder_parameters

        if audio_encoder_parameters:
            cmd += audio_encoder_parameters 

        cmd += [output_file]

        # output the ffmpeg command to our log
        line = "FFMpeg started with the following command line arguments:\n\n{}\n\n".format(' '.join(cmd))
        self.connection.send(video_output_plugin, event_atom(), Uuid(job_id), line, True)

        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        self.thread = threading.Thread(target=simple_run, args=(proc, video_output_plugin, job_id, self.connection))
        self.thread.start()
        self.jobs[job_id] = proc

        j = JsonStore()
        j.parse_string(json.dumps({"ENCODING STARTED": "chonk"}))

        self.connection.send(video_output_plugin, event_atom(), Uuid(job_id), "Process monitoring thread started.\n\n", True)

        return j


# This method is required by xSTUDIO
def create_plugin_instance(connection):
    return FFMpegEncoderPlugin(connection)