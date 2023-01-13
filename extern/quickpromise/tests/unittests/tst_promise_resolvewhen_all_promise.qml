import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Promise_ResolveWhen_All_Promise"

    Component {
        id: promiseCreator;
        Promise {
            id : promise
            signal triggered
            property alias promise1 : promiseItem1
            property alias promise2 : promiseItem2
            resolveWhen: Q.all([promise1,promise2,promise.triggered])
            Promise {
                id: promiseItem1
            }
            Promise {
                id: promiseItem2
            }
        }
    }

    function test_resolve() {
        var promise = promiseCreator.createObject();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.triggered();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.promise1.resolve();
        promise.promise2.resolve();
        compare(promise.isSettled,false);

        tick();
        compare(promise.isSettled,true);
    }
}

