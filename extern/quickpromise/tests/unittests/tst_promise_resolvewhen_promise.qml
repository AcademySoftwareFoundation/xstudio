import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestSuite {
    name : "Promise_ResolveWhen_Promise"

    Component {
        id: promiseItem
        Promise {
            property alias source : sourcePromise
            resolveWhen : Promise {
                id : sourcePromise
            }
        }
    }

    function test_resolve() {
        var promise = promiseItem.createObject();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.source.resolve();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,true);
        compare(promise.isFulfilled,true);

    }

    function test_reject() {
        var promise = promiseItem.createObject();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,false);

        promise.source.reject();
        compare(promise.isSettled,false);
        tick();
        compare(promise.isSettled,true);
        compare(promise.isRejected,true);
    }

}

