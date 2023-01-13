import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0


TestCase {
    name : "PromiseJS_Executor"

    function tick() {
        wait(0);
        wait(0);
        wait(0);
    }

    function test_promise_executor_resolve() {
        var promise = Q.promise(function(fulfill, reject) {
            fulfill(123);
        });
        compare(promise.isFulfilled, false);
        compare(promise.isRejected, false);

        tick();

        compare(promise.isFulfilled, true);
        compare(promise.isRejected, false);
        compare(promise._result, 123);

    }

    function test_promise_executor_reject() {
        var promise = Q.promise(function(fulfill, reject) {
            reject(456);
        });
        compare(promise.isFulfilled, false);
        compare(promise.isRejected, false);

        tick();

        compare(promise.isFulfilled, false);
        compare(promise.isRejected, true);
        compare(promise._result, 456);

    }


    function test_promise_executor_exception() {
        var promise = Q.promise(function(fulfill, reject) {
            throw("error");
        });
        compare(promise.isFulfilled, false);
        compare(promise.isRejected, false);

        tick();

        compare(promise.isFulfilled, false);
        compare(promise.isRejected, true);
        compare(promise._result, "error");

    }

}


