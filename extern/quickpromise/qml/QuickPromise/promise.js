// Quick Promise Script
// Author:  Ben Lau (https://github.com/benlau)

.pragma library

var _nextId = 0;
var _timers = {}

var _timerCreator = Qt.createComponent("PromiseTimer.qml");

function clearTimeout(timerId) {
    if (!_timers.hasOwnProperty(timerId)) {
        return;
    }
    var timer = _timers[timerId];
    timer.stop();
    timer.destroy();
    delete _timers[timerId];
}

function setTimeout(callback, timeout) {

    var tid = ++_nextId;

    var obj = _timerCreator.createObject(null, {interval: timeout});
    obj.triggered.connect(function() {
        try {
            callback();
        } catch (e) {
            console.warn(e);
        }
        obj.destroy();
        delete _timers[tid];
    });
    obj.running = true;
    _timers[tid] = obj;
    return tid;
}

/* JavaScript implementation of promise object
 */

function QPromise(executor) {
    this.state = "pending";

    this._onFulfilled = [];
    this._onRejected = [];

    // The value of resolved promise or the reason to be rejected.
    this._result = undefined;

    this.___promiseSignature___ = true;

    this.isSettled = false;
    this.isFulfilled = false;
    this.isRejected = false;

    if (typeof executor === "function") {
        var promise = this;

        try {
            executor(function() {
                promise.resolve.apply(promise, arguments);
            }, function() {
                promise.reject.apply(promise, arguments);
            });

        } catch(e) {
            promise.reject(e);
        }
    }
}

function instanceOfPromiseJS(object) {
    return typeof object === "object" &&
           typeof object.hasOwnProperty === "function" &&
            object.hasOwnProperty("___promiseSignature___");
}

function instanceOfPromiseItem(object) {
    return typeof object === "object" &&
           typeof object.hasOwnProperty === "function" &&
            object.hasOwnProperty("___promiseQmlSignature71237___");
}

function instanceOfPromise(object) {
    return instanceOfPromiseJS(object) || instanceOfPromiseItem(object)
}

function _instanceOfSignal(object) {
    return (typeof object === "object" ||
            typeof object === "function") &&
            typeof object.hasOwnProperty === "function" &&
            typeof object.connect === "function" &&
            typeof object.disconnect === "function";
}

QPromise.prototype.then = function(onFulfilled,onRejected) {
    var thenPromise = new QPromise();

    this._onFulfilled.push(function(value) {
        if (typeof onFulfilled === "function" ) {
            try {
                var x = onFulfilled(value);
                thenPromise.resolve(x);
            } catch (e) {
                console.error(e);
                console.trace();
                thenPromise.reject(e);
            }

        } else {
            // 2.2.7.3
            thenPromise.resolve(value);
        }
    });

    this._onRejected.push(function(reason) {
        if (typeof onRejected === "function") {
            try {
                var x = onRejected(reason);
                thenPromise.resolve(x);
            } catch (e) {
                console.error(e);
                console.trace();
                thenPromise.reject(e);
            }
        } else {
            // 2.2.7.4
            thenPromise.reject(reason);
        }
    });

    if (this.state !== "pending") {
        var promise = this;
        setTimeout(function() {
            promise._executeThen();
        },0);
    }

    return thenPromise;
}


QPromise.prototype.resolve = function(value) {
    if (this.state !== "pending") {
        return;
    }

    if (this === value) { // 2.3.1
        this.reject(new TypeError("Promise.resolve(value) : The value can not be same as the promise object."));
        return;
    }

    var promise = this;

    if (value && _instanceOfSignal(value)) {
        try {
            var newPromise = new QPromise();
            value.connect(function() {
                newPromise.resolve(arguments);
            });
            value = newPromise;
        } catch (e) {
            console.error(e);
            throw new Error("promise.resolve(object): Failed to call object.connect(). Are you passing the result of Qt.binding()? Please use QML Promise and pass it to resolveWhen property.");
        }
    }

    if (value &&
        instanceOfPromise(value)) {

        if (value.state === "pending") {
            value.then(function(x) {
                promise._resolveInTick(x);
            },function(x) {
                promise.reject(x);
            });

            return;
        } else if (value.state === "fulfilled") {
            this._resolveInTick(value._result);
            return;
        } else if (value.state === "rejected") {
            this.reject(value._result);
            return;
        }
    }

    if (value !== null && (typeof value === "object" || typeof value === "function")) {
        var then;
        try {
            then = value.then;
        } catch (e) {
            console.error(e);
            console.trace();
            promise.reject(e);
            return;
        }

        if (typeof then === "function") {
            try {
                var called = false;
                then.call(value,function(y) {
                    if (called)
                        return;
                    called = true;
                    promise.resolve(y);
                },function (y) {
                    if (called)
                        return;

                    called = true;
                    promise.reject(y);
                });
            } catch (e) {
                console.error(e);
                console.trace();
                if (!called) {
                    promise.reject(e);
                }
            }
            return;
        }
    }

    this._resolveInTick(value);
}

QPromise.prototype._resolveInTick = function(value) {
    var promise = this;

    setTimeout(function() {
        if (promise.state !== "pending")
            return;

        promise._resolveUnsafe(value);
    },0);
}

/*
     Resolve without value type checking
 */

QPromise.prototype._resolveUnsafe = function(value) {
    this._result = value;
    this._setState("fulfilled");
    this._executeThen();
}

QPromise.prototype.reject = function(reason) {
    if (this.state !== "pending")
        return;

    var promise = this;

    if (reason && _instanceOfSignal(reason)) {
        reason.connect(function() {
            promise.reject(arguments);
        });
        return;
    }

    setTimeout(function() {
        if (promise.state !== "pending")
            return;

        promise._rejectUnsafe(reason);
    },0);
}

QPromise.prototype._rejectUnsafe = function(reason) {
    this._result = reason;
    this._setState("rejected");
    this._executeThen();
}

QPromise.prototype._emit = function(arr,value) {
    var res = value;
    for (var i = 0 ; i < arr.length;i++) {
        var func = arr[i];
        if (typeof func !== "function")
            continue;
        var t = func(value);
        if (t) // It is non-defined behaviour in A+
            res = t;
    }
    return res;
}

/// Execute the registered "then" function
QPromise.prototype._executeThen = function() {
    var arr = [];

    if (this.state === "rejected") {
        arr = this._onRejected;
    } else if (this.state === "fulfilled") {
        arr = this._onFulfilled;
    }

    this._emit(arr, this._result);
    this._onFulfilled = [];
    this._onRejected = [];
}

QPromise.prototype._setState = function(state) {
    if (state === "fulfilled") {
        this.isFulfilled = true;
        this.isSettled = true;
    } else if (state === "rejected"){
        this.isRejected = true;
        this.isSettled = true;
    }
    this.state = state;
}

function promise(executor) {
    return new QPromise(executor);
}

function resolve(result) {
    var p = promise();
    p.resolve(result);
    return p;
}

function resolved(result) {
    return resolve(result);
}

function reject(reason) {
    var p = promise();
    p.reject(reason);
    return p;
}

function rejected(reason) {
    return reject(reason);
}

/* Combinator */

// Combinate a list of promises into a single promise object.
function Combinator(promises,allSettled) {
    this._combined = new QPromise();
    this._allSettled = allSettled === undefined ? false  : allSettled;
    this._promises = [];
    this._results = [];
    this._count = 0;

    // Any promise rejected?
    this._rejected = false;
    this._rejectReason = undefined;

    this.add(promises);

    this._settle();
}

Combinator.prototype.add = function(promises) {
    if (instanceOfPromise(promises)) {
        this._addPromise(promises);
    } else {
        this._addPromises(promises);
    }
    return this.combined;
}

Combinator.prototype._addPromises = function(promises) {
    for (var i = 0 ; i < promises.length;i++) {
        this._addPromise(promises[i]);
    }
}

Combinator.prototype._addPromise = function(promise) {
    if (_instanceOfSignal(promise)) {
        var delegate = new QPromise();
        delegate.resolve(promise);
        this._addCheckedPromise(delegate);
    } else if (promise.isSettled) {
        if (promise.isRejected) {
            this._reject(promise._result);
        }
        // calling `resolve` after adding resolved promises is covered
        // by the call to `_settle` in constructor
    } else {
        this._addCheckedPromise(promise);
    }
}

Combinator.prototype._addCheckedPromise = function(promise) {
    var combinator = this;

    var promiseIndex = this._promises.length;
    combinator._promises.push(promise);
    combinator._results.push(undefined);
    combinator._count++;

    promise.then(function(value) {
        combinator._count--;
        combinator._results[promiseIndex] = value;
        combinator._settle();
    },function(reason) {
        combinator._count--;
        combinator._reject(reason);
    });
}

Combinator.prototype._reject = function(reason) {
    if (this._allSettled) {
        this._rejected = true;
        this._rejectReason = reason;
    } else {
        this._combined.reject(reason);
    }
}

/// Check it match the settle condition, it will call resolve / reject on the combined promise.
Combinator.prototype._settle = function() {
    if (this._count !== 0) {
        return;
    }

    if (this._rejected) {
        this._combined.reject(this._rejectedReason);
    } else {
        this._combined.resolve(this._results);
    }
}

function combinator(promises,allSettled) {
    return new Combinator(promises,allSettled);
}

function all(promises) {
    var combinator = new Combinator(promises,false);

    return combinator._combined;
}

/// Combine multiple promises into one. It will wait for all of the promises to either be fulfilled or rejected.
function allSettled(promises) {
    var combinator = new Combinator(promises,true);

    return combinator._combined;
}

QPromise.all = all;
QPromise.resolve = resolve;
QPromise.reject = reject;
