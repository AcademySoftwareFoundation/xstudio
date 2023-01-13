import QtQuick 2.0
import Testable 1.0
import QuickFuture 1.0
import FutureTests 1.0
import QtTest 1.1

TestCase {

    name: "PromiseTests";

    function test_Promise() {
        var called = false;
        var result;

        var promise = Future.promise(Actor.read("a-file-not-existed"));

        promise.then(function(value) {
            called = true;
            result = value
        });

        wait(1000);
        compare(called, true);
        compare(result, "");
    }

}
