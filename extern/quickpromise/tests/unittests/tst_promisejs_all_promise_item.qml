import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "PromiseJS_All_PromiseItem"

    function tick() {
        wait(0);
        wait(0);
        gc();
        wait(0);
    }

    Component {
        id : promiseItem
        Promise {
        }
    }

    function test_single() {
        var source = promiseItem.createObject();
        var promise = Q.all([source]);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "pending");

        source.resolve();
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "fulfilled");
    }

    function test_rejected() {
        var source1 = promiseItem.createObject();
        var source2 = promiseItem.createObject();
        var promise = Q.all([source1,source2]);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "pending");

        source2.reject();
        compare(promise.state , "pending");

        tick();
        compare(promise.state , "rejected");
    }

    function test_already_rejected() {
        var source1 = promiseItem.createObject();
        var source2 = promiseItem.createObject();
        source1.reject();

        var promise = Q.all([source1,source2]);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "rejected");
    }

}

