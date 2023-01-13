import QtQuick 2.0
import QtTest 1.1

TestCase {

    function waitUntil(callback, timeout) {
        var elapsed = 0;
        var res = true;
        while (!callback()) {
            wait(100);
            elapsed+=100;
            if (timeout !== undefined && elapsed > timeout) {
                res = false;
                break;
            }
        }

        return res;
    }

    function tick() {
        var i = 3;
        while (i--) {
            wait(0);
        }
    }

}
