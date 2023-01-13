#! /usr/bin/env python
# SPDX-License-Identifier: Apache-2.0

"""Currently, a *very* rough demo showing remote control of xstudio with two
way communication to a PySide2 interface"""

from PySide2.QtWidgets import QApplication
from PySide2.QtWidgets import QMainWindow
from PySide2.QtWidgets import QVBoxLayout, QHBoxLayout, QGridLayout
from PySide2.QtWidgets import QPushButton
from PySide2.QtWidgets import QWidget
from PySide2.QtWidgets import QCheckBox
from PySide2.QtWidgets import QSlider
from PySide2.QtWidgets import QLineEdit
from PySide2.QtQuickWidgets import QQuickWidget
from PySide2.QtWidgets import QFileDialog
from PySide2.QtCore import QUrl
from PySide2.QtCore import QTimer
from PySide2.QtCore import Qt
import sys
import time
import subprocess
from xstudio.core import event_atom, add_media_atom, position_atom, duration_frames_atom

from xstudio.connection import Connection
from xstudio.core import CompareMode
from xstudio.core import CompareMode
from xstudio.api.session.media import Media

# Run the bare-bones xstudio viewport, we will connect to it later. You may need
# to modify the path to this binary.
viewer_process = subprocess.Popen(
    ["./bin/glx_minimal_player.bin"]
    )

XSTUDIO = None

class PlayheadUI(QWidget):

    def __init__(self, parent):

        super(PlayheadUI, self).__init__(parent)

        self.layout = QHBoxLayout(self)
        self.frame = QLineEdit(self)
        self.layout.addWidget(self.frame)

        self.slider = QSlider(Qt.Horizontal, self)
        self.slider.valueChanged.connect(self.position_changed)
        self.layout.addWidget(self.slider, 1)
        self.num_frames = QLineEdit(self)
        self.layout.addWidget(self.num_frames)
        self.backend_position = 0

    def set_duration(self, duration_frames):
        self.num_frames.setText(str(duration_frames))
        self.slider.setMaximum(duration_frames)

    def set_position(self, position):
        self.frame.setText(str(position))
        self.backend_position = position
        self.slider.setValue(position)

    def position_changed(self, new_position):

        if self.backend_position != new_position:

            playlist = XSTUDIO.api.session.playlists[0]

            # Ensure we have a playhead
            if not playlist or not playlist.playheads:
                return

            playlist.playheads[0].position = new_position

class PlaylistUI(QWidget):

    def __init__(self, parent):

        super(PlaylistUI, self).__init__(parent)

        self.layout = QVBoxLayout(self)
        self.playlistItems = []

    def media_added(self, media_remote):

        media_item = Media(XSTUDIO, media_remote)
        checkbox = QCheckBox(str(media_item.media_source().media_reference), self)
        checkbox.setCheckState(Qt.Checked)
        self.layout.addWidget(checkbox)
        self.playlistItems.append((checkbox, media_item))
        checkbox.stateChanged.connect(self.onCheckboxClicked)
        self.onCheckboxClicked(0)

    def onCheckboxClicked(self, state):

        selected_items = []
        for (checkbox, media_item) in self.playlistItems:
            if checkbox.checkState() == Qt.Checked:
                selected_items.append(media_item.uuid)

        playlist = XSTUDIO.api.session.playlists[0]

        # Ensure we have a playhead
        if not playlist.playheads:
           return

        playlist.playheads[0].source.set_selection(selected_items)

class XsPysideDemo(QWidget):

    def __init__(self, parent):

        super(XsPysideDemo, self).__init__(parent)

        self.playhead_event_handler = None
        self.playlist_event_handler = None

        self.event_loop_timer = QTimer(self)
        self.event_loop_timer.timeout.connect(self.process_playhead_events)

        layout = QVBoxLayout(self)
        self.playlist_ui = PlaylistUI(self)
        layout.addWidget(self.playlist_ui)
        layout.addStretch(1)

        buttonLayout = QHBoxLayout()
        loadButton = QPushButton("Load Media (.mov)", self)
        loadButton.clicked.connect(self.onLoad)
        buttonLayout.addWidget(loadButton)

        sleepButton = QPushButton("Python Sleep", self)
        sleepButton.clicked.connect(self.onSleep)
        buttonLayout.addWidget(sleepButton)

        startButton = QPushButton("Start Playing", self)
        startButton.clicked.connect(self.onStartPlaying)
        buttonLayout.addWidget(startButton)

        stopButton = QPushButton("Stop Playing", self)
        stopButton.clicked.connect(self.onStopPlaying)
        buttonLayout.addWidget(stopButton)

        layout.addLayout(buttonLayout)

        self.playhead_ui = PlayheadUI(self)
        layout.addWidget(self.playhead_ui)

    def onLoad(self):

        fname = QFileDialog.getOpenFileName(
            self,
            "Get media","","MOV Files (*.mov)"
            )
        if fname != "":
            self.load_media(fname[0])

    def onSleep(self):

        time.sleep(1)

    def setPlaying(self, playing):

        self.open_connection()

        if XSTUDIO.api.session.playlists:
            playlist = XSTUDIO.api.session.playlists[0]
            if playlist.playheads:
                playlist.playheads[0].playing = playing

        self.event_loop_timer.stop()
        if playing:
            self.event_loop_timer.start(20)
        else:
            self.event_loop_timer.start(200)


    def onStartPlaying(self, playing):

        self.setPlaying(True)

    def onStopPlaying(self):

        self.setPlaying(False)

    def makePlayhead(self):

        playlist = XSTUDIO.api.session.playlists[0]

        # Ensure we have a playhead
        if not playlist.playheads:
            playlist.create_playhead()

        # Enter the 'string' compare mode and select all media so that loaded
        # media is strung together
        playlist.playheads[0].compare_mode = "String"

        if not self.playhead_event_handler:
            self.playhead_event_handler = playlist.playheads[0].add_handler(
                self.playhead_events
                )


    def load_media(self, filename):

        self.open_connection()

        # Ensure we have a playlist
        if not XSTUDIO.api.session.playlists:
            XSTUDIO.api.session.create_playlist()
            playlist = XSTUDIO.api.session.playlists[0]

        playlist = XSTUDIO.api.session.playlists[0]

        if not self.playlist_event_handler:
            self.playlist_event_handler = playlist.add_handler(
                self.playlist_events
                )

        # Load the media item
        media = playlist.add_media(filename)

        self.makePlayhead()

    def playhead_events(self, sender, req_id, event):
        if type(event[0]) == position_atom and type(event[1]) == int:
            self.playhead_ui.set_position(event[1])
        elif type(event[0]) == event_atom  and type(event[1]) == duration_frames_atom and type(event[2]) == int:
            self.playhead_ui.set_duration(event[2])

    def playlist_events(self, sender, req_id, event):

        if type(event[0]) == event_atom and type(event[1]) == add_media_atom:
            # 3rd eement of event is a UuidActor of the new media item
            self.playlist_ui.media_added(event[2].actor)


    def process_playhead_events(self):
        XSTUDIO.process_events()

    def open_connection(self):

        global XSTUDIO
        if not XSTUDIO:
            XSTUDIO = Connection(auto_connect=True)
            self.event_loop_timer.start(200)

if __name__=="__main__":

    app = QApplication(sys.argv)
    appWindow = QMainWindow()

    centralWidget = XsPysideDemo(appWindow)
    appWindow.setCentralWidget(centralWidget)
    appWindow.resize(960, 540)
    appWindow.show()

    sys.exit(app.exec_())

    viewer_process.kill()