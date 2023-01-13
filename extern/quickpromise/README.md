<a href="https://promisesaplus.com/">
    <img src="https://promisesaplus.com/assets/logo-small.png" alt="Promises/A+ logo"
         title="Promises/A+ 1.0 compliant" align="right" />
</a>

Quick Promise - QML Promise Library
===================================

[![Build Status](https://travis-ci.org/benlau/quickpromise.svg?branch=master)](https://travis-ci.org/benlau/quickpromise)

The Promise object is widely used for deferred and asynchronous computation in Javascript Application. "Quick Promise” is a library that provides Promise object in a QML way. It comes with a Promise component with signal and property. It could be resolved via a binary expression,  another promise object, then trigger your callback by QueuedConnection.

Moreover, it also provides Promise as a Javascript object that don’t need to declare in QML way. The API is fully compliant with  [Promises/A+](https://promisesaplus.com/) specification with all the test cases passed and therefore it just works like many other Promise solutions for Javascript application.

*Example:*

```qml
import QtQuick 2.0
import QuickPromise 1.0

Item {
    id: item
    opacity: 0

    Image {
        id: image
        asynchronous: true
        source: "https://lh3.googleusercontent.com/_yimBU8uLUTqQQ9gl5vODqDufdZ_cJEiKnx6O6uNkX6K9lT63MReAWiEonkAVatiPxvWQu7GDs8=s640-h400-e365-rw";
    }

    Timer {
        id: timer
        interval: 3000
    }

    Promise {
        // Resolve when the image is ready
        resolveWhen: image.status === Image.Ready

        // Reject when time out
        rejectWhen: timer.triggered

        onFulfilled:  {
            // If the timer is reached, the image will not be shown even it is ready.
            item.opacity = 1;
        }
    }
}
```

The code above demonstrated how Promise component could be used in asynchronous workflow for QML application.
The resolveWhen property accepts a boolean expression, another Promise object and signal. 
Once the result of expression becomes truth, 
it will trigger the “onFulfilled” slot via QueuedConnection. *The slot will be executed for once only*.

Remarks: The QML Promise component is not fully compliant with Promises/A+ specification.

Related articles:

1. [JavaScript Promises with ArcGIS Runtime SDK for Qt | GeoNet](https://geonet.esri.com/community/developers/native-app-developers/arcgis-runtime-sdk-for-qt/blog/2016/07/05/javascript-promises-with-arcgis-runtime-sdk-for-qt)

Feature List
------------

1. Promise in a QML way
 1. Trigger resolve()/reject() via binary expression, signal from resloveWhen / rejectWhen property
 2. isFulfilled / isRejected / isSettled properties for data binding.
 3. fulfulled , rejected , settled signals
2. Promise in a Javascript way
 1. Unlike QML component, it don’t need to declare before use it.
 2. It is fully compatible with [Promises/A+](https://promisesaplus.com/) specification. It is easy to get started.
3. Extra API
 1. Q.setTimeout() - An implementation of setTimeout() function for QML.
 2. all()/allSettled()  - Create a promise object from an array of promises

Installation Instruction (using qpm)
==============================

For user who are already using qpm from [qpm.io](https://qpm.io)

 1) Run `qpm install`

```sh
$ qpm install com.github.benlau.quickpromise
```

 2) Include vendor/vendor.pri in your .pro file

You may skip this step if you are already using qpm.

```qmake
include(vendor/vendor.pri)
```

 3) Add "qrc://" to your QML import path

```qml
engine.addImportPath("qrc:///"); // QQmlEngine
```

 4) Add import statement in your QML file

```qml
import QuickPromise 1.0
```

Installation Instruction (without qpm)
========================

 1) Clone this repository or download release to a folder within your source tree.

 2) Add this line to your profile file(.pro):

    include(quickpromise/quickpromise.pri) # You should modify the path by yourself

 3) Add "qrc://" to your QML import path

```cpp
engine.addImportPath("qrc:///"); // QQmlEngine
```

 4) Add import statement in your QML file

```qml
import QuickPromise 1.0
```

Minimal Installation
====================

Since v1.0.8, the C++ setTimeout function is deprecated and therefore it becomes a pure JavaScript library. You may copy the content of QuickPromise folder to your application.

In case you only need the JavaScript's promise. You only need to copy two files to your source code. They are

```
QuickPromise/promise.js
QuickPromise/PromiseTimer.qml
```

Usage

```qml
import "./promise.js" as Q

// ...

var promise = new Q.Promise(function(resolve, reject) {
    // ...
});
```

Reference: [Q.promise()](#qpromise)

What is Promise and how to use it?
==========================

Please refer to this site for core concept: [Promises](https://www.promisejs.org/)

Promise QML Component
=====================

```qml
Promise {
    property bool isFulfilled

    property bool isRejected

    property bool isSettled: isFulfilled || isRejected

    /// An expression that will trigger resolve() if the value become true or another promise object got resolved.
    property var resolveWhen

    /// An expression that will trigger reject() if the value become true. Don't assign another promise object here.
    property var rejectWhen

    signal fulfilled(var value)

    signal rejected(var reason)

    signal settled(var value)

    function setTimeout(func, timeout) { ... }

    function then(onFulfilled, onRejected) { ... }

    function resolve(value) { ... }

    function reject(reason) { ... }

    function all(promises) { ... }

    function allSettled(promises) { ... }
}
```

**isFullfilled**

It is true if resolve() has been called on that promise object

**isRejected**

It is true if reject() has been called on that promise object

**isSettled**

It is true if either of isFullfilled or isRejected has been set.

**resolveWhen**

resolveWhen property is an alternative method to call resolve() in a QML way. You may bind a binary expression, another promise, signal to the "resolveWhen" property. That will trigger the resolve() depend on its type and value.

**resolveWhen: binary expression**

Once the expression become true, it will trigger resolve(true).

**resolveWhen: signal**

Listen the signal, once it is triggered, it will call resolve().

_Example:_

```qml
Timer {
    id: timer
    repeat: true
    interval: 50
}

Promise {
    id: promise
    resolveWhen: timer.onTriggered
}
```

**resolveWhen: promise**

It is equivalent to resolve(promise). It will adopt the state from the input promise object.

Let x be the input promise.

If x is fulfilled, call resolve().

If x is rejected, call reject().

If x is not settled, listen its state change. Once it is fulfilled/rejected, repeat the above steps.

**rejectWhen**

_rejectWhen_ property is an alternative method to call reject() in a QML way. You may bind a binary expression, signal to this property. It may trigger the reject() depend on its type and value.

Remarks: _rejectWhen_ can not take promise as parameter.

**rejectWhen: binary expression**

Once the expression become true, it will trigger reject(true).

**rejectWhen: signal**

Listen the signal, once it is triggered, it will call reject().


Q.promise()
===========

Q.promise(executor) is the creator function of Promise object in a Javascript way. You won't need to declare a QML component before use it. As it is fully compliant with Promise/A+ specification, it is very easy to get started. But it don't support property binding (resolveWhen , rejectWhen) like the Promise QML component.

However, you may still pass a signal object to resolve()/reject(). In this case, the promise will not change its state until the signal is triggered. If multiple signal call are made, the first signal call takes precedence, and any further calls are ignored.

But it don't support to resolve by the result of Qt.binding(). It will just throw exception. In this case, you should use a QML Promise and pass it to resolveWhen property.


Remarks: `Q.promise(executor)` is equivalent to `new Q.Promise(executor)`.

*API*

```js
Q.Promise(executor)
Q.Promise.prototype.then = function(onFulfilled,onRejected) { ... }
Q.Promise.resolve = function(result) { ... }
Q.Promise.reject = function(reason) { ... }
Q.Promise.all = function(promises) { }
Q.Promise.allSettled = function(promises) { }
```

Instruction of using then/resolve/reject: [Promise](https://developer.mozilla.org/zh-TW/docs/Web/JavaScript/Reference/Global_Objects/Promise)

[Example Code](https://github.com/benlau/quickpromise/blob/master/tests/unittests/tst_promisejs_examples.qml)

Extra API
---------

**Q.setTimeout(func, milliseconds)**

The setTimeout() method will wait the specified number of milliseconds, and then execute the specified function. If the milliseconds is equal to zero, the behaviour will be same as triggering a signal via QueuedConnection.


**Q.all(promises)**

Given an array of promise / signal , it will create a promise object that will be fulfilled once all the input promises are fulfilled. And it will be rejected if any one of the input promises is rejected.


**Q.allSettled(promises)**

Given an array of promise / signal , it will create a promise object that will be fulfilled once all the input promises are fulfilled. And it will be rejected if any one of the input promises is rejected. It won't change the state until all the input promises are settled.

Advanced Usage
==============

1. Resolve by multiple signals.
-------------------------------

```qml
Promise {
    resolveWhen: Q.all([timer.triggered, loader.loaded]);
}
```

2. Resolve by signal and binary expression
------------------------------------------

```qml
Promise {
    resolveWhen: Q.all([timer.triggered, promise2]);

    Promise {
        id: promise2
        resolveWhen: image.status === Image.Ready
    }
}
```

Related Projects
=================

**Libaries**

 1. [benlau/quickcross](https://github.com/benlau/quickcross) - QML Cross Platform Utility Library
 1. [benlau/qsyncable](https://github.com/benlau/qsyncable) - Synchronize data between models
 1. [benlau/testable](https://github.com/benlau/testable) - QML Unit Test Utilities
 1. [benlau/quickflux](https://github.com/benlau/quickflux) - Message Dispatcher / Queue for Qt/QML
 1. [benlau/biginteger](https://github.com/benlau/biginteger) - QML BigInteger library
 1. [benlau/qtci](https://github.com/benlau/qtci) -  A set of scripts to install Qt in Linux command line environment (e.g travis)
 1. [benlau/quickfuture](https://github.com/benlau/quickfuture) - Using QFuture in QML
 1. [benlau/fontawesome.pri](https://github.com/benlau/fontawesome.pri) - Using FontAwesome in QML
 1. [benlau/quickandroid](https://github.com/benlau/quickandroid) - QML Material Design Component and Support Library for Android


**Tools**

 1. [SparkQML](https://github.com/benlau/sparkqml) - QML Document Viewer for State and Transition Preview
