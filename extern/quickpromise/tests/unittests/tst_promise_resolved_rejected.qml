import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "Promise_Resolved_Rejected"

    function test_resolved() {
        var promise = Q.resolved("blue");
        wait(50);
        compare(promise.hasOwnProperty("___promiseSignature___"), true);
        compare(promise.isSettled, true);
        compare(promise.isFulfilled, true);
        compare(promise.isRejected, false);
        compare(promise._result, "blue");
    }

    function test_rejected() {
        var promise = Q.rejected("blue");
        wait(50);
        compare(promise.hasOwnProperty("___promiseSignature___"), true);
        compare(promise.isSettled, true);
        compare(promise.isFulfilled, false);
        compare(promise.isRejected, true);
        compare(promise._result, "blue");
    }


}

