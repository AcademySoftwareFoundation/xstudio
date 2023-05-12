# SPDX-License-Identifier: Apache-2.0
import pytest
import subprocess
import xstudio
import time
import signal
from xstudio.connection import Connection
from xstudio.core import *

@pytest.fixture(scope="session")
def spawn():
    p = subprocess.Popen(["./bin/xstudio.bin","--session","pythontest","-e","-n","--log-file","xstudio.log"])
    time.sleep(10)
    m = RemoteSessionManager(remote_session_path())
    s = m.find("pythontest")
    XSTUDIO = Connection()
    XSTUDIO.connect_remote(
        s.host(),
        s.port()
    )
    yield XSTUDIO
    p.send_signal(signal.SIGINT)
    time.sleep(1)
    p.terminate()
    time.sleep(1)
    p.kill()
    p.wait()