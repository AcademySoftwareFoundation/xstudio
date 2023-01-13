import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "PromiseJS_All_Signal"

    function tick() {
        wait(0);
        wait(0);
        wait(0);
    }

    Item {
        id: item1
        signal triggered(var value);
    }

    Item {
        id: item2
        signal triggered;
    }

    function test_all_single_signal() {
        var promise = Q.all([item1.triggered]);
        compare(promise.state , "pending");
        item1.triggered(3);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "fulfilled");

        compare(promise._result, [{"0" : 3}]);

        /* Test Q.allSettled */

        promise = Q.allSettled([Q.resolved(item1.triggered)]);
        compare(promise.state , "pending");
        item1.triggered(6);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "fulfilled");

        compare(promise._result, [{"0" : 6}]);
    }

    function test_all_multiple_signal() {
        var promise = Q.all([item1.triggered,item2.triggered]);

        compare(promise.state , "pending");
        item2.triggered();
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "pending");

        item1.triggered(9);
        compare(promise.state , "pending");
        tick();
        compare(promise.state , "fulfilled");

        compare(promise._result, [{"0" : 9}, {}]);

    }
}

