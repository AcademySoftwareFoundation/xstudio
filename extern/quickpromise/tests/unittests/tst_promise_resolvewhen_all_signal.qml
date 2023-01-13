import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Promise_ResolveWhen_Signal_All"

    Promise {
        id : promise
        signal triggered1
        signal triggered2
        resolveWhen: Q.all([promise.triggered1,promise.triggered2])
    }

    function test_resolve() {
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.triggered1();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.triggered2();
        compare(promise.isSettled,false);

        tick();
        compare(promise.isSettled,true);
    }
}

