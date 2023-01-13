import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Promise_ResolveWhen_Signal"

    Timer {
        id: timer;
        repeat: true
        interval : 50
    }

    Promise {
        id : promise1
        resolveWhen: timer.onTriggered
    }

    function test_resolvewhen_signal() {
        compare(promise1.isFulfilled,false);
        wait(10);
        compare(promise1.isFulfilled,false);
        timer.start();
        wait(10);
        compare(promise1.isFulfilled,false);
        wait(50);
        compare(promise1.isFulfilled,true);
    }

    QtObject {
        id: customObject

        signal custom(string v1, int v2);
    }

    Promise {
        id: promise2

        resolveWhen: customObject.custom

        property var result: null

        onFulfilled: {
            result = value;
        }
    }

    function test_resolvewhen_signal_with_arguments() {
        compare(promise2.isFulfilled,false);
        customObject.custom("v1",2);
        tick();

        compare(promise2.isFulfilled, true)
        compare(JSON.stringify(promise2.result),
                JSON.stringify({"0": "v1" , "1" : 2}));
    }
}

