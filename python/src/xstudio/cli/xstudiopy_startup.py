# SPDX-License-Identifier: Apache-2.0
"""Wrapper."""
import sys
import os
import xstudio
from xstudio.connection import Connection
from xstudio.core import *

__DEBUG = False

if os.environ.get("XSTUDIOPY_DEBUG", "False") == "True":
    __DEBUG = True

if os.environ.get("XSTUDIOPY_HOST", None):
    XSTUDIO = Connection(
        debug=__DEBUG
    )
    XSTUDIO.connect_remote(
        os.environ.get("XSTUDIOPY_HOST", "localhost"),
        int(os.environ.get("XSTUDIOPY_PORT", "45500"))
    )
elif os.environ.get("XSTUDIOPY_SESSION", None):
    m = RemoteSessionManager(remote_session_path())
    s = m.find(os.environ.get("XSTUDIOPY_SESSION"))

    XSTUDIO = Connection(
        debug=__DEBUG
    )
    if s is not None:
        XSTUDIO.connect_remote(
            s.host(),
            s.port()
        )
    else:
        print("Remote session not found")
else:
    m = RemoteSessionManager(remote_session_path())
    s = m.first_api()

    XSTUDIO = Connection(
        debug=__DEBUG
    )
    if s is not None:
        XSTUDIO.connect_remote(
            s.host(),
            s.port()
        )
    else:
        print("No remote session found")

if os.environ.get("XSTUDIOPY_EXECUTE", "") == "":
    print ("XSTUDIO", XSTUDIO)
