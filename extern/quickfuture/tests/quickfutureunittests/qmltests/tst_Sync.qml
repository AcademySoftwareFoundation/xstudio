import QtQuick 2.0
import QtTest 1.1
import Testable 1.0
import QuickFuture 1.0
import FutureTests 1.0

CustomTestCase {
    name: "Sync";

    QtObject {
        id: target1
        property var isRunning
        property var isFinished
    }

    function test_sync() {
        compare(target1.isRunning, undefined);
        compare(target1.isFinished, undefined);

        var future = Actor.dummy();
        Future.sync(future,"isRunning", target1, "isRunning");
        Future.sync(future,"isFinished", target1); // last argument is optional
        compare(target1.isFinished, false);
        compare(target1.isRunning, true);

        waitUntil(function() {
            return Future.isFinished(future);
        });

        tick();

        compare(target1.isRunning, false);
        compare(target1.isFinished, true);
    }


}
