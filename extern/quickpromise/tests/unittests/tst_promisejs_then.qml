import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0


TestCase {
    name : "PromiseJS_Then"

    function tick() {
        for (var i = 0 ; i < 4;i++) {
            wait(0);
        }
    }


    function test_promise_then_resolved() {
        var called = false;
        var promise = Q.resolved();
        promise.then(function() {
            called = true;
        });
        tick();

        compare(called, true);

        called = false;

        promise.then(function() {
            called = true;
        });

        compare(called, false);

        tick();

        compare(called, true);
    }


}
