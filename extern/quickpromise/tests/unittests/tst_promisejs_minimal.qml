import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0

TestCase {
    name : "PromiseJS_Minimal"

    function tick() {
        wait(0);
        wait(0);
        wait(0);
    }

    function test_constructor() {
        var callbackResult = {}

        var promise = Q.promise(function(resolve, reject) {
           resolve("ready");
        });

        promise.then(function(result) {
            callbackResult.result = result;
        });

        tick();

        compare(callbackResult.result, "ready");
    }

    function test_resolve() {
        var promise = Q.resolved("done");
        var callbackResult = {}
        promise.then(function(result) {
            callbackResult.result = result;
        });

        tick();

        compare(callbackResult.result, "done");
    }


}
