import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "Promise_Resolve_Signal"

    Item {
        id: customItem
        signal emitted()
    }

    Timer {
        id: builtInItem
        repeat: false;
        interval: 50
    }

    function test_type() {
        compare(typeof customItem.emitted,"function");
        compare(typeof customItem.emitted.connect,"function");
        compare(typeof customItem.emitted.disconnect,"function");
        compare(typeof customItem.emitted.hasOwnProperty,"function");

        compare(typeof customItem.onEmitted,"object"); // That is the differnet between custom type and built-in type
        compare(typeof customItem.onEmitted.connect,"function");
        compare(typeof customItem.onEmitted.disconnect,"function");
        compare(typeof customItem.onEmitted.hasOwnProperty,"function")

        compare(typeof builtInItem.triggered,"function");
        compare(typeof builtInItem.triggered.connect,"function");
        compare(typeof builtInItem.triggered.disconnect,"function");
        compare(typeof builtInItem.triggered.hasOwnProperty,"function");

        compare(typeof builtInItem.onTriggered,"function");
        compare(typeof builtInItem.onTriggered.connect,"function");
        compare(typeof builtInItem.onTriggered.disconnect,"function");
        compare(typeof builtInItem.onTriggered.hasOwnProperty,"function");
    }

    Promise {
        id : promise
    }

    function test_instanceOfSignal() {
        compare(promise._instanceOfSignal(0),false);
        compare(promise._instanceOfSignal(3),false);
        compare(promise._instanceOfSignal({}),false);
        compare(promise._instanceOfSignal(""),false);

        compare(promise._instanceOfSignal(customItem.emitted),true);
        compare(promise._instanceOfSignal(customItem.onEmitted),true);

        compare(promise._instanceOfSignal(builtInItem.triggered),true);
        compare(promise._instanceOfSignal(builtInItem.onTriggered),true);
    }

    Component {
        id : promiseCreator
        Promise {

        }
    }

    function run(sig,resolve,timeout) {
        var promise = promiseCreator.createObject();
        compare(promise._instanceOfSignal(sig),true);
        compare(promise.isFulfilled,false);
        promise.resolve(sig);
        compare(promise.isFulfilled,false);
        wait(timeout);
        compare(promise.isFulfilled,false);

        resolve();

        wait(timeout);
        compare(promise.isFulfilled,true);
    }

    function test_resolve_signal() {
        run(customItem.emitted,function() {customItem.emitted();},50);
        run(customItem.onEmitted,function() {customItem.emitted();},50);
        run(builtInItem.triggered,function() {builtInItem.start();},100);
        run(builtInItem.onTriggered,function() {builtInItem.start();},100);
    }


}

