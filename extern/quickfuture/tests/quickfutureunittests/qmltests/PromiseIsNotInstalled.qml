import QtQuick 2.0
import QuickFuture 1.0

Item {

    function test() {
        var called = false;
        var result;

        var promise = Future.promise(Actor.read("a-file-not-existed"));

        promise.then(function(value) {
            called = true;
            result = value
        });
    }

}
