#! /usr/bin/env bash
'''':
# Hack to enter the xstudio bob world and set-up environment before running
# this script again as python
# if root isn't set
if [ -z "$XSTUDIO_ROOT" ]
then
	# use bob world path
	if [ ! -z "$BOB_WORLD_SLOT_dneg_xstudio" ]
	then
		export XSTUDIO_ROOT=$BOB_WORLD_SLOT_dneg_xstudio/share/xstudio
		export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$XSTUDIO_ROOT/lib
        exec python "${BASH_SOURCE[0]}" "$@"
	else
        exec bob-world -t xstudio-pyside -d /builds/targets/gen-2025-05-20T16:03:54.596603 -- "${BASH_SOURCE[0]}" "$@"
	fi
fi
'''

import sys
import os

if 'BOB_WORLD_SLOT_dneg_xstudio' in os.environ:
    os.environ['XSTUDIO_ROOT'] = os.environ['BOB_WORLD_SLOT_dneg_xstudio'] + "/share/xstudio"
    os.environ['LD_LIBRARY_PATH'] = os.environ['XSTUDIO_ROOT'] + "/lib"

print (os.environ['LD_LIBRARY_PATH'])

from PySide6.QtWidgets import QApplication
from PySide6.QtWidgets import QMainWindow
from PySide6.QtWidgets import QVBoxLayout, QHBoxLayout
from PySide6.QtWidgets import QPushButton
from PySide6.QtWidgets import QWidget
from PySide6.QtWidgets import QCheckBox
from PySide6.QtWidgets import QSlider
from PySide6.QtWidgets import QLineEdit
from PySide6.QtWidgets import QFileDialog
from PySide6.QtCore import QTimer
from PySide6.QtCore import Qt
from xstudio.gui import PySideQmlViewport, XstudioPyApp, PlainViewport
import time
from xstudio.core import event_atom, add_media_atom, position_atom, duration_frames_atom

from xstudio.connection import Connection
from xstudio.api.session.media import Media
from xstudio.api.app import Viewport

XSTUDIO = None

class PlayheadUI(QWidget):

    def __init__(self, parent, _playhead):

        super(PlayheadUI, self).__init__(parent)

        self.layout = QHBoxLayout(self)
        self.frame = QLineEdit(self)
        self.layout.addWidget(self.frame)
        self.playhead = _playhead

        self.slider = QSlider(Qt.Horizontal, self)
        self.slider.valueChanged.connect(self.position_changed)
        self.layout.addWidget(self.slider, 1)
        self.num_frames = QLineEdit(self)
        self.layout.addWidget(self.num_frames)
        self.backend_position = 0

    def set_duration(self, duration_frames):
        self.num_frames.setText(str(duration_frames))
        self.slider.setMinimum(0)
        self.slider.setMaximum(duration_frames)

    def set_position(self, position):
        self.frame.setText(str(position))
        self.backend_position = position
        self.slider.setValue(position)

    def position_changed(self, new_position):

        if self.backend_position != new_position and self.slider.isSliderDown():
            self.playhead.position = new_position


class PlaylistUI(QWidget):

    def __init__(self, parent, _playlist):

        super(PlaylistUI, self).__init__(parent)

        self.playlist = _playlist
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

        self.playlist.playhead_selection.set_selection(selected_items)

class XsPysideDemo(QWidget):

    def __init__(self, parent):

        super(XsPysideDemo, self).__init__(parent)

        # The xSTUDIO viewport needs a unique ID for the window that it is 
        # embedded in. This assists the handling of keystrokes, so we know whether
        # the user hit the 'play' hotkey, for example, in this window or another
        # window
        window_id = str(parent)

        self.viewport = PlainViewport(
            self, 
            window_id
            )

        self.playlist = None
        self.playhead = None
        self.playhead_event_handler = None
        self.playlist_event_handler = None

        self.makePlaylist()

        layout = QVBoxLayout(self)

        layout.addWidget(self.viewport, 1)

        self.playlist_ui = PlaylistUI(self, self.playlist)
        layout.addWidget(self.playlist_ui)

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

        self.playhead_ui = PlayheadUI(self, self.playhead)
        layout.addWidget(self.playhead_ui)

    def onLoad(self):

        self.setPlaying(False)

        fname = QFileDialog.getOpenFileName(
            self,
            "Get media","","MOV Files (*.mov)"
            )
        if fname != "":
            self.load_media(fname[0])

    def onSleep(self):

        time.sleep(1)

    def setPlaying(self, playing):

        self.playhead.playing = playing

    def onStartPlaying(self, playing):

        self.setPlaying(True)

    def onStopPlaying(self):

        self.setPlaying(False)

    def makePlaylist(self):

        if not XSTUDIO.api.session.playlists:
            XSTUDIO.api.session.create_playlist()
            self.playlist = XSTUDIO.api.session.playlists[0]
            self.playhead = self.playlist.playhead

            # here we put the playhead into 'String' compare mode
            self.playhead.get_attribute("Compare").set_value("String")

            # this call is crucial. in xSTUDIO every playlist, subset, contact
            # sheet and timeline has its own 'playhead' that controls playback
            # and delivers video and audio.
            # The xSTUDIO viewports have to be 'attached' to a playhead to 
            # recieve the video frames and display them. By default viewports
            # will automaticall attach to the global active playhead, but this
            # must be set via the API. Thus, the UI can decide which playlist
            # or timeline etc. is being viewed/played.
            # (QuickView windows are an exception, where they ignore updates
            # to the global active playhead but use their own internal 
            # playhead.)
            XSTUDIO.api.app.set_global_playhead(
                self.playhead
                )
            
        if not self.playlist_event_handler:
            self.playlist_event_handler = self.playlist.add_event_callback(
                self.playlist_events
                )

        if not self.playhead_event_handler:
            self.playhead_event_handler = self.playhead.add_event_callback(
                self.playhead_events
                )


    def load_media(self, filename):

        # Load the media item
        self.playlist.add_media(filename)

    def playhead_events(self, event):

        if len(event) >= 3 and type(event[1]) == position_atom and type(event[2]) == int:
            self.playhead_ui.set_position(event[2])
        elif len(event) >= 3 and (type(event[0]) == event_atom and
            type(event[1]) == duration_frames_atom and
            type(event[2])) == int:
            self.playhead_ui.set_duration(event[2])

    def playlist_events(self, event):

        if type(event[0]) == event_atom and type(event[1]) == add_media_atom:
            # 3rd eement of event is a UuidActorVector of the new media items
            for media in event[2]:
                self.playlist_ui.media_added(media.actor)

if __name__=="__main__":

    app = QApplication(sys.argv)

    # this object is required to manage xstudio's core components
    xstudio_py_app = XstudioPyApp()

    # here we connect to the xstudio backend. XSTUDIO serves as the API entry
    # point
    XSTUDIO = Connection(auto_connect=True)

    appWindow = QMainWindow()
    centralWidget = XsPysideDemo(appWindow)
    appWindow.setCentralWidget(centralWidget)
    appWindow.resize(960, 540)
    appWindow.show()

    sys.exit(app.exec_())