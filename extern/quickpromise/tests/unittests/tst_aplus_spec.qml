import QtQuick 2.0
import QtTest 1.0
import QuickPromise 1.0


TestCase {
    name : "APlusSpec"

    function tick() {
        for (var i = 0 ; i < 4; i++) {
            wait(0);
        }
    }

    // The implementation is based on Promises/A+
    // https://promisesaplus.com

    function test_2_2_7_1_resolve() {
        var promise1 = Q.promise();
        var promise2 = promise1.then(function() {
            return "accepted"
        },function() {
        });

        compare(promise2 !== undefined,true);
        var resolvedValue;
        promise2.then(function(value) {
            resolvedValue = value;
        });

        promise1.resolve();
        compare(resolvedValue,undefined);
        tick();

        compare(resolvedValue,"accepted");
    }

    function test_2_2_7_1_reject() {
        var promise1 = Q.promise();
        var promise2 = promise1.then(function() {
        },function() {
            return "rejected"
        });

        compare(promise1._onFulfilled.length,1);

        compare(promise2 !== undefined,true);
        var resolvedValue;
        promise2.then(function(value) {
            resolvedValue = value;
        });

        promise1.reject();
        compare(resolvedValue,undefined);
        tick();
        compare(resolvedValue,"rejected");
    }

    function test_2_2_7_2() {
        var promise1,promise2;
        promise1 = Q.promise();
        promise2 = promise1.then(function() {
            throw new Error("Expected Error for Testing");
        },function() {
            throw new Error("Exception");
        });

        promise1.resolve("x");
        tick();

        compare(promise2.state , "rejected");
        compare(String(promise2._result) , "Error: Expected Error for Testing");
    }

    function test_2_2_7_3() {
        var promise1 = Q.promise();
        var promise2 = promise1.then(undefined,function() {
        });

        compare(promise2 !== undefined,true);
        var resolvedValue;
        promise2.then(function(value) {
            resolvedValue = value;
        });

        promise1.resolve("2_2_7_3");

        tick();
        compare(resolvedValue,"2_2_7_3");
    }

    function test_2_2_7_4() {
        var promise1 = Q.promise();
        var promise2 = promise1.then(function() {
        },undefined);

        compare(promise1._onFulfilled.length,1);

        compare(promise2 !== undefined,true);
        var resolvedValue;
        promise2.then(undefined,function(value) {
            resolvedValue = value;
        });

        promise1.reject("2_2_7_4");

        tick();

        compare(resolvedValue,"2_2_7_4");
    }

    function test_2_3_1() {
        var promise1 = Q.promise();
        var promise2 = promise1.then(function() {} , function() {});

        promise1.resolve(promise1);
        compare(promise1.state , "pending");
        tick();
        compare(promise1.state , "rejected");
    }

    function test_2_3_2_1() {
        var promiseSrc = Q.promise();
        var promise1 = Q.promise();
        var promise2 = promise1.then(function() {} , function() {});

        promise1.resolve(promiseSrc);
        compare(promise1.state, "pending");

        promiseSrc.resolve("x");
        tick();
        compare(promise1.state, "fulfilled");

        promiseSrc.reject("x");
        compare(promise1.state, "fulfilled");
    }

    function test_2_3_2_2() {
        var promiseSrc = Q.promise();
        var promise1 = Q.promise();
        var resolvedValue;
        var promise2 = promise1.then(function(value) {
            resolvedValue = value;
        } , function() {});

        promiseSrc.resolve("x");

        promise1.resolve(promiseSrc);
        tick();
        compare(promise1.state, "fulfilled");
        compare(resolvedValue, "x");
    }

    function test_2_3_2_3() {
        var promiseSrc = Q.promise();
        var promise1 = Q.promise();
        var rejectedValue;
        var promise2 = promise1.then(function(value) {
        } , function(value) {
            rejectedValue = value;

        });

        promiseSrc.reject("x");

        promise1.resolve(promiseSrc);
        compare(promise1.state, "pending");
        tick();
        compare(promise1.state, "rejected");
        compare(rejectedValue, "x");
    }

    function test_q_all() {
        // Condition 1: Successful condition
        var promise1 = Q.promise();
        var promise2 = Q.promise();
        var promise3 = Q.promise();

        var allPromise = Q.all([promise1,promise2,promise3]);
        compare(allPromise.state ,"pending");
        promise2.resolve("b"); // resolve out of order
        tick();
        compare(allPromise.state ,"pending");
        promise1.resolve("a");
        tick();
        compare(allPromise.state ,"pending");
        promise3.resolve("c");
        tick();
        compare(allPromise.state ,"fulfilled");
        // results should keep original order regardless of resolution order
        compare(allPromise._result, ["a", "b", "c"]);

        // Condition 2. Reject one of the promise
        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();

        allPromise = Q.all([promise1,promise2,promise3]);
        compare(allPromise.state ,"pending");
        promise2.reject();
        compare(allPromise.state ,"pending");
        tick();
        compare(allPromise.state ,"rejected");

        // Condition 3: Insert rejected promise
        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();
        promise2.reject();

        allPromise = Q.all([promise1,promise2,promise3]);
        compare(allPromise.state ,"pending");
        tick();
        compare(allPromise.state ,"rejected");

        // Condition 4: All the promise is fulfilled already

        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();

        promise2.resolve('y');
        promise1.resolve('x');
        promise3.resolve('z');

        allPromise = Q.all([promise1,promise2,promise3]);
        tick();
        compare(allPromise.state ,"fulfilled");
        compare(allPromise._result, ["x", "y", "z"]);
    }

    function test_q_allSettled() {
        // Condition 1: Successful condition
        var promise1 = Q.promise();
        var promise2 = Q.promise();
        var promise3 = Q.promise();

        var allPromise = Q.allSettled([promise1,promise2,promise3]);
        compare(allPromise.state ,"pending");
        promise3.resolve("c"); // resolve out of order
        tick();
        compare(allPromise.state ,"pending");
        promise1.resolve("a");
        tick();
        compare(allPromise.state ,"pending");
        promise2.resolve("b");
        tick();
        compare(allPromise.state ,"fulfilled");
        // results should keep original order regardless of resolution order
        compare(allPromise._result, ["a", "b", "c"]);

        // Condition 2. Reject one of the promise
        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();

        allPromise = Q.allSettled([promise1,promise2,promise3]);
        compare(allPromise.state ,"pending");

        promise2.reject();
        tick();
        compare(allPromise.state ,"pending");
        promise3.resolve();
        tick();
        compare(allPromise.state ,"pending");
        promise1.resolve();
        tick();
        compare(allPromise.state ,"rejected");

        // Condition 3: Insert rejected promise
        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();
        promise2.reject();

        allPromise = Q.allSettled([promise1,promise2,promise3]);
        tick();
        compare(allPromise.state ,"pending");
        promise1.resolve();
        tick();
        compare(allPromise.state ,"pending");
        promise3.resolve();
        tick();
        compare(allPromise.state ,"rejected");

        // Condition 4: All the promise is fulfilled already

        promise1 = Q.promise();
        promise2 = Q.promise();
        promise3 = Q.promise();

        promise2.resolve('y');
        promise1.resolve('x');
        promise3.resolve('z');

        allPromise = Q.allSettled([promise1,promise2,promise3]);
        tick();
        compare(allPromise.state ,"fulfilled");
        compare(allPromise._result, ["x", "y", "z"]);
    }

    Component {
        id : promiseCreator1
        Promise {
            property int x : 0
            resolveWhen: x > 0
            rejectWhen: x < 0
        }
    }

    Component {
        id : emitterCreator
        Item {
            signal emitted()
        }
    }

    function test_promise() {
        var onFulFilled = false;
        var onRejected = false;
        var value;

        // Condition 1: Resolve the promise

        var promise = promiseCreator1.createObject();
        promise.onFulfilled.connect(function(v) {
            onFulFilled = true;
            value = v;
        });

        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        promise.resolve("x");

        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        tick();

        compare(promise.isFulfilled,true);
        compare(promise.isRejected,false);
        compare(promise.isSettled,true);

        compare(onFulFilled, true);
        compare(value,"x");

        // Condition 2: Reject the promise

        promise = promiseCreator1.createObject();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        promise.onRejected.connect(function() {
            onRejected = true;
        });

        promise.reject();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);
        compare(onRejected, false);

        tick();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,true);
        compare(promise.isSettled,true);

        compare(onRejected, true);


        // Condition 3: resvoleWhen : expression => true

        promise = promiseCreator1.createObject();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        promise.x = 1;

        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        tick();

        // resolveWhen should be done on next tick is
        // necessary for promise created via Component
        // and fulfill immediately

        compare(promise.isFulfilled,true);
        compare(promise.isRejected,false);
        compare(promise.isSettled,true);

        // Condition 4: rejectWhen: expression => true

        promise = promiseCreator1.createObject();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        promise.x = -1;
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,false);
        compare(promise.isSettled,false);

        tick();
        compare(promise.isFulfilled,false);
        compare(promise.isRejected,true);
        compare(promise.isSettled,true);

        // Condition 5: all()

        var promise1 = Q.promise();
        var promise2 = Q.promise();
        var promise3 = Q.promise();

        promise = promiseCreator1.createObject();
        promise.resolve(promise.all([promise1,promise2,promise3]));

        promise1.resolve();
        tick();
        compare(promise.isSettled,false);
        promise2.resolve();
        tick();
        compare(promise.isSettled,false);
        promise3.resolve();
        tick();

        compare(promise.isSettled,true);

        // Condition 6: resolveWhen = promise
        promise1 = promiseCreator1.createObject();
        promise2 = promiseCreator1.createObject();

        compare(promise1.isFulfilled,false);
        promise1.resolveWhen = promise2;
        compare(promise1.isFulfilled,false);
        promise2.resolve();

        compare(promise1.isFulfilled,false);

        wait(0);wait(0);

        compare(promise1.isFulfilled,true);

        // Condition 7: resolveWhen = Q.promise
        promise1 = promiseCreator1.createObject();
        promise2 = Q.promise();

        compare(promise1.isFulfilled,false);
        promise1.resolveWhen = promise2;

        compare(promise1.isFulfilled,false);

        promise2.resolve();

        compare(promise1.isFulfilled,false);

        wait(0);wait(0);

        compare(promise1.isFulfilled,true);

        /* Condition 8: rejectWhen = promise */
        promise1 = promiseCreator1.createObject();
        promise2 = promiseCreator1.createObject();

        compare(promise1.isRejected,false);
        promise1.rejectWhen = promise2;
        compare(promise1.isRejected,false);
        promise2.resolve();

        // rejectWhen ignore promise object
        compare(promise1.isRejected,false);
    }

    function test_promise_then() {
        var fulfilled = false;
        var rejected = false;
        var promise1 = promiseCreator1.createObject();
        var promise2 = promise1.then(function() {
            fulfilled = true;
        },function() {
            rejected = true;
        });

        compare(promise1.instanceOfPromise(promise2),false);

        compare(Q.instanceOfPromise(promise2),true);
        // It is not a Promise Component since it can not create QML recursively

        promise1.resolve("x");
        tick();
        compare(fulfilled,true);
        // @TODO. Support async callback
    }

    function test_promise_resolve_signal() {
        var emitter = emitterCreator.createObject();

        var fulfilled = false;
        var rejected = false;
        var promise = promiseCreator1.createObject();

        promise.onFulfilled.connect(function(v) {
            fulfilled = true;
        });

        promise.onRejected.connect(function() {
           rejected = true;
        });

        promise.resolve(emitter.onEmitted);
        compare(fulfilled,false);
        compare(rejected,false);

        wait(0);
        compare(fulfilled,false);
        compare(rejected,false);

        wait(0);

        emitter.emitted();
        compare(fulfilled,false);
        compare(rejected,false);
        wait(0);wait(0);

        compare(fulfilled,true);
        compare(rejected,false);
    }
}


