// SPDX-License-Identifier: Apache-2.0
import QtQuick 2.12

import xStudio 1.0


Timer {
    id: timer
    function setTimeout(cb, delayTime) {
        timer.interval = delayTime;
        timer.repeat = false;
        timer.triggered.connect(cb);
        timer.triggered.connect(function release () {
            timer.triggered.disconnect(cb); // This is important
            timer.triggered.disconnect(release); // This is important as well
        });
        timer.start();
    }
}

