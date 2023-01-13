import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "PromiseJS_Examples"

    function tick() {
        wait(0);
        wait(0);
        wait(0);
    }

    function test_resolve_value() {

        var fulfilled = false;
        var fulfilledResult = null;
        var rejected = false;

        var promise = Q.promise();

        promise.then(function(result) {
            fulfilled = true;
            fulfilledResult = result;
        }, function(reason) {
            rejected = true;
            rejectedReason = reason;
        });

        promise.resolve(3);
        compare(fulfilled, false);
        tick();

        compare(fulfilled, true);
        compare(fulfilledResult, 3);
        compare(rejected, false);
    }

    function test_reject_value() {
        var fulfilled = false;
        var fulfilledResult = null;
        var rejected = false;
        var rejectedReason = null;

        var promise = Q.promise();

        promise.then(function(result) {
            fulfilled = true;
            fulfilledResult = result;
        }, function(reason) {
            rejected = true;
            rejectedReason = reason;
        });

        promise.reject("error");
        compare(fulfilled, false);
        tick();

        compare(fulfilled, false);
        compare(fulfilledResult, null);
        compare(rejected, true);
        compare(rejectedReason, "error");
    }

    Item {
        id: item
        signal customSignal(int value)
    }

    function test_resolve_signal() {
        var fulfilled = false;
        var fulfilledResult = null;
        var rejected = false;
        var rejectedReason = null;

        var promise = Q.promise();

        promise.then(function(result) {
            fulfilled = true;
            fulfilledResult = result;
        }, function(reason) {
            rejected = true;
            rejectedReason = reason;
        });

        promise.resolve(item.customSignal);
        item.customSignal(9);
        compare(fulfilled, false);
        tick();

        compare(fulfilled, true);
        compare(fulfilledResult[0], 9);
        compare(rejected, false);
        compare(rejectedReason, null);

    }


}
