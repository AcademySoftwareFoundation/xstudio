// Author:  Ben Lau (https://github.com/benlau)
import QtQuick 2.0
import QtQml 2.2
import QuickPromise 1.0
import "promise.js" as PromiseJS

QtObject {
    id : promise

    default property alias data: promise.__data
    property list<QtObject> __data: [QtObject{}]

    property bool isFulfilled : false

    property bool isRejected : false

    property bool isSettled : isFulfilled || isRejected

    /// An expression that will trigger resolve() if the value become true or another promise object got resolved.
    property var resolveWhen

    /// An expression that will trigger reject() if the value become true. Don't assign another promise object here.
    property var rejectWhen

    signal fulfilled(var value)
    signal rejected(var reason)
    signal settled(var value)

    property var _promise;

    /// Random signature for type checking
    property var ___promiseQmlSignature71237___

    function setTimeout(callback,interval) {
        QPTimer.setTimeout(callback,interval);
    }

    function then(onFulfilled,onRejected) {
        return _promise.then(onFulfilled,onRejected);
    }

    function resolve(value) {
        if (instanceOfPromise(value)) {
            value._init();
            value = value._promise;
        }

        _promise.resolve(value);
    }

    function reject(reason) {
        _promise.reject(reason);
    }

    /// Combine multiple promises into a single promise.
    function all(promises) {
        return PromiseJS.all(promises);
    }

    /// Combine multiple promises into a single promise. It will wait for all of the promises to either be fulfilled or rejected.
    function allSettled(promises) {
        return PromiseJS.allSettled(promises);
    }

    function instanceOfPromise(object) {
        return typeof object === "object" && object.hasOwnProperty("___promiseQmlSignature71237___");
    }

    function _instanceOfSignal(object) {
        return PromiseJS._instanceOfSignal(object);
    }

    function _init() {
        if (!_promise)
            _promise = PromiseJS.promise();
    }

    onResolveWhenChanged: {
        _init();

        if (resolveWhen === true) {
            resolve(resolveWhen);
        } else if (instanceOfPromise(resolveWhen) ||
                   PromiseJS.instanceOfPromiseJS(resolveWhen) ||
                   _instanceOfSignal(resolveWhen)) {
            resolve(resolveWhen);
        }
    }

    onRejectWhenChanged: {
        _init();
        if (rejectWhen === true) {
            reject(true);
        } else if (_instanceOfSignal(rejectWhen)) {
            reject(rejectWhen);
        }
    }

    Component.onCompleted: {
        _init();

        _promise.then(function(value) {
            isFulfilled = true;
            fulfilled(value);
            settled(value);
        },function(reason) {
            isRejected = true;
            rejected(reason);
            settled(reason);
        });
    }

}

