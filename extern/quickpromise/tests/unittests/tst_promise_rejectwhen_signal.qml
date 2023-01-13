import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Promise_RejectWhen_Signal"

    Timer {
        id: timer;
        repeat: true
        interval : 50
    }

    Promise {
        id : promise1
        rejectWhen: timer.onTriggered
    }

    function test_rejectwhen_signal() {
        compare(promise1.isRejected,false);
        wait(10);
        compare(promise1.isRejected,false);
        timer.start();
        wait(10);
        compare(promise1.isRejected,false);

        tryCompare(promise1, 'isRejected', true);
    }

    QtObject {
        id: customObject

        signal custom(string v1, int v2);
    }

    Promise {
        id: promise2

        rejectWhen: customObject.custom

        property var result: null

        onRejected: {
            result = reason;
        }
    }

    function test_rejectwhen_signal_with_arguments() {
        compare(promise2.isRejected,false);
        customObject.custom("v1",2);
        tick();

        compare(promise2.isRejected, true)
        compare(JSON.stringify(promise2.result),
                JSON.stringify({"0": "v1" , "1" : 2}));
    }

}

